#pragma once

#include <JuceHeader.h>

#include "StudioContext.h"

namespace sis
{
/** Statusleiste (30 px): links Kontext, rechts Audio-Format, Puffer und CPU. */
class StatusBar final : public juce::Component,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    explicit StatusBar (StudioContext&);
    ~StatusBar() override;

    /** Wird beim Klick auf die Audio-Angaben aufgerufen (nur Standalone). */
    std::function<void()> onAudioSettingsRequested;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    juce::String getContextText() const;
    juce::StringArray getSystemTexts() const;

    StudioContext& ctx;
    juce::Rectangle<int> audioInfoArea;
    int lastCpuPercent = -1;
    int lastVoiceCount = -1;
};
} // namespace sis
