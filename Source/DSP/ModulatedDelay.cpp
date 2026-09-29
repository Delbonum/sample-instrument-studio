#include "ModulatedDelay.h"

#include <cmath>

namespace sis::dsp
{
namespace
{
    constexpr double flangerBaseMs = 0.6;
    constexpr double flangerSweepMs = 6.0;

    // Das Vibrato schwingt um eine feste Mitte, damit die Tonhöhe nach oben wie nach
    // unten gleich weit geht
    constexpr double vibratoCentreMs = 4.5;
    constexpr double vibratoSweepMs = 3.5;
}

void ModulatedDelay::prepare (double sampleRate, int numChannels)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lineLength = (int) std::ceil (maxDelayMs * 0.001 * currentSampleRate) + 4;

    lines.assign ((size_t) juce::jlimit (1, maxChannels, numChannels),
                  std::vector<float> ((size_t) lineLength, 0.0f));

    lfo.prepare (currentSampleRate);
    reset();
}

void ModulatedDelay::reset()
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);

    feedbackState.fill (0.0f);
    writeIndex = 0;
    lfo.reset();
}

void ModulatedDelay::setSettings (const ModulatedDelaySettings& newSettings) noexcept
{
    settings = newSettings;
    lfo.setRate (settings.rateHz);
}

float ModulatedDelay::readInterpolated (int channel, double delaySamples) const noexcept
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

void ModulatedDelay::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (lines.empty() || lineLength <= 0 || numChannels <= 0)
        return;

    const int usableChannels = juce::jmin (numChannels, (int) lines.size());

    const bool isVibrato = settings.mode == DelayMode::vibrato;
    const double depth = juce::jlimit (0.0f, 1.0f, settings.depth);

    // Das Vibrato ist immer ganz nass - mit Trockenanteil wäre es ein Chorus
    const float wet = isVibrato ? 1.0f : juce::jlimit (0.0f, 1.0f, settings.mix);
    const float dry = 1.0f - wet;
    const float feedback = isVibrato ? 0.0f : juce::jlimit (0.0f, 0.9f, settings.feedback);

    const double baseSamples = (isVibrato ? vibratoCentreMs : flangerBaseMs) * 0.001 * currentSampleRate;
    const double sweepSamples = (isVibrato ? vibratoSweepMs : flangerSweepMs) * 0.001 * currentSampleRate * depth;

    // Der Flanger darf im Stereobild wandern, das Vibrato nicht: eine Tonhöhe, die links
    // und rechts verschieden schwankt, klingt nach Chorus statt nach Vibrato.
    const double rightOffset = isVibrato ? 0.0 : 0.25;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < usableChannels; ++channel)
            lines[(size_t) channel][(size_t) writeIndex] =
                channels[channel][i] + feedback * feedbackState[(size_t) channel];

        for (int channel = 0; channel < usableChannels; ++channel)
        {
            const double modulation = lfo.value (channel == 0 ? 0.0 : rightOffset);
            const double delaySamples = baseSamples + sweepSamples * 0.5 * (1.0 + modulation);

            const float delayed = readInterpolated (channel, delaySamples);
            feedbackState[(size_t) channel] = delayed;

            channels[channel][i] = dry * channels[channel][i] + wet * delayed;
        }

        writeIndex = (writeIndex + 1) % lineLength;
        lfo.advance();
    }
}
} // namespace sis::dsp
