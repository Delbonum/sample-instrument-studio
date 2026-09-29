#include "Limiter.h"

#include <cmath>

namespace sis::dsp
{
void Limiter::prepare (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    setSettings (settings);
    reset();
}

void Limiter::reset()
{
    currentGain = 1.0f;
    gainReductionDb.store (0.0f, std::memory_order_relaxed);
}

void Limiter::setSettings (const LimiterSettings& newSettings) noexcept
{
    settings = newSettings;

    inputGain = juce::Decibels::decibelsToGain (settings.inputDb);
    ceiling = juce::Decibels::decibelsToGain (settings.ceilingDb);

    const double samples = juce::jmax (1.0, (double) settings.releaseMs * 0.001 * currentSampleRate);
    releaseCoefficient = (float) std::exp (-1.0 / samples);
}

void Limiter::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (numChannels <= 0)
        return;

    float lowestGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            channels[channel][i] *= inputGain;
            peak = juce::jmax (peak, std::abs (channels[channel][i]));
        }

        // Was nötig wäre, um genau auf der Decke zu landen
        const float required = peak > ceiling ? ceiling / peak : 1.0f;

        /* Nach unten sofort, nach oben geglättet: so wird die Decke nie überschritten,
           und das Zurückkommen bleibt trotzdem ruhig. */
        currentGain = required < currentGain
                          ? required
                          : releaseCoefficient * (currentGain - required) + required;

        lowestGain = juce::jmin (lowestGain, currentGain);

        for (int channel = 0; channel < numChannels; ++channel)
            channels[channel][i] *= currentGain;
    }

    gainReductionDb.store (juce::Decibels::gainToDecibels (lowestGain, -60.0f),
                           std::memory_order_relaxed);
}
} // namespace sis::dsp
