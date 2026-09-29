#include "ChannelStrip.h"

namespace sis::dsp
{
void ChannelStrip::prepare (double sampleRate, int numChannels, int maximumBlockSize)
{
    gate.prepare (sampleRate, numChannels);
    equalizer.prepare (sampleRate, numChannels, maximumBlockSize);
    compressor.prepare (sampleRate, numChannels);
    reset();
}

void ChannelStrip::reset()
{
    gate.reset();
    equalizer.reset();
    compressor.reset();
}

void ChannelStrip::setSettings (const ChannelStripSettings& newSettings) noexcept
{
    settings = newSettings;

    if (settings.gateActive)
        gate.setSettings (settings.gate);

    // Fertige Koeffizienten übernehmen - keine Berechnung, keine Speicheranforderung
    for (int band = 0; band < Equalizer::numBands; ++band)
        equalizer.setPreparedBand (band, settings.equalizer[(size_t) band],
                                   settings.bandAudible[(size_t) band]);

    if (settings.compressorActive)
        compressor.setSettings (settings.compressor);
}

void ChannelStrip::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    // Die Reihenfolge ist der Sinn der Sache - siehe ChannelStrip.h
    if (settings.gateActive)
        gate.process (channels, numChannels, numSamples);

    equalizer.process (channels, numChannels, numSamples);

    if (settings.compressorActive)
        compressor.process (channels, numChannels, numSamples);

    if (! juce::approximatelyEqual (settings.outputGain, 1.0f))
        for (int channel = 0; channel < numChannels; ++channel)
            juce::FloatVectorOperations::multiply (channels[channel], settings.outputGain, numSamples);
}
} // namespace sis::dsp
