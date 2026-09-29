#include "TrackArea.h"
#include "../../Model/ClipGeometry.h"
#include "../../PluginProcessor.h"
#include "../SampleDrop.h"
#include "../Widgets.h"

namespace sis
{
namespace
{
    constexpr int clipInset = 6;        // Abstand Clip zu Spurrand
    constexpr int clipHeaderHeight = 15;
    constexpr int edgeGrab = 7;
    constexpr int fadeHandleSize = 11;
    constexpr int fadeHandleTop = 19;
    constexpr int dropStripHeight = 44; // Platz unter der letzten Spur: dort entsteht eine neue
}

TrackArea::TrackArea (StudioContext& c) : ctx (c)
{
    setWantsKeyboardFocus (true);
    ctx.model.addChangeListener (this);
    ctx.ui.addChangeListener (this);
}

TrackArea::~TrackArea()
{
    ctx.model.removeChangeListener (this);
    ctx.ui.removeChangeListener (this);
}

Zone* TrackArea::getZone() const
{
    return ctx.model.getSelectedZone();
}

int TrackArea::getIdealHeight() const
{
    const auto* zone = getZone();
    return zone != nullptr ? (int) zone->tracks.size() * (rowHeight + 1) + dropStripHeight : 0;
}

double TrackArea::axisLength (const Zone* zone)
{
    if (zone == nullptr)
        return 1.0;

    // Auf volle Sekunden aufrunden und eine Sekunde Luft lassen, damit man hinter den
    // letzten Clip noch etwas legen kann
    const double seconds = std::ceil (zone->contentEnd() * InstrumentModel::timelineSeconds) + 1.0;
    return juce::jlimit (1.0, maxAxis, seconds / InstrumentModel::timelineSeconds);
}

float TrackArea::laneWidth() const
{
    return (float) juce::jmax (1, getWidth() - headerWidth);
}

float TrackArea::timeToX (double time) const
{
    return (float) headerWidth + (float) (time - ctx.ui.timelineStart) * laneWidth();
}

double TrackArea::xToTime (float x) const
{
    return ctx.ui.timelineStart + (double) ((x - (float) headerWidth) / laneWidth());
}

juce::Rectangle<int> TrackArea::getRowBounds (int index) const
{
    return { 0, index * (rowHeight + 1), getWidth(), rowHeight };
}

juce::Rectangle<int> TrackArea::getLaneBounds (int index) const
{
    return getRowBounds (index).withTrimmedLeft (headerWidth);
}

juce::Rectangle<float> TrackArea::getClipBounds (int index) const
{
    const auto* zone = getZone();
    if (zone == nullptr || ! juce::isPositiveAndBelow (index, (int) zone->tracks.size()))
        return {};

    const auto& track = zone->tracks[(size_t) index];

    if (! track.hasClip())
        return {};

    const auto lane = getLaneBounds (index).toFloat();
    const auto length = juce::jmax (0.005, track.clipLength());

    return { timeToX (track.offset), lane.getY() + (float) clipInset,
             (float) length * laneWidth(), lane.getHeight() - 2.0f * (float) clipInset };
}

juce::Rectangle<int> TrackArea::getMuteBounds (int index) const
{
    auto head = getRowBounds (index).removeFromLeft (headerWidth).reduced (9, 8);
    return { head.getX(), head.getBottom() - 18, 21, 18 };
}

juce::Rectangle<int> TrackArea::getSoloBounds (int index) const
{
    return getMuteBounds (index).translated (21 + 5, 0);
}

int TrackArea::rowAt (juce::Point<int> p) const
{
    const auto* zone = getZone();
    if (zone == nullptr)
        return -1;

    const int row = p.y / (rowHeight + 1);
    return juce::isPositiveAndBelow (row, (int) zone->tracks.size()) ? row : -1;
}

double TrackArea::positionToTime (int x) const
{
    return juce::jmax (0.0, xToTime ((float) x));
}

bool TrackArea::isStretching (const juce::MouseEvent& e) const
{
    const bool shift = e.mods.isShiftDown();
    return (ctx.ui.edgeMode == EdgeMode::stretch) != shift;
}

TrackArea::Drag TrackArea::dragModeAt (int trackIndex, juce::Point<int> p) const
{
    const auto clip = getClipBounds (trackIndex);
    if (clip.isEmpty() || p.x < headerWidth)
        return Drag::none;

    const auto* zone = getZone();
    const auto& track = zone->tracks[(size_t) trackIndex];
    const auto row = getRowBounds (trackIndex);
    const float y = (float) p.y;
    const float x = (float) p.x;

    // Fade-Ecken oben
    const float handleTop = (float) (row.getY() + fadeHandleTop);
    if (y >= handleTop && y <= handleTop + (float) fadeHandleSize)
    {
        const float fadeInX = clip.getX() + (float) track.fadeIn * clip.getWidth();
        const float fadeOutX = clip.getRight() - (float) track.fadeOut * clip.getWidth();

        if (std::abs (x - fadeInX) <= fadeHandleSize * 0.5f + 1.0f)
            return Drag::fadeIn;
        if (std::abs (x - fadeOutX) <= fadeHandleSize * 0.5f + 1.0f)
            return Drag::fadeOut;
    }

    if (y < clip.getY() || y > clip.getBottom())
        return Drag::none;

    if (x >= clip.getX() && x <= clip.getX() + edgeGrab)
        return Drag::leftEdge;
    if (x <= clip.getRight() && x >= clip.getRight() - edgeGrab)
        return Drag::rightEdge;
    if (clip.contains (x, y))
        return Drag::move;

    return Drag::none;
}

//==============================================================================
void TrackArea::paint (juce::Graphics& g)
{
    g.fillAll (colours::workspace);

    auto* zone = getZone();
    if (zone == nullptr || zone->tracks.empty())
    {
        if (dropActive)
        {
            g.setColour (colours::accentSoft.withAlpha (0.6f));
            g.fillRect (getLocalBounds().withHeight (rowHeight));
        }

        g.setColour (colours::textTertiary);
        g.setFont (sansFont (12.0f));
        g.drawText (zone == nullptr ? "Keine Zone gewählt · im Mapping eine Zone oder ein Kit-Teil anklicken"_u
                                    : "Keine Spuren · ein Sample aus der Liste hierher ziehen oder „Spur hinzufügen“"_u,
                    getLocalBounds().withHeight (60), juce::Justification::centred, true);
        return;
    }

    const bool anySolo = std::any_of (zone->tracks.begin(), zone->tracks.end(),
                                      [] (const Track& t) { return t.solo; });
    const float visibleEnd = (float) getWidth();

    for (int i = 0; i < (int) zone->tracks.size(); ++i)
    {
        const auto& track = zone->tracks[(size_t) i];
        const bool selected = i == zone->selectedTrack;
        const bool audible = ! track.mute && (! anySolo || track.solo);

        const auto row = getRowBounds (i);
        g.setColour (selected ? colours::lowerZone : colours::workspace);
        g.fillRect (row);
        g.setColour (colours::keyLine);
        g.fillRect (row.getX(), row.getBottom(), row.getWidth(), 1);

        // Spur: alles, was mitrollt, wird auf die Spur beschnitten – der Kopf bleibt stehen
        const auto lane = getLaneBounds (i);
        {
            const juce::Graphics::ScopedSaveState laneState (g);
            g.reduceClipRegion (lane);

            // Rasterlinien je Sekunde
            g.setColour (colours::lineFine);
            const double start = ctx.ui.timelineStart;
            for (int s = (int) std::ceil (start * InstrumentModel::timelineSeconds); ; ++s)
            {
                const float x = timeToX (s / InstrumentModel::timelineSeconds);
                if (x > visibleEnd)
                    break;
                if (s > 0)
                    g.fillRect (juce::Rectangle<float> (std::round (x), (float) lane.getY(), 1.0f, (float) lane.getHeight()));
            }

            const auto clip = getClipBounds (i);

            if (clip.isEmpty())
            {
                g.setColour (colours::textTertiary);
                g.setFont (monoFont (10.0f));
                g.drawText ("Sample aus der Liste hierher ziehen", lane.reduced (12, 0),
                            juce::Justification::centredLeft, true);
            }
            else
            {
                {
                    const juce::Graphics::ScopedSaveState clipState (g);
                    g.reduceClipRegion (clip.toNearestInt());
                    const float alpha = audible ? 1.0f : 0.3f;

                    g.setColour (track.softColour.withMultipliedAlpha (alpha));
                    g.fillRect (clip);

                    // Kopfstreifen mit Clipname und Stretch-Anzeige; der Name bleibt lesbar,
                    // auch wenn der Clip-Anfang links aus dem Bild gerollt ist
                    auto header = clip.withHeight ((float) clipHeaderHeight);
                    g.setColour (track.colour.withMultipliedAlpha (alpha));
                    g.fillRect (header);

                    auto headerText = header.withLeft (juce::jmax (header.getX(), (float) lane.getX())).reduced (5.0f, 0.0f);
                    headerText.setRight (juce::jmin (headerText.getRight(), visibleEnd - 5.0f));
                    const auto badge = juce::String (track.stretch, 2) + juce::String::fromUTF8 ("×");
                    const float badgeWidth = textWidth (monoFont (10.0f), badge) + 6.0f;
                    g.setColour (colours::white.withMultipliedAlpha (alpha));
                    g.setFont (monoFont (10.0f));
                    g.drawText (badge, headerText.removeFromRight (badgeWidth).toNearestInt(), juce::Justification::centredRight, false);
                    g.drawText (track.clip, headerText.toNearestInt(), juce::Justification::centredLeft, true);

                    // Wellenform des sichtbaren Ausschnitts
                    const auto peaks = ctx.model.waveformFor (track, i);
                    const int total = (int) peaks.size();
                    const int from = juce::jlimit (0, total - 1, (int) (track.trimStart * (total - 1)));
                    const int to = juce::jlimit (from + 1, total - 1, (int) (track.trimEnd * (total - 1)));
                    std::vector<float> slice (peaks.begin() + from, peaks.begin() + to + 1);
                    if (track.reverse)
                        std::reverse (slice.begin(), slice.end());

                    auto waveArea = clip.withTrimmedTop ((float) clipHeaderHeight).reduced (0.0f, 4.0f);
                    const int bars = juce::jlimit (1, (int) slice.size(), juce::roundToInt (clip.getWidth() / 3.0f));
                    draw::waveBars (g, waveArea, slice, bars, 1.0f, 0.04f, track.colour.withMultipliedAlpha (alpha));

                    // Fade-Flächen
                    const auto fadeArea = clip.withTrimmedTop ((float) clipHeaderHeight);
                    g.setColour (colours::surface.withAlpha (0.82f * alpha));
                    if (track.fadeIn > 0.0)
                    {
                        juce::Path p;
                        const float w = (float) track.fadeIn * clip.getWidth();
                        p.addTriangle (fadeArea.getX(), fadeArea.getY(), fadeArea.getX() + w, fadeArea.getY(),
                                       fadeArea.getX(), fadeArea.getBottom());
                        g.fillPath (p);
                    }
                    if (track.fadeOut > 0.0)
                    {
                        juce::Path p;
                        const float w = (float) track.fadeOut * clip.getWidth();
                        p.addTriangle (fadeArea.getRight() - w, fadeArea.getY(), fadeArea.getRight(), fadeArea.getY(),
                                       fadeArea.getRight(), fadeArea.getBottom());
                        g.fillPath (p);
                    }
                }

                g.setColour (selected ? track.colour : track.colour.withMultipliedAlpha (0.85f));
                g.drawRect (clip, 1.0f);
                if (selected)
                    g.drawRect (clip.reduced (1.0f), 1.0f);

                // Griffe für Fade-in und Fade-out
                const float handleY = (float) (row.getY() + fadeHandleTop);
                for (const bool isFadeIn : { true, false })
                {
                    const float x = isFadeIn ? clip.getX() + (float) track.fadeIn * clip.getWidth()
                                             : clip.getRight() - (float) track.fadeOut * clip.getWidth();
                    const juce::Rectangle<float> handle (x - fadeHandleSize * 0.5f, handleY,
                                                         (float) fadeHandleSize, (float) fadeHandleSize);
                    g.setColour (track.colour);
                    g.fillRect (handle);
                    g.setColour (colours::surface);
                    g.drawRect (handle, 1.0f);
                }
            }
        }

        // Spurkopf – nach der Spur gezeichnet, damit nichts Hereingerolltes darüber liegt
        auto head = row.withWidth (headerWidth);
        g.setColour (selected ? colours::surface : colours::lowerZone);
        g.fillRect (head);
        g.setColour (colours::line);
        g.fillRect (head.getRight() - 1, head.getY(), 1, head.getHeight());

        auto inner = head.reduced (9, 8);
        auto titleRow = inner.removeFromTop (16);
        g.setColour (track.colour);
        g.fillRect (titleRow.removeFromLeft (8).withSizeKeepingCentre (8, 8));
        titleRow.removeFromLeft (6);
        g.setColour (colours::text);
        g.setFont (sansFont (11.5f, Weight::medium));
        g.drawText (track.name.isNotEmpty() ? track.name : "Spur " + juce::String (i + 1), titleRow,
                    juce::Justification::centredLeft, true);

        const auto mute = getMuteBounds (i);
        const auto solo = getSoloBounds (i);
        const auto drawToggle = [&g] (juce::Rectangle<int> area, const juce::String& text, bool on, juce::Colour onColour)
        {
            g.setColour (on ? onColour : colours::surface);
            g.fillRect (area);
            g.setColour (colours::line);
            g.drawRect (area, 1);
            g.setColour (on ? colours::white : colours::textSecondary);
            g.setFont (monoFont (10.0f));
            g.drawText (text, area, juce::Justification::centred, false);
        };
        drawToggle (mute, "M", track.mute, colours::warning);
        drawToggle (solo, "S", track.solo, colours::accent);

        g.setColour (colours::textSecondary);
        g.setFont (monoFont (10.0f));
        g.drawText (gainToText (track.gain),
                    juce::Rectangle<int> (solo.getRight() + 5, solo.getY(),
                                          head.getRight() - 9 - (solo.getRight() + 5), 18),
                    juce::Justification::centredRight, false);
    }

    // Vorschau des hereingezogenen Samples: wo der Clip landet, und ob eine Spur entsteht
    if (dropActive && dropRow >= 0)
    {
        const bool newTrack = dropRow >= (int) zone->tracks.size();
        const auto row = newTrack ? juce::Rectangle<int> (0, (int) zone->tracks.size() * (rowHeight + 1),
                                                          getWidth(), dropStripHeight)
                                  : getRowBounds (dropRow);
        const auto lane = row.withTrimmedLeft (headerWidth);

        if (newTrack)
        {
            g.setColour (colours::accentSoft.withAlpha (0.5f));
            g.fillRect (row);
            g.setColour (colours::accentDark);
            g.setFont (monoFont (10.0f));
            g.drawText ("+ neue Spur", row.withWidth (headerWidth).reduced (10, 0), juce::Justification::centredLeft, false);
        }

        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (lane);

        const juce::Rectangle<float> ghost (timeToX (dropTime), (float) (lane.getY() + clipInset),
                                            juce::jmax (4.0f, (float) dropLength * laneWidth()),
                                            (float) (lane.getHeight() - 2 * clipInset));
        g.setColour (colours::accent.withAlpha (0.18f));
        g.fillRect (ghost);
        g.setColour (colours::accent);
        g.drawRect (ghost, 1.5f);
    }

    // Locator und Wiedergabelinie, nur über den Spuren (nicht über den Köpfen)
    {
        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (getLocalBounds().withTrimmedLeft (headerWidth));

        const float locatorX = timeToX (ctx.ui.locator);
        g.setColour (colours::accent.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (locatorX, 0.0f, 1.0f, (float) getHeight()));

        if (ctx.ui.playing)
        {
            const float x = timeToX (ctx.ui.playhead);
            g.setColour (colours::text);
            g.fillRect (juce::Rectangle<float> (x, 0.0f, 1.0f, (float) getHeight()));
        }
    }
}

//==============================================================================
void TrackArea::mouseMove (const juce::MouseEvent& e)
{
    const int row = rowAt (e.getPosition());
    hoverRow = row;

    auto newCursor = juce::MouseCursor::NormalCursor;

    if (row >= 0 && e.x >= headerWidth)
    {
        switch (dragModeAt (row, e.getPosition()))
        {
            case Drag::leftEdge:
            case Drag::rightEdge: newCursor = juce::MouseCursor::LeftRightResizeCursor; break;
            case Drag::fadeIn:
            case Drag::fadeOut:   newCursor = juce::MouseCursor::LeftRightResizeCursor; break;
            case Drag::move:      newCursor = juce::MouseCursor::DraggingHandCursor; break;
            case Drag::none:      break;
        }
    }

    setMouseCursor (newCursor);
}

void TrackArea::mouseDown (const juce::MouseEvent& e)
{
    // Damit Entf hier ankommt
    grabKeyboardFocus();

    auto* zone = getZone();
    const int row = rowAt (e.getPosition());

    if (zone == nullptr)
        return;

    if (row >= 0 && zone->selectedTrack != row)
    {
        zone->selectedTrack = row;
        ctx.model.notifyChanged();
    }

    if (e.mods.isPopupMenu())
    {
        showTrackMenu (row);
        return;
    }

    if (row < 0)
        return;

    auto& track = zone->tracks[(size_t) row];

    if (e.x < headerWidth)
    {
        if (getMuteBounds (row).contains (e.getPosition()))
        {
            ctx.step (track.mute ? "Spur hörbar"_u : "Spur stumm"_u);
            track.mute = ! track.mute;
            ctx.model.notifyChanged();
        }
        else if (getSoloBounds (row).contains (e.getPosition()))
        {
            ctx.step ("Solo umgeschaltet"_u);
            track.solo = ! track.solo;
            ctx.model.notifyChanged();
        }
        return;
    }

    drag = dragModeAt (row, e.getPosition());
    dragTrack = row;
    grabOffset = positionToTime (e.x) - track.offset;
}

void TrackArea::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::none)
        return;

    // Am Rand weiterrollen, damit ein Clip über den sichtbaren Ausschnitt hinaus gezogen werden kann
    if (e.x > getWidth() - 16)
        scrollBy (0.02);
    else if (e.x < headerWidth + 8 && ctx.ui.timelineStart > 0.0)
        scrollBy (-0.02);

    applyDrag (e);
}

