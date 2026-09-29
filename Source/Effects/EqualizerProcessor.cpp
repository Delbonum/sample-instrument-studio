#include "EqualizerProcessor.h"
#include "EqualizerEditor.h"

namespace sis
{
namespace
{
    constexpr int numBands = dsp::Equalizer::numBands;

    juce::String bandName (int index)
    {
        switch (index)
        {
            case 0:  return "Tiefen";
            case 1:  return "Tiefmitten";
            case 2:  return "Hochmitten";
            default: return juce::String::fromUTF8 ("Höhen");
        }
    }

    juce::StringArray bandTypeNames()
    {
        return { "Kuhschwanz tief", "Glocke", "Kuhschwanz hoch", "Hochpass", "Tiefpass" };
    }
} // namespace

juce::String EqualizerProcessor::parameterId (int band, const juce::String& what)
{
    return "band" + juce::String (band + 1) + "_" + what;
}

EqualizerProcessor::EqualizerProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SISEQ", createLayout())
{
    pullParametersIntoFilter();
    uiEqualizer.prepare (44100.0, 2, 512);
}

juce::AudioProcessorValueTreeState::ParameterLayout EqualizerProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Frequenzen logarithmisch, damit der Regler über den ganzen Bereich brauchbar bleibt
    juce::NormalisableRange<float> frequencyRange (dsp::Equalizer::minFrequency, dsp::Equalizer::maxFrequency, 1.0f);
    frequencyRange.setSkewForCentre (1000.0f);

    for (int band = 0; band < numBands; ++band)
    {
        const auto defaults = dsp::Equalizer::getDefaultBand (band);
        const auto prefix = bandName (band) + " ";

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { parameterId (band, "on"), 1 }, prefix + "an", true));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { parameterId (band, "type"), 1 }, prefix + "Art",
            bandTypeNames(), (int) defaults.type));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { parameterId (band, "freq"), 1 }, prefix + "Frequenz",
            frequencyRange, defaults.frequency,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { parameterId (band, "gain"), 1 }, prefix + "Anhebung",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { parameterId (band, "q"), 1 }, prefix + "Güte"_u,
            juce::NormalisableRange<float> (0.1f, 12.0f, 0.01f, 0.4f), defaults.q));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Ausgang",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

dsp::BandSettings EqualizerProcessor::getBandSettings (int band) const
{
    dsp::BandSettings settings;
    const auto getValue = [this, band] (const juce::String& what)
    {
        const auto* parameter = state.getRawParameterValue (parameterId (band, what));
        return parameter != nullptr ? parameter->load() : 0.0f;
    };

    settings.enabled = getValue ("on") > 0.5f;
    settings.type = (dsp::BandType) juce::jlimit (0, 4, (int) getValue ("type"));
    settings.frequency = getValue ("freq");
    settings.gainDb = getValue ("gain");
    settings.q = getValue ("q");
    return settings;
}

double EqualizerProcessor::getMagnitudeAt (double frequency) const
{
    // uiEqualizer wird nur hier benutzt; die Bänder werden vorher abgeglichen
    auto& mutableSelf = const_cast<EqualizerProcessor&> (*this);

    for (int band = 0; band < numBands; ++band)
        mutableSelf.uiEqualizer.setBand (band, getBandSettings (band));

    return mutableSelf.uiEqualizer.getMagnitudeAt (frequency);
}

void EqualizerProcessor::pullParametersIntoFilter()
{
    for (int band = 0; band < numBands; ++band)
        equalizer.setBand (band, getBandSettings (band));
}

void EqualizerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    equalizer.prepare (sampleRate, getTotalNumOutputChannels(), samplesPerBlock);
    uiEqualizer.prepare (sampleRate, 1, samplesPerBlock);
    pullParametersIntoFilter();

    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        state.getRawParameterValue ("output")->load()));
}

bool EqualizerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void EqualizerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    pullParametersIntoFilter();
    equalizer.process (buffer);

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (state.getRawParameterValue ("output")->load()));
    outputGain.applyGain (buffer, buffer.getNumSamples());

    const float peak = buffer.getMagnitude (0, buffer.getNumSamples());
    const float previous = outputLevel.load (std::memory_order_relaxed);
    outputLevel.store (juce::jmax (peak, previous * 0.82f), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* EqualizerProcessor::createEditor()
{
    return new EqualizerEditor (*this);
}

void EqualizerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EqualizerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}
} // namespace sis

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sis::EqualizerProcessor();
}
