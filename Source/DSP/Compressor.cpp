#include "Compressor.h"

namespace sis::dsp
{
namespace
{
    /** Ein-Pol-Koeffizient für eine Zeitkonstante in Millisekunden. */
    float timeToCoefficient (float milliseconds, double sampleRate)
    {
        const double seconds = juce::jmax (0.0001, (double) milliseconds * 0.001);
        return (float) std::exp (-1.0 / (seconds * sampleRate));
    }
}

void Compressor::prepare (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateCoefficients();
    reset();
}

void Compressor::reset()
{
    envelope = 0.0f;
    gainReductionDb.store (0.0f, std::memory_order_relaxed);
}

void Compressor::updateCoefficients() noexcept
{
    attackCoefficient = timeToCoefficient (settings.attackMs, currentSampleRate);
    releaseCoefficient = timeToCoefficient (settings.releaseMs, currentSampleRate);
}

void Compressor::setSettings (const CompressorSettings& newSettings) noexcept
{
    if (settings == newSettings)
        return;

    settings = newSettings;
    settings.ratio = juce::jmax (1.0f, settings.ratio);
    updateCoefficients();
}

void Compressor::process (juce::AudioBuffer<float>& buffer)
{
    process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

void Compressor::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (numSamples <= 0 || numChannels <= 0)
        return;

    const float slope = 1.0f - 1.0f / juce::jmax (1.0f, settings.ratio);
    const float makeup = juce::Decibels::decibelsToGain (settings.makeupDb);
    float strongestReduction = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Beide Kanäle steuern denselben Detektor
        float level = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            level = juce::jmax (level, std::abs (channels[channel][i]));

        const float coefficient = level > envelope ? attackCoefficient : releaseCoefficient;
        envelope = coefficient * (envelope - level) + level;

        float reductionDb = 0.0f;

        if (envelope > 1.0e-6f)
        {
            const float levelDb = juce::Decibels::gainToDecibels (envelope);
            const float overshoot = levelDb - settings.thresholdDb;

            if (overshoot > 0.0f)
                reductionDb = -overshoot * slope;
        }

        strongestReduction = juce::jmin (strongestReduction, reductionDb);
        const float gain = juce::Decibels::decibelsToGain (reductionDb) * makeup;

        for (int channel = 0; channel < numChannels; ++channel)
            channels[channel][i] *= gain;
    }

    gainReductionDb.store (strongestReduction, std::memory_order_relaxed);
}
} // namespace sis::dsp
