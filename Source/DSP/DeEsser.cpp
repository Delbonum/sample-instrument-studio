#include "DeEsser.h"

#include <cmath>

namespace sis::dsp
{
namespace
{
    constexpr double detectorAttackMs = 0.5;   // Zischlaute setzen schnell ein
    constexpr float floorDb = -100.0f;
}

void DeEsser::prepare (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    detector.prepare (currentSampleRate);
    setSettings (settings);
    reset();
}

void DeEsser::reset()
{
    lowState.fill (0.0f);
    lowState2.fill (0.0f);
    detector.reset();
}

void DeEsser::setSettings (const DeEsserSettings& newSettings) noexcept
{
    settings = newSettings;

    const double frequency = juce::jlimit (1000.0, currentSampleRate * 0.45,
                                           (double) settings.frequencyHz);

    splitCoefficient = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi
                                                * frequency / currentSampleRate));

    detector.setTimes (detectorAttackMs, juce::jmax (1.0f, settings.releaseMs));
}

void DeEsser::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int usableChannels = juce::jmin (numChannels, maxChannels);

    if (usableChannels <= 0)
        return;

    const float range = juce::jmax (0.0f, settings.rangeDb);

    std::array<float, maxChannels> high {};

    for (int i = 0; i < numSamples; ++i)
    {
        float highestBand = 0.0f;

        for (int channel = 0; channel < usableChannels; ++channel)
        {
            float& first = lowState[(size_t) channel];
            float& second = lowState2[(size_t) channel];
            const float in = channels[channel][i];

            // Zwei Pole hintereinander: steiler, und die Summe bleibt exakt das Eingangssignal
            first += splitCoefficient * (in - first);
            second += splitCoefficient * (first - second);

            high[(size_t) channel] = in - second;

            highestBand = juce::jmax (highestBand, std::abs (high[(size_t) channel]));
        }

        // Geregelt wird nach dem oberen Band, nicht nach dem ganzen Signal
        const float envelope = detector.process (highestBand);
        const float levelDb = juce::Decibels::gainToDecibels (envelope, floorDb);

        float gain = 1.0f;

        if (levelDb > settings.thresholdDb)
        {
            const float overDb = levelDb - settings.thresholdDb;
            gain = juce::Decibels::decibelsToGain (-juce::jmin (range, overDb));
        }

        for (int channel = 0; channel < usableChannels; ++channel)
            channels[channel][i] = lowState2[(size_t) channel] + gain * high[(size_t) channel];
    }
}
} // namespace sis::dsp
