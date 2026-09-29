#include "ZoneGrid.h"
#include "../SampleDrop.h"
#include "../Widgets.h"

namespace sis
{
namespace
{
    constexpr double flashSeconds = 0.9;
}

ZoneGrid::ZoneGrid (StudioContext& c) : ctx (c), model (c.model), ui (c.ui)
{
    model.addChangeListener (this);
    ui.addChangeListener (this);
}

ZoneGrid::~ZoneGrid()
{
    model.removeChangeListener (this);
    ui.removeChangeListener (this);
}

juce::Rectangle<float> ZoneGrid::getInnerArea() const
{
    return getLocalBounds().toFloat().reduced (1.0f);
}

juce::Rectangle<float> ZoneGrid::getZoneBounds (const Zone& z) const
{
    const auto area = getInnerArea();
    constexpr float span = (float) InstrumentModel::numNotes;

    const float left = (float) (z.lowNote - InstrumentModel::lowestNote) / span;
    const float width = (float) (z.highNote - z.lowNote + 1) / span;
    const float top = ui.velocityLayers ? (float) (127 - z.highVelocity) / 127.0f : 0.0f;
    const float height = ui.velocityLayers ? (float) (z.highVelocity - z.lowVelocity) / 127.0f : 1.0f;

    return { area.getX() + left * area.getWidth(), area.getY() + top * area.getHeight(),
             width * area.getWidth(), height * area.getHeight() };
}

int ZoneGrid::noteAt (float x) const
{
    const auto area = getInnerArea();
    const float share = (x - area.getX()) / juce::jmax (1.0f, area.getWidth());
    return juce::jlimit (InstrumentModel::lowestNote, InstrumentModel::highestNote,
                         InstrumentModel::lowestNote + (int) std::floor (share * (float) InstrumentModel::numNotes));
}

int ZoneGrid::velocityAt (float y) const
{
    if (! ui.velocityLayers)
        return 100;

    const auto area = getInnerArea();
    const float share = (y - area.getY()) / juce::jmax (1.0f, area.getHeight());
    return juce::jlimit (0, 127, 127 - juce::roundToInt (share * 127.0f));
}

const Zone* ZoneGrid::zoneAt (juce::Point<float> p) const
{
    // Zuletzt gezeichnete Zone liegt oben
    for (auto it = model.zones.rbegin(); it != model.zones.rend(); ++it)
        if (getZoneBounds (*it).contains (p))
            return &*it;
    return nullptr;
}

void ZoneGrid::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto area = getInnerArea();

    g.setColour (colours::surface);
    g.fillRect (bounds);

    // Oktav-Trennlinien
    g.setColour (colours::divider);
    for (int octave = 1; octave < InstrumentModel::numNotes / 12; ++octave)
    {
        const float x = area.getX() + area.getWidth() * (float) (octave * 12) / (float) InstrumentModel::numNotes;
        g.fillRect (juce::Rectangle<float> (std::floor (x), area.getY(), 1.0f, area.getHeight()));
    }

    // Gestrichelte Mittellinie bei Velocity 64
    const float dashes[] = { 3.0f, 3.0f };
    const float midY = std::floor (area.getCentreY());
    g.drawDashedLine ({ area.getX(), midY, area.getRight(), midY }, dashes, 2, 1.0f);

    // Zonen
    {
        const juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (area.toNearestInt());

        const auto nameFont = sansFont (11.5f, Weight::medium);
        const auto infoFont = monoFont (10.0f);

        for (const auto& z : model.zones)
        {
            const bool selected = z.id == model.selectedZoneId;
            const auto r = getZoneBounds (z);

            g.setColour (selected ? colours::accentSoft : colours::zoneIdle);
            g.fillRect (r);
            g.setColour (selected ? colours::accent : colours::line);
            g.drawRect (r, 1.0f);
            if (selected)
                g.drawRect (r.reduced (1.0f), 1.0f);   // Innenring

            const juce::Graphics::ScopedSaveState zoneClip (g);
            g.reduceClipRegion (r.toNearestInt());

            const int numSamples = z.numSamples();
            auto text = r.reduced (9.0f, 7.0f);

            /* Marke mit der Zahl der Samples und unten eine kleine Wellenform: daran sieht
               man im Mapping, dass ein hereingezogenes Sample angekommen ist. */
            if (numSamples > 0)
            {
                if (r.getWidth() >= 34.0f)
                {
                    const auto badge = juce::Rectangle<float> (r.getRight() - 22.0f, r.getY() + 6.0f, 16.0f, 16.0f);
                    g.setColour (colours::accent);
                    g.fillEllipse (badge);
                    g.setColour (colours::white);
                    g.setFont (monoFont (9.5f, Weight::semibold));
                    g.drawText (juce::String (numSamples), badge.toNearestInt(), juce::Justification::centred, false);
                    text.removeFromRight (18.0f);
                }

                if (r.getHeight() >= 48.0f)
                {
                    for (int i = 0; i < (int) z.tracks.size(); ++i)
                    {
                        if (! z.tracks[(size_t) i].hasClip())
                            continue;

                        const auto peaks = model.waveformFor (z.tracks[(size_t) i], i);
                        auto wave = r.reduced (8.0f, 0.0f);
                        wave = wave.removeFromBottom (juce::jmin (22.0f, r.getHeight() * 0.3f)).withTrimmedBottom (5.0f);
                        const int bars = juce::jlimit (4, 60, juce::roundToInt (wave.getWidth() / 3.0f));
                        draw::waveBars (g, wave, peaks, bars, 1.0f, 0.06f, z.colour.withAlpha (0.75f));
                        break;
                    }
                }
            }

            g.setColour (colours::text);
            g.setFont (nameFont);
            g.drawText (z.name, text.removeFromTop (16.0f), juce::Justification::centredLeft, true);

            // Zusatzzeile nur bei ausreichender Breite (≥ 17 % der Achse)
            const float widthShare = (float) (z.highNote - z.lowNote + 1) / (float) InstrumentModel::numNotes;
            if (widthShare >= 0.17f)
            {
                text.removeFromTop (2.0f);
                g.setColour (colours::textSecondary);
                g.setFont (infoFont);
                g.drawText (noteName (z.lowNote) + "–"_u + noteName (z.highNote) + " · "_u
                                + juce::String (z.numSamples()) + " Smp",
                            text.removeFromTop (14.0f), juce::Justification::centredLeft, true);
            }
        }
    }

