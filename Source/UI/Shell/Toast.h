#pragma once

#include <JuceHeader.h>

namespace sis
{
/** Kurze Einblendung unten mittig (#191817, weiße Schrift, 2,2 s). */
class Toast final : public juce::Component, private juce::Timer
{
public:
    Toast();

    void show (const juce::String& message);
    void updatePosition();

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    static constexpr int shadowMargin = 30;
    juce::String message;
};
} // namespace sis