void TrackArea::mouseUp (const juce::MouseEvent&)
{
    drag = Drag::none;
    dragTrack = -1;
}

void TrackArea::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Waagerecht wischen oder Shift+Rad rollt die Zeit, alles andere die Spuren
    if (wheel.deltaX != 0.0f || e.mods.isShiftDown())
    {
        const float delta = wheel.deltaX != 0.0f ? -wheel.deltaX : -wheel.deltaY;
        scrollBy ((double) delta * 0.5);
        return;
    }

    Component::mouseWheelMove (e, wheel);
}

void TrackArea::scrollBy (double delta)
{
    const double limit = juce::jmax (0.0, juce::jmax (axisLength (getZone()), ctx.ui.timelineStart + 1.0) - 1.0);
    const double next = juce::jlimit (0.0, juce::jmax (limit, drag != Drag::none ? maxAxis - 1.0 : 0.0),
                                      ctx.ui.timelineStart + delta);

    if (std::abs (next - ctx.ui.timelineStart) > 1.0e-9)
    {
        ctx.ui.timelineStart = next;
        ctx.ui.changed();
    }
}

bool TrackArea::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelected();
        return true;
    }

    return false;
}

void TrackArea::applyDrag (const juce::MouseEvent& e)
{
    auto* zone = getZone();
    if (zone == nullptr || ! juce::isPositiveAndBelow (dragTrack, (int) zone->tracks.size()))
        return;

    auto& track = zone->tracks[(size_t) dragTrack];
    const double pointerTime = positionToTime (e.x);
    const bool snapping = ctx.ui.snapToGrid;
    const bool stretching = isStretching (e);

    geometry::ClipState clip { track.offset, track.natural, track.stretch,
                               track.trimStart, track.trimEnd, track.fadeIn, track.fadeOut };

    switch (drag)
    {
        case Drag::move:      clip = geometry::moveClip (clip, pointerTime, grabOffset, snapping, maxAxis); break;
        case Drag::rightEdge: clip = geometry::dragRightEdge (clip, pointerTime, snapping, stretching, maxAxis); break;
        case Drag::leftEdge:  clip = geometry::dragLeftEdge (clip, pointerTime, snapping, stretching); break;
        case Drag::fadeIn:    clip = geometry::dragFadeIn (clip, pointerTime); break;
        case Drag::fadeOut:   clip = geometry::dragFadeOut (clip, pointerTime); break;
        case Drag::none:      return;
    }

    // Der Name der Geste hält den ganzen Zug als einen Schritt zusammen
    switch (drag)
    {
        case Drag::move:      ctx.step ("Clip verschoben"_u); break;
        case Drag::rightEdge:
        case Drag::leftEdge:  ctx.step (stretching ? "Clip gestreckt"_u : "Clip zugeschnitten"_u); break;
        case Drag::fadeIn:    ctx.step ("Fade ein geändert"_u); break;
        case Drag::fadeOut:   ctx.step ("Fade aus geändert"_u); break;
        case Drag::none:      break;
    }

    track.offset = clip.offset;
    track.stretch = clip.stretch;
    track.trimStart = clip.trimStart;
    track.trimEnd = clip.trimEnd;
    track.fadeIn = clip.fadeIn;
    track.fadeOut = clip.fadeOut;

    ctx.model.notifyChanged();
}

