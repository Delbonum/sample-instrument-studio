#pragma once

#include <JuceHeader.h>

#include "../DSP/Equalizer.h"

namespace sis
{
/** Eigenständiger Equalizer: dasselbe Filter wie in der Effektkette des Studios,
    als VST3-Effekt und Standalone-Programm für beliebige DAWs. */
class EqualizerProcessor final : public juce::AudioProcessor
{
public:
    EqualizerProcessor();
    ~EqualizerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SIS Equalizer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    /** Aktuelle Einstellung eines Bandes – für die Kurve in der Oberfläche. */
    dsp::BandSettings getBandSettings (int index) const;
    double getMagnitudeAt (double frequency) const;
    float getOutputLevel() const noexcept { return outputLevel.load (std::memory_order_relaxed); }

    static juce::String parameterId (int band, const juce::String& what);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pullParametersIntoFilter();

    juce::AudioProcessorValueTreeState state;
    dsp::Equalizer equalizer;
    dsp::Equalizer uiEqualizer;      // nur für die Kurve, vom Message-Thread benutzt
    juce::SmoothedValue<float> outputGain;
    std::atomic<float> outputLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqualizerProcessor)
};
} // namespace sis
