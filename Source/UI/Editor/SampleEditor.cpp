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

double SampleEditor::sampleSeconds() const
{
    if (const auto* track = getTrack())
    {
        if (const auto* sample = ctx.model.findSample (track->clip))
            if (sample->lengthSeconds > 0.0)
                return sample->lengthSeconds;

        return track->natural * InstrumentModel::timelineSeconds;
    }

    return 0.0;
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

    if (track == nullptr)
    {
        ctx.toast ("Keine Spur gewählt"_u);
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
            track->trimStart = a;
            track->trimEnd = juce::jmax (a + 0.02, b);
            ctx.model.notifyChanged();
            ctx.toast ("Clip auf Auswahl zugeschnitten · "_u + track->name);
            return;
        }

        case SampleTool::fadeIn:
            ctx.step ("Fade ein gesetzt"_u);
            track->fadeIn = 0.12;
            ctx.model.notifyChanged();
            ctx.toast ("Fade ein gesetzt · Ecke im Clip ziehen ändert die Länge"_u);
            return;

        case SampleTool::fadeOut:
            ctx.step ("Fade aus gesetzt"_u);
            track->fadeOut = 0.18;
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

    const double seconds = sampleSeconds();
    const auto selection = track != nullptr
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

    if (track != nullptr)
    {
        auto selectionArea = header.removeFromRight (juce::jmin (selectionWidth, header.getWidth()));
        header.removeFromRight (8);

        g.setColour (colours::text);
        g.setFont (sansFont (11.5f, Weight::medium));
        g.drawText (track->clip, header, juce::Justification::centredLeft, true);

        g.setColour (colours::textTertiary);
        g.setFont (monoFont (10.0f));
        g.drawText (selection, selectionArea, juce::Justification::centredRight, true);
    }

    // Wellenform
    g.setColour (colours::surface);
    g.fillRect (waveArea);
    g.setColour (colours::line);
    g.drawRect (waveArea, 1);

    if (track == nullptr)
    {
        g.setColour (colours::textTertiary);
        g.setFont (sansFont (12.0f));
        g.drawText ("Keine Spur gewählt"_u, waveArea, juce::Justification::centred, true);
        return;
    }

    auto* zone = ctx.model.getSelectedZone();
    const auto peaks = ctx.model.waveformFor (*track, zone != nullptr ? zone->selectedTrack : 0);
    const auto inner = waveArea.reduced (1).toFloat();
    const int bars = (int) peaks.size();
    const float barWidth = inner.getWidth() / (float) bars;

    for (int i = 0; i < bars; ++i)
    {
        const double t = i / (double) juce::jmax (1, bars - 1);
        const bool inTrim = t >= track->trimStart && t <= track->trimEnd;
        const bool inSelection = t >= juce::jmin (ctx.ui.selectionStart, ctx.ui.selectionEnd)
                                 && t <= juce::jmax (ctx.ui.selectionStart, ctx.ui.selectionEnd);

        const float value = peaks[(size_t) (track->reverse ? bars - 1 - i : i)];
        const float h = juce::jmax (0.04f, value) * inner.getHeight();

        g.setColour (! inTrim ? colours::waveOutside : (inSelection ? colours::accent : colours::waveInactive));
        g.fillRect (inner.getX() + (float) i * barWidth, inner.getCentreY() - h * 0.5f,
                    juce::jmax (1.0f, barWidth - 1.0f), h);
    }

    // Auswahlrahmen mit zwei Griffen
    const float left = inner.getX() + (float) ctx.ui.selectionStart * inner.getWidth();
    const float right = inner.getX() + (float) ctx.ui.selectionEnd * inner.getWidth();

    g.setColour (colours::accent.withAlpha (0.07f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (left, inner.getY(), right, inner.getBottom()));
    g.setColour (colours::accent);
    g.fillRect (juce::Rectangle<float> (left, inner.getY(), 1.0f, inner.getHeight()));
    g.fillRect (juce::Rectangle<float> (right - 1.0f, inner.getY(), 1.0f, inner.getHeight()));
    g.fillRect (juce::Rectangle<float> (left, inner.getY(), 3.0f, 13.0f));
    g.fillRect (juce::Rectangle<float> (right - 3.0f, inner.getY(), 3.0f, 13.0f));

    /* Schleife: wo sie beginnt und wo übergeblendet wird. Die Überblendung liegt an beiden
       Enden – hinten klingt das Ende aus, während vorn der Loop-Anfang schon einsetzt. */
    if (track->loop != LoopMode::oneShot)
    {
        const double span = track->trimEnd - track->trimStart;
        const double loopFrom = track->trimStart + track->loopStart * span;
        const double fade = track->loopCrossfade * (track->trimEnd - loopFrom);
        const auto xAt = [&inner] (double fraction) { return inner.getX() + (float) fraction * inner.getWidth(); };

        g.setColour (colours::accentHover.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xAt (loopFrom), inner.getY(), xAt (track->trimEnd), inner.getBottom()));

        if (fade > 0.0)
        {
            g.setColour (colours::accentHover.withAlpha (0.22f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xAt (loopFrom), inner.getY(), xAt (loopFrom + fade), inner.getBottom()));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xAt (track->trimEnd - fade), inner.getY(), xAt (track->trimEnd), inner.getBottom()));
        }

        g.setColour (colours::accentHover);
        g.fillRect (juce::Rectangle<float> (xAt (loopFrom), inner.getY(), 1.0f, inner.getHeight()));
        g.setFont (monoFont (9.5f));
        g.drawText (track->loop == LoopMode::pingPong ? "LOOP VOR/RÜCK"_u : juce::String ("LOOP"),
                    juce::Rectangle<float> (xAt (loopFrom) + 4.0f, inner.getBottom() - 15.0f, 110.0f, 13.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    }
}

double SampleEditor::positionToFraction (int x) const
{
    const auto inner = waveArea.reduced (1);
    return juce::jlimit (0.0, 1.0, (double) (x - inner.getX()) / (double) juce::jmax (1, inner.getWidth()));
}

void SampleEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! waveArea.contains (e.getPosition()))
        return;

    const auto inner = waveArea.reduced (1);
    const float left = (float) inner.getX() + (float) ctx.ui.selectionStart * (float) inner.getWidth();
    const float right = (float) inner.getX() + (float) ctx.ui.selectionEnd * (float) inner.getWidth();

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

    if (draggingHandle == 1)
        ctx.ui.selectionStart = juce::jmin (f, ctx.ui.selectionEnd - 0.02);
    else
        ctx.ui.selectionEnd = juce::jmax (f, ctx.ui.selectionStart + 0.02);

    ctx.ui.selectionStart = juce::jlimit (0.0, 1.0, ctx.ui.selectionStart);
    ctx.ui.selectionEnd = juce::jlimit (0.0, 1.0, ctx.ui.selectionEnd);
    ctx.ui.changed();
}

void SampleEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateToolStyles();
    resized();
    repaint();
}
} // namespace sis
