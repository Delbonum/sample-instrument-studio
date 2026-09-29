#include "SampleEditor.h"
#include "../../PluginProcessor.h"

namespace sis
{
namespace
{
    const std::array<std::pair<SampleTool, const char*>, 6> toolDefs { {
        { SampleTool::select,    "Auswahl" },
        { SampleTool::trim,      "Zuschneiden" },
        { SampleTool::fadeIn,    "Fade ein" },
        { SampleTool::fadeOut,   "Fade aus" },
        { SampleTool::normalise, "Normalisieren" },
        { SampleTool::reverse,   "Umkehren" },
    } };

    constexpr int handleGrab = 5;
    constexpr int locatorStrip = 12;   // oben im Wellenfeld: dort setzt ein Klick den Locator

    // Die Schleife in eigener Farbe – die Auswahl ist schon violett, beides soll man nicht verwechseln
    const juce::Colour loopColour { 0xff3b8f6e };
}

SampleEditor::SampleEditor (StudioContext& c) : ctx (c)
{
    for (size_t i = 0; i < tools.size(); ++i)
    {
        auto& button = tools[i];
        button.setButtonText (juce::String::fromUTF8 (toolDefs[i].second));
        button.onClick = [this, i] { applyTool (toolDefs[i].first); };
        addAndMakeVisible (button);
    }

    updateToolStyles();
    ctx.model.addChangeListener (this);
    ctx.ui.addChangeListener (this);
}

SampleEditor::~SampleEditor()
{
    ctx.model.removeChangeListener (this);
    ctx.ui.removeChangeListener (this);
}

Track* SampleEditor::getTrack() const
{
    auto* zone = ctx.model.getSelectedZone();
    return zone != nullptr ? zone->getSelectedTrack() : nullptr;
}

Clip* SampleEditor::getClip() const
{
    return currentClip (ctx.model, ctx.ui);
}

double SampleEditor::sampleSeconds() const
{
    if (const auto* clip = getClip())
    {
        if (const auto* sample = ctx.model.findSample (clip->sample))
            if (sample->lengthSeconds > 0.0)
                return sample->lengthSeconds;

        return clip->natural * InstrumentModel::timelineSeconds;
    }

    return 0.0;
}

bool SampleEditor::timeToFraction (double time, double& fraction) const
{
    const auto* clip = getClip();

    if (clip == nullptr || clip->length() <= 0.0)
        return false;

    const double share = (time - clip->offset) / clip->length();
    fraction = clip->trimStart + share * (clip->trimEnd - clip->trimStart);
    return fraction >= 0.0 && fraction <= 1.0;
}

double SampleEditor::fractionToTime (double fraction) const
{
    const auto* clip = getClip();

    if (clip == nullptr || clip->trimEnd - clip->trimStart <= 0.0)
        return ctx.ui.locator;

    const double share = (fraction - clip->trimStart) / (clip->trimEnd - clip->trimStart);
    return juce::jmax (0.0, clip->offset + share * clip->length());
}

void SampleEditor::updateToolStyles()
{
    for (size_t i = 0; i < tools.size(); ++i)
    {
        const auto tool = toolDefs[i].first;
        const auto* track = getTrack();
        const bool on = tool == SampleTool::reverse ? (track != nullptr && track->reverse)
                                                    : ctx.ui.tool == tool;

        FlatButton::Style style;
        style.fontSize = 11.0f;
        style.background = on ? colours::accentSoft : colours::surface;
        style.text = on ? colours::accentDark : colours::textSecondary;
        style.border = on ? colours::accent : colours::line;
        tools[i].setStyle (style);
    }
}

void SampleEditor::applyTool (SampleTool tool)
{
    auto* track = getTrack();
    auto* clip = getClip();

    if (track == nullptr)
    {
        ctx.toast ("Keine Spur gewählt"_u);
        return;
    }

    if (clip == nullptr && tool != SampleTool::select && tool != SampleTool::reverse)
    {
        ctx.toast ("Kein Clip auf dieser Spur"_u);
        return;
    }

    switch (tool)
    {
        case SampleTool::select:
            ctx.ui.tool = tool;
            ctx.ui.changed();
            return;

        case SampleTool::trim:
        {
            const double a = juce::jmin (ctx.ui.selectionStart, ctx.ui.selectionEnd);
            const double b = juce::jmax (ctx.ui.selectionStart, ctx.ui.selectionEnd);
            ctx.step ("Auf Auswahl zugeschnitten"_u);
            clip->trimStart = a;
            clip->trimEnd = juce::jmax (a + 0.02, b);
            ctx.model.notifyChanged();
            ctx.toast ("Clip auf Auswahl zugeschnitten · "_u + clip->sample);
            return;
        }

        case SampleTool::fadeIn:
            ctx.step ("Fade ein gesetzt"_u);
            clip->fadeIn = juce::jmin (0.12, 1.0 - clip->fadeOut);
            ctx.model.notifyChanged();
            ctx.toast ("Fade ein gesetzt · Ecke im Clip ziehen ändert die Länge"_u);
            return;

        case SampleTool::fadeOut:
            ctx.step ("Fade aus gesetzt"_u);
            clip->fadeOut = juce::jmin (0.18, 1.0 - clip->fadeIn);
            ctx.model.notifyChanged();
            ctx.toast ("Fade aus gesetzt · Ecke im Clip ziehen ändert die Länge"_u);
            return;

        case SampleTool::normalise:
            ctx.toast ("Normalisieren folgt mit der Audio-Engine"_u);
            return;

        case SampleTool::reverse:
            ctx.step ("Richtung umgekehrt"_u);
            track->reverse = ! track->reverse;
            ctx.model.notifyChanged();
            ctx.toast (track->reverse ? "Umgekehrt abspielen: ein"_u : "Umgekehrt abspielen: aus"_u);
            return;
    }
}

//==============================================================================
void SampleEditor::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (34);