//==============================================================================
void TrackArea::deleteSelected()
{
    const auto* zone = getZone();

    if (zone == nullptr || ! juce::isPositiveAndBelow (zone->selectedTrack, (int) zone->tracks.size()))
        return;

    const int index = zone->selectedTrack;

    if (zone->tracks[(size_t) index].hasClip())
        removeClip (index);
    else
        removeTrack (index);
}

void TrackArea::removeClip (int trackIndex)
{
    auto* zone = getZone();

    if (zone == nullptr || ! juce::isPositiveAndBelow (trackIndex, (int) zone->tracks.size()))
        return;

    auto& track = zone->tracks[(size_t) trackIndex];
    const auto clipName = track.clip;

    ctx.step ("Clip entfernt");
    track.clip.clear();
    ctx.model.notifyChanged();
    ctx.toast (clipName + " entfernt · die Spur bleibt · Strg+Z holt ihn zurück"_u);
}

void TrackArea::removeTrack (int trackIndex)
{
    auto* zone = getZone();

    if (zone == nullptr || ! juce::isPositiveAndBelow (trackIndex, (int) zone->tracks.size()))
        return;

    const auto& track = zone->tracks[(size_t) trackIndex];
    const auto name = track.name.isNotEmpty() ? track.name : "Spur " + juce::String (trackIndex + 1);

    ctx.step (name + " gelöscht"_u);
    ctx.processor.removeTrack (zone->id, trackIndex);
    ctx.toast (name + " gelöscht · Strg+Z holt sie zurück"_u);
}

