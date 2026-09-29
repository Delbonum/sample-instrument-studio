#include "StatusBar.h"
#include "../../PluginProcessor.h"
#include "../Widgets.h"

namespace sis
{
StatusBar::StatusBar (StudioContext& c) : ctx (c)
{
    ctx.ui.addChangeListener (this);
    ctx.model.addChangeListener (this);
    startTimer (500);
}

StatusBar::~StatusBar()
{
    ctx.ui.removeChangeListener (this);
    ctx.model.removeChangeListener (this);
}

juce::String StatusBar::getContextText() const
{
    if (ctx.ui.heldNote >= 0)
    {
        const auto* zone = ctx.model.findZoneForNote (ctx.ui.heldNote,
                                                      ctx.ui.velocityLayers ? ctx.ui.heldVelocity : -1);
        return "Note " + noteName (ctx.ui.heldNote) + " → "_u + (zone != nullptr ? zone->name : "keine Zone");
    }

    const auto* zone = ctx.model.getSelectedZone();
    if (zone == nullptr)
        return "Keine Zone";

    const auto* track = zone->getSelectedTrack();
    auto text = "Zone " + zone->name + " · Spur "_u + (track != nullptr ? track->name : "–"_u);

    // Ohne geladene Audiodaten bleibt die Zone stumm - das gehört gesagt.
    if (! ctx.processor.zoneHasAudio (*zone))
        text += " · keine Audiodaten"_u;

    return text;
}

juce::StringArray StatusBar::getSystemTexts() const
{
    auto& p = ctx.processor;
    juce::StringArray texts;

    const double rate = p.getSampleRate();
    if (rate > 0.0)
    {
        const double khz = rate / 1000.0;
        const auto rateText = std::abs (khz - std::round (khz)) < 0.05 ? juce::String ((int) std::round (khz))
                                                                       : juce::String (khz, 1);
        texts.add (rateText + " kHz · 24 Bit · Puffer "_u + juce::String (p.getBlockSize()));
    }
    else
    {
        texts.add ("Audio inaktiv");
    }

    const auto megabytes = (double) p.getSampleMemoryBytes() / (1024.0 * 1024.0);
    texts.add ("RAM " + juce::String (megabytes, megabytes < 10.0 ? 1 : 0) + " MB");
    texts.add (juce::String (p.getActiveVoiceCount()) + " Stimmen");
    texts.add ("CPU " + juce::String (juce::jmax (0, lastCpuPercent)) + " %");
    return texts;
}

void StatusBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::line);
    g.fillRect (0, 0, getWidth(), 1);

    auto area = getLocalBounds().withTrimmedTop (1).reduced (14, 0);
    const auto font = monoFont (10.5f);
    g.setFont (font);

    // Rechts: gap 16px zwischen den Einträgen
    auto texts = getSystemTexts();
    g.setColour (colours::textTertiary);
    for (int i = texts.size(); --i >= 0;)
    {
        const int w = juce::roundToInt (textWidth (font, texts[i])) + 2;
        const auto textArea = area.removeFromRight (w);

        if (i == 0)
        {
            // Die Audio-Angaben führen zu den Einstellungen
            audioInfoArea = textArea;

            if (onAudioSettingsRequested != nullptr)
                g.setColour (isMouseOver() ? colours::accentHover : colours::textTertiary);
        }

        g.drawText (texts[i], textArea, juce::Justification::centredRight, false);
        area.removeFromRight (16);
    }

    g.setColour (colours::textSecondary);
    g.drawText (getContextText(), area, juce::Justification::centredLeft, true);
}

void StatusBar::mouseDown (const juce::MouseEvent& e)
{
    if (onAudioSettingsRequested != nullptr && audioInfoArea.contains (e.getPosition()))
        onAudioSettingsRequested();
}

void StatusBar::mouseMove (const juce::MouseEvent& e)
{
    const bool overAudio = onAudioSettingsRequested != nullptr && audioInfoArea.contains (e.getPosition());
    setMouseCursor (overAudio ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint (audioInfoArea);
}

void StatusBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void StatusBar::timerCallback()
{
    const int cpu = juce::roundToInt (ctx.processor.getCpuLoad() * 100.0);
    const int voices = ctx.processor.getActiveVoiceCount();

    if (cpu != lastCpuPercent || voices != lastVoiceCount)
    {
        lastCpuPercent = cpu;
        lastVoiceCount = voices;
        repaint();
    }
}
} // namespace sis