    auto row = headerArea.reduced (12, 8);
    for (int i = (int) tools.size(); --i >= 0;)
    {
        auto& button = tools[(size_t) i];
        const int width = button.getTextWidth() + 18;
        button.setBounds (row.removeFromRight (width).withSizeKeepingCentre (width, 24));
        row.removeFromRight (8);
    }

    toolsLeft = row.getRight();
    waveArea = area.reduced (12).withTrimmedTop (0);
}

float SampleEditor::fractionToX (double fraction) const
{
    const auto inner = waveArea.reduced (1).toFloat();
    return inner.getX() + (float) fraction * inner.getWidth();
}

void SampleEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::lowerZone);
    g.setColour (colours::lineStrong);
    g.fillRect (0, 0, getWidth(), 1);

    /* Nur bis zum ersten Werkzeug schreiben: im Zonen-Dialog der Plugin-Ansicht ist die
       Kopfzeile halb so breit wie im Studio, und der Text liefe sonst unter die Knöpfe. */
    auto header = headerArea.reduced (12, 0);
    header.setRight (juce::jmax (header.getX(), toolsLeft - 8));

    const auto* track = getTrack();
    const auto* clip = getClip();

    const double seconds = sampleSeconds();
    const auto selection = clip != nullptr
                               ? "Auswahl " + juce::String (ctx.ui.selectionStart * seconds, 2) + " – "_u
                                     + juce::String (ctx.ui.selectionEnd * seconds, 2) + " s"
                               : juce::String();

    const juce::String caption ("SAMPLE-EDITOR");
    const int capsWidth = juce::roundToInt (textWidth (capsFont(), caption)) + 4;
    const int selectionWidth = juce::roundToInt (textWidth (monoFont (10.0f), selection)) + 4;
    constexpr int minNameWidth = 90;

    /* Wird es eng, fällt zuerst der Titel weg. Dass dies der Sample-Editor ist, sieht man
       auch ohne ihn; welche Datei und welcher Ausschnitt gemeint sind, dagegen nicht. */
    if (header.getWidth() >= capsWidth + 8 + minNameWidth + 8 + selectionWidth)
    {
        draw::capsLabel (g, caption, header.removeFromLeft (capsWidth));
        header.removeFromLeft (8);
    }

    if (clip != nullptr)
    {
        auto selectionArea = header.removeFromRight (juce::jmin (selectionWidth, header.getWidth()));
        header.removeFromRight (8);

        g.setColour (colours::text);
        g.setFont (sansFont (11.5f, Weight::medium));
        g.drawText (clip->sample, header, juce::Justification::centredLeft, true);

        g.setColour (colours::textTertiary);
        g.setFont (monoFont (10.0f));
        g.drawText (selection, selectionArea, juce::Justification::centredRight, true);
    }

    // Wellenform
    g.setColour (colours::surface);
    g.fillRect (waveArea);
    g.setColour (colours::line);
    g.drawRect (waveArea, 1);

    if (track == nullptr || clip == nullptr)
    {
        g.setColour (colours::textTertiary);
        g.setFont (sansFont (12.0f));
        g.drawText (track == nullptr ? "Keine Spur gewählt"_u : "Kein Clip auf dieser Spur"_u,
                    waveArea, juce::Justification::centred, true);
        return;
    }

    auto* zone = ctx.model.getSelectedZone();
    const auto peaks = ctx.model.waveformFor (*clip, zone != nullptr ? zone->selectedTrack : 0);
    const auto inner = waveArea.reduced (1).toFloat();
    const int bars = (int) peaks.size();
    const float barWidth = inner.getWidth() / (float) bars;

    for (int i = 0; i < bars; ++i)
    {
        const double t = i / (double) juce::jmax (1, bars - 1);
        const bool inTrim = t >= clip->trimStart && t <= clip->trimEnd;
        const bool inSelection = t >= juce::jmin (ctx.ui.selectionStart, ctx.ui.selectionEnd)
                                 && t <= juce::jmax (ctx.ui.selectionStart, ctx.ui.selectionEnd);

        const float value = peaks[(size_t) (track->reverse ? bars - 1 - i : i)];
        const float h = juce::jmax (0.04f, value) * inner.getHeight();

        g.setColour (! inTrim ? colours::waveOutside : (inSelection ? colours::accent : colours::waveInactive));
        g.fillRect (inner.getX() + (float) i * barWidth, inner.getCentreY() - h * 0.5f,
                    juce::jmax (1.0f, barWidth - 1.0f), h);
    }

    // Was nicht gespielt wird, tritt zurück: der Clip ist nur der zugeschnittene Teil
    g.setColour (colours::lowerZone.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (inner.getX(), inner.getY(), fractionToX (clip->trimStart), inner.getBottom()));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (fractionToX (clip->trimEnd), inner.getY(), inner.getRight(), inner.getBottom()));

    // Auswahlrahmen mit zwei Griffen
    const float left = fractionToX (ctx.ui.selectionStart);
    const float right = fractionToX (ctx.ui.selectionEnd);

    g.setColour (colours::accent.withAlpha (0.07f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (left, inner.getY(), right, inner.getBottom()));
    g.setColour (colours::accent);
    g.fillRect (juce::Rectangle<float> (left, inner.getY(), 1.0f, inner.getHeight()));
    g.fillRect (juce::Rectangle<float> (right - 1.0f, inner.getY(), 1.0f, inner.getHeight()));
    g.fillRect (juce::Rectangle<float> (left, inner.getY() + (float) locatorStrip, 3.0f, 13.0f));
    g.fillRect (juce::Rectangle<float> (right - 3.0f, inner.getY() + (float) locatorStrip, 3.0f, 13.0f));

    /* Schleife: wo sie beginnt und wo übergeblendet wird – beides innerhalb des gespielten
       Ausschnitts. Die Überblendung liegt an beiden Enden der Schleife: hinten klingt ihr
       Ende aus, während vorn ihr Anfang schon einsetzt. */
    if (track->loop != LoopMode::oneShot)
    {
        const double span = clip->trimEnd - clip->trimStart;
        const double loopFrom = clip->trimStart + track->loopStart * span;
        const double fade = track->loopCrossfade * (clip->trimEnd - loopFrom);
        const auto band = [&] (double from, double to)
        {
            return juce::Rectangle<float>::leftTopRightBottom (fractionToX (from), inner.getY(), fractionToX (to), inner.getBottom());
        };

        g.setColour (loopColour.withAlpha (0.08f));
        g.fillRect (band (loopFrom, clip->trimEnd));

        if (fade > 0.0)
        {
            for (const auto& area : { band (loopFrom, loopFrom + fade), band (clip->trimEnd - fade, clip->trimEnd) })
            {
                const juce::Graphics::ScopedSaveState state (g);
                g.reduceClipRegion (area.toNearestInt());
                g.setColour (loopColour.withAlpha (0.35f));

                for (float x = area.getX() - area.getHeight(); x < area.getRight(); x += 6.0f)
                    g.drawLine (x, area.getBottom(), x + area.getHeight(), area.getY(), 1.0f);

                g.setColour (loopColour);
                g.setFont (monoFont (9.0f));
                g.drawText ("X-FADE", area.reduced (3.0f, 2.0f).toNearestInt(), juce::Justification::bottomLeft, false);
            }
        }

        g.setColour (loopColour);
        g.fillRect (juce::Rectangle<float> (fractionToX (loopFrom), inner.getY(), 1.0f, inner.getHeight()));
        g.fillRect (juce::Rectangle<float> (fractionToX (clip->trimEnd) - 1.0f, inner.getY(), 1.0f, inner.getHeight()));
        g.setFont (monoFont (9.5f));
        g.drawText (track->loop == LoopMode::pingPong ? "LOOP VOR/RÜCK"_u : juce::String ("LOOP"),
                    juce::Rectangle<float> (fractionToX (loopFrom) + 4.0f, inner.getY() + (float) locatorStrip + 2.0f, 110.0f, 13.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    }

    // Laufmarke und Locator – derselbe wie im Zeitlineal oben
    double fraction = 0.0;

    if (ctx.ui.playing && timeToFraction (ctx.ui.playhead, fraction))
    {
        g.setColour (colours::text);
        g.fillRect (juce::Rectangle<float> (fractionToX (fraction), inner.getY(), 1.0f, inner.getHeight()));
    }

    if (timeToFraction (ctx.ui.locator, fraction))
    {
        const float x = fractionToX (fraction);
        juce::Path marker;
        marker.addTriangle (x - 5.0f, inner.getY(), x + 5.0f, inner.getY(), x, inner.getY() + 8.0f);
        g.setColour (colours::accent);
        g.fillPath (marker);
        g.fillRect (juce::Rectangle<float> (x, inner.getY(), 1.0f, inner.getHeight()));
    }
}

double SampleEditor::positionToFraction (int x) const
{
    const auto inner = waveArea.reduced (1);
    return juce::jlimit (0.0, 1.0, (double) (x - inner.getX()) / (double) juce::jmax (1, inner.getWidth()));
}

bool SampleEditor::isOnLocator (juce::Point<int> p) const
{
    double fraction = 0.0;

    if (! waveArea.contains (p) || ! timeToFraction (ctx.ui.locator, fraction))
        return false;

    return std::abs ((float) p.x - fractionToX (fraction)) <= (float) handleGrab;
}

void SampleEditor::mouseMove (const juce::MouseEvent& e)
{
    const bool locator = isOnLocator (e.getPosition())
                         || (waveArea.contains (e.getPosition()) && e.y < waveArea.getY() + locatorStrip);
    setMouseCursor (locator ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void SampleEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! waveArea.contains (e.getPosition()) || getClip() == nullptr)
        return;

    // Locator: am Strich greifen oder oben im Streifen setzen
    if (isOnLocator (e.getPosition()) || e.y < waveArea.getY() + locatorStrip)
    {
        draggingHandle = 3;
        ctx.ui.locator = fractionToTime (positionToFraction (e.x));
        ctx.ui.changed();
        return;
    }

    const float left = fractionToX (ctx.ui.selectionStart);
    const float right = fractionToX (ctx.ui.selectionEnd);

    if (std::abs ((float) e.x - left) <= handleGrab)
        draggingHandle = 1;
    else if (std::abs ((float) e.x - right) <= handleGrab)
        draggingHandle = 2;
    else
    {
        // Neue Auswahl aufziehen
        draggingHandle = 2;
        ctx.ui.selectionStart = positionToFraction (e.x);
        ctx.ui.selectionEnd = ctx.ui.selectionStart;
        ctx.ui.changed();
    }
}

void SampleEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingHandle == 0)
        return;

    const double f = positionToFraction (e.x);

    if (draggingHandle == 3)
    {
        ctx.ui.locator = fractionToTime (f);
        ctx.ui.changed();
        return;
    }

    if (draggingHandle == 1)
        ctx.ui.selectionStart = juce::jmin (f, ctx.ui.selectionEnd - 0.02);
    else
        ctx.ui.selectionEnd = juce::jmax (f, ctx.ui.selectionStart + 0.02);

    ctx.ui.selectionStart = juce::jlimit (0.0, 1.0, ctx.ui.selectionStart);
    ctx.ui.selectionEnd = juce::jlimit (0.0, 1.0, ctx.ui.selectionEnd);
    ctx.ui.changed();
}

void SampleEditor::mouseUp (const juce::MouseEvent&)
{
    draggingHandle = 0;
}

void SampleEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Die Laufmarke braucht während der Wiedergabe laufend ein neues Bild
    if (ctx.ui.playing && ! isTimerRunning())
        startTimerHz (30);
    else if (! ctx.ui.playing && isTimerRunning())
        stopTimer();

    updateToolStyles();
    resized();
    repaint();
}

void SampleEditor::timerCallback()
{
    repaint();
}
} // namespace sis
