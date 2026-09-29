#include "Chorus.h"

#include <cmath>

namespace sis::dsp
{
void Chorus::prepare (double sampleRate, int numChannels)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    // Platz für den größten Wert plus eine Stützstelle zum Interpolieren
    lineLength = (int) std::ceil ((baseDelayMs + maxSweepMs) * 0.001 * currentSampleRate) + 4;

    lines.assign ((size_t) juce::jmax (1, numChannels), std::vector<float> ((size_t) lineLength, 0.0f));
    reset();
}

void Chorus::reset()
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);

    writeIndex = 0;
    phase = 0.0;
}

void Chorus::setSettings (const ChorusSettings& newSettings) noexcept
{
    settings = newSettings;
}

float Chorus::readInterpolated (int channel, double delaySamples) const noexcept
{
    const auto& line = lines[(size_t) channel];

    double readPosition = (double) writeIndex - delaySamples;

    while (readPosition < 0.0)
        readPosition += (double) lineLength;

    const int index = (int) readPosition;
    const float fraction = (float) (readPosition - (double) index);

    const float a = line[(size_t) (index % lineLength)];
    const float b = line[(size_t) ((index + 1) % lineLength)];

    return a + fraction * (b - a);
}

void Chorus::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (lines.empty() || lineLength <= 0 || numChannels <= 0)
        return;

    const int usableChannels = juce::jmin (numChannels, (int) lines.size());

    const float wet = juce::jlimit (0.0f, 1.0f, settings.mix);
    const float dry = 1.0f - wet;
    const double depth = juce::jlimit (0.0f, 1.0f, settings.depth);
    const double width = juce::jlimit (0.0f, 1.0f, settings.width);

    const double baseSamples = baseDelayMs * 0.001 * currentSampleRate;
    const double sweepSamples = maxSweepMs * 0.001 * currentSampleRate * depth;
    const double phaseStep = juce::jlimit (0.01f, 20.0f, settings.rateHz) / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < usableChannels; ++channel)
            lines[(size_t) channel][(size_t) writeIndex] = channels[channel][i];

        for (int channel = 0; channel < usableChannels; ++channel)
        {
            // Der zweite Kanal läuft um bis zu eine halbe Schwingung versetzt
            const double channelPhase = phase + (channel == 0 ? 0.0 : 0.5 * width);
            const double modulation = std::sin (2.0 * juce::MathConstants<double>::pi * channelPhase);
            const double delaySamples = baseSamples + sweepSamples * 0.5 * (1.0 + modulation);

            const float delayed = readInterpolated (channel, delaySamples);
            channels[channel][i] = dry * channels[channel][i] + wet * delayed;
        }

        writeIndex = (writeIndex + 1) % lineLength;

        phase += phaseStep;
        if (phase >= 1.0)
            phase -= 1.0;
    }
}
} // namespace sis::dsp