    // Ziel eines gezogenen Samples: die Zone darunter, sonst eine neue um die Taste herum
    if (dragging)
    {
        if (const auto* target = model.findZone (dropZoneId))
        {
            const auto r = getZoneBounds (*target);
            g.setColour (colours::accentSoft.withAlpha (0.55f));
            g.fillRect (r);
            g.setColour (colours::accent);
            g.drawRect (r, 2.0f);
        }
        else
        {
            const int note = noteAt (dropPosition.x);
            const float keyWidth = area.getWidth() / (float) InstrumentModel::numNotes;
            const float x = area.getX() + (float) (note - InstrumentModel::lowestNote) * keyWidth;

            g.setColour (colours::accent.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (x, area.getY(), keyWidth, area.getHeight()));

            const auto label = "Neue Zone · "_u + noteName (note);
            const auto font = monoFont (10.0f);
            const float w = textWidth (font, label) + 12.0f;
            auto box = juce::Rectangle<float> (w, 18.0f).withPosition (x + keyWidth + 4.0f, dropPosition.y - 22.0f);
            box = box.constrainedWithin (area);

            g.setColour (colours::accent);
            g.fillRect (box);
            g.setColour (colours::white);
            g.setFont (font);
            g.drawText (label, box.toNearestInt(), juce::Justification::centred, false);
        }
    }

    // Kurzes Aufleuchten der Zone, die gerade ein Sample bekommen hat
    if (const auto* flashed = model.findZone (flashingZoneId))
    {
        const double t = (juce::Time::getMillisecondCounterHiRes() - flashStart) / (flashSeconds * 1000.0);

        if (t >= 0.0 && t < 1.0)
        {
            g.setColour (colours::accent.withAlpha ((float) (1.0 - t)));
            g.drawRect (getZoneBounds (*flashed).expanded (1.0f + 5.0f * (float) t), 2.0f);
        }
    }

    g.setColour (colours::line);
    g.drawRect (bounds, 1.0f);
}

bool ZoneGrid::isInterestedInDragSource (const SourceDetails& details)
{
    return sampleDrop::indexOf (details) >= 0;
}

void ZoneGrid::itemDragMove (const SourceDetails& details)
{
    dragging = true;
    dropPosition = details.localPosition.toFloat();

    const auto* zone = zoneAt (dropPosition);
    dropZoneId = zone != nullptr ? zone->id : juce::String();
    repaint();
}

void ZoneGrid::itemDragExit (const SourceDetails&)
{
    dragging = false;
    dropZoneId.clear();
    repaint();
}

void ZoneGrid::itemDropped (const SourceDetails& details)
{
    dragging = false;
    dropZoneId.clear();

    const auto position = details.localPosition.toFloat();
    const int index = sampleDrop::indexOf (details);

    const auto countClips = [this]
    {
        int clips = 0;
        for (const auto& z : model.zones)
            for (const auto& t : z.tracks)
                clips += t.hasClip() ? 1 : 0;
        return clips;
    };

    const int clipsBefore = countClips();

    if (auto* zone = const_cast<Zone*> (zoneAt (position)))
        sampleDrop::intoZone (ctx, *zone, index);
    else
        sampleDrop::ontoNote (ctx, noteAt (position.x), velocityAt (position.y), index);

    // Aufleuchten nur, wenn wirklich etwas angekommen ist – in der dann gewählten Zone
    if (const auto* selected = countClips() > clipsBefore ? model.getSelectedZone() : nullptr)
    {
        flashingZoneId = selected->id;
        flashStart = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
    }

    repaint();
}

void ZoneGrid::timerCallback()
{
    if (juce::Time::getMillisecondCounterHiRes() - flashStart > flashSeconds * 1000.0)
    {
        flashingZoneId.clear();
        stopTimer();
    }

    repaint();
}

void ZoneGrid::mouseDown (const juce::MouseEvent& e)
{
    if (const auto* z = zoneAt (e.position))
        model.selectZone (z->id);
}

void ZoneGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (const auto* z = zoneAt (e.position))
    {
        model.selectZone (z->id);
        if (onOpenZone != nullptr)
            onOpenZone (z->id);
    }
}

void ZoneGrid::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}
} // namespace sis
