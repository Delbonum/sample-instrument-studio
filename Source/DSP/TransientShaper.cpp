#include "TransientShaper.h"

#include <cmath>

namespace sis::dsp
{
namespace
{
    constexpr float minimumLevel = 1.0e-5f;   // darunter wird nicht geregelt
    constexpr float maximumGainDb = 18.0f;
    constexpr float sensitivity = 2.0f;       // wie kräftig der Abstand auf die Verstärkung wirkt
}

float TransientShaper::coefficientFor (double milliseconds, double sampleRate) noexcept
{
    const double samples = juce::jmax (1.0, milliseconds * 0.001 * sampleRate);
    return (float) std::exp (-1.0 / samples);
}

void TransientShaper::prepare (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    // Schnell folgt dem Anschlag, langsam dem Verlauf; der Abstand ist das Nutzsignal
    fastAttack  = coefficientFor (0.5, currentSampleRate);
    fastRelease = coefficientFor (40.0, currentSampleRate);
    slowAttack  = coefficientFor (25.0, currentSampleRate);
    slowRelease = coefficientFor (250.0, currentSampleRate);

    reset();
}

void TransientShaper::reset()
{
    fastEnvelope = 0.0f;
    slowEnvelope = 0.0f;
}

void TransientShaper::setSettings (const TransientSettings& newSettings) noexcept
{
    settings = newSettings;
}

void TransientShaper::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (numChannels <= 0)
        return;

    const float attackAmount = juce::jlimit (-1.0f, 1.0f, settings.attack);
    const float sustainAmount = juce::jlimit (-1.0f, 1.0f, settings.sustain);

    for (int i = 0; i < numSamples; ++i)
    {
        // Gekoppelt: der lauteste Kanal bestimmt die Regelung
        float level = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            level = juce::jmax (level, std::abs (channels[channel][i]));

        fastEnvelope = level > fastEnvelope ? fastAttack * (fastEnvelope - level) + level
                                            : fastRelease * (fastEnvelope - level) + level;
        slowEnvelope = level > slowEnvelope ? slowAttack * (slowEnvelope - level) + level
                                            : slowRelease * (slowEnvelope - level) + level;

        float gain = 1.0f;

        if (slowEnvelope > minimumLevel && fastEnvelope > minimumLevel)
        {
            // Positiv, solange der Pegel steigt; negativ, während er abfällt
            const float distanceDb = juce::Decibels::gainToDecibels (fastEnvelope / slowEnvelope, -60.0f);

            const float gainDb = distanceDb > 0.0f ? attackAmount * distanceDb * sensitivity
                                                   : sustainAmount * (-distanceDb) * sensitivity;

            gain = juce::Decibels::decibelsToGain (juce::jlimit (-maximumGainDb, maximumGainDb, gainDb));
        }

        for (int channel = 0; channel < numChannels; ++channel)
            channels[channel][i] *= gain;
    }
}
} // namespace sis::dsp