void TrackArea::showTrackMenu (int trackIndex)
{
    const auto* zone = getZone();
    juce::PopupMenu menu;

    if (zone != nullptr && juce::isPositiveAndBelow (trackIndex, (int) zone->tracks.size()))
    {
        const auto& track = zone->tracks[(size_t) trackIndex];
        menu.addSectionHeader (track.name.isNotEmpty() ? track.name : "Spur " + juce::String (trackIndex + 1));
        menu.addItem ("Clip entfernen (Entf)", track.hasClip(), false, [this, trackIndex] { removeClip (trackIndex); });
        menu.addItem ("Spur löschen"_u, [this, trackIndex] { removeTrack (trackIndex); });
        menu.addSeparator();
    }

    menu.addItem ("Spur hinzufügen"_u, [this] { ctx.commands.invokeDirectly (cmd::addTrack, false); });
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

//==============================================================================
bool TrackArea::isInterestedInDragSource (const SourceDetails& details)
{
    return sampleDrop::indexOf (details) >= 0;
}

void TrackArea::itemDragMove (const SourceDetails& details)
{
    const auto* zone = getZone();
    const int index = sampleDrop::indexOf (details);

    dropActive = true;
    dropRow = -1;

    if (zone != nullptr)
    {
        const int row = details.localPosition.y / (rowHeight + 1);
        dropRow = juce::jmin (row, (int) zone->tracks.size());

        if (juce::isPositiveAndBelow (index, (int) ctx.model.samples.size()))
            dropLength = ctx.model.samples[(size_t) index].lengthSeconds / InstrumentModel::timelineSeconds;

        dropTime = geometry::snapTime (positionToTime (details.localPosition.x), ctx.ui.snapToGrid);
    }

    repaint();
}

void TrackArea::itemDragExit (const SourceDetails&)
{
    dropActive = false;
    dropRow = -1;
    repaint();
}

void TrackArea::itemDropped (const SourceDetails& details)
{
    dropActive = false;
    dropRow = -1;
    repaint();

    auto* zone = getZone();

    if (zone == nullptr)
    {
        ctx.toast ("Erst im Mapping eine Zone oder ein Kit-Teil wählen"_u);
        return;
    }

    const auto* sample = sampleDrop::resolve (ctx, sampleDrop::indexOf (details));

    if (sample == nullptr)
        return;

    const double length = sample->lengthSeconds / InstrumentModel::timelineSeconds;
    const double time = juce::jlimit (0.0, juce::jmax (0.0, maxAxis - length),
                                      geometry::snapTime (positionToTime (details.localPosition.x), ctx.ui.snapToGrid));
    const int row = details.localPosition.y / (rowHeight + 1);

    ctx.step (sample->name + " auf Spur gelegt");

    if (juce::isPositiveAndBelow (row, (int) zone->tracks.size()))
    {
        // Auf eine Spur: ihr Clip wird ersetzt – eine Spur hat genau einen
        auto& track = zone->tracks[(size_t) row];
        const bool replacing = track.hasClip();
        const auto previous = track.clip;

        track.placeSample (*sample, time);
        zone->selectedTrack = row;
        ctx.model.notifyChanged();
        ctx.toast (replacing ? sample->name + " ersetzt "_u + previous
                             : sample->name + " → "_u + track.name);
    }
    else
    {
        // Darunter: neue Spur
        const auto& palette = trackPalette()[zone->tracks.size() % trackPalette().size()];

        Track track;
        track.name.clear();
        track.colour = palette.main;
        track.softColour = palette.soft;
        track.gain = 0.8f;
        track.placeSample (*sample, time);

        zone->tracks.push_back (std::move (track));
        zone->selectedTrack = (int) zone->tracks.size() - 1;
        ctx.model.notifyChanged();
        ctx.toast ("Neue Spur "_u + zone->tracks.back().name);
    }

    grabKeyboardFocus();
}

//==============================================================================
void TrackArea::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (ctx.ui.playing && ! isTimerRunning())
        startTimerHz (30);
    else if (! ctx.ui.playing && isTimerRunning())
        stopTimer();

    repaint();
}

void TrackArea::timerCallback()
{
    // Die Laufmarke nicht aus dem Bild laufen lassen: umblättern, sobald sie den Rand erreicht
    const double start = ctx.ui.timelineStart;

    if (ctx.ui.playing && (ctx.ui.playhead > start + 1.0 || ctx.ui.playhead < start))
    {
        ctx.ui.timelineStart = juce::jmax (0.0, ctx.ui.playhead - 0.05);
        ctx.ui.changed();
    }

    repaint();
}
} // namespace sis
