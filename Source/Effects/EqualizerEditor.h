#pragma once

#include <JuceHeader.h>

#include "../UI/StudioLookAndFeel.h"
#include "../UI/Widgets.h"
#include "EqualizerProcessor.h"

namespace sis
{
/** Ein Band: Schalter, Filterart und die drei Regler. */
class BandStrip final : public juce::Component
{
public:
    BandStrip (EqualizerProcessor&, int bandIndex, const juce::String& title, juce::Colour);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    struct Row
    {
        juce::String label;
        juce::String suffix;
        ValueBar bar;
        juce::Rectangle<int> labelArea, valueArea;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        juce::RangedAudioParameter* parameter = nullptr;
    };

    void setUpRow (Row&, const juce::String& parameterName, const juce::String& label,
                   const juce::String& suffix);
    juce::String formatValue (const Row&) const;

    EqualizerProcessor& processor;
    int index;
    juce::String title;
    juce::Colour colour;

    FlatButton powerButton { "AN" };
    juce::ComboBox typeBox;
    std::array<Row, 3> rows;   // Frequenz, Anhebung, Güte

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;

    juce::Rectangle<int> titleArea;
};

/** Fenster des eigenständigen Equalizers. */
class EqualizerEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EqualizerEditor (EqualizerProcessor&);
    ~EqualizerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::Path createResponsePath (juce::Rectangle<float>) const;

    EqualizerProcessor& processor;
    StudioLookAndFeel lookAndFeel;
    std::array<std::unique_ptr<BandStrip>, dsp::Equalizer::numBands> strips;

    ValueBar outputBar;
    std::unique_ptr<juce::ParameterAttachment> outputAttachment;

    juce::Rectangle<int> headerArea, curveArea, footerArea, meterArea, outputLabelArea, outputValueArea;
    float lastLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqualizerEditor)
};
} // namespace sis
