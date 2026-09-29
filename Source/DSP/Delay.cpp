#include "Delay.h"

#include <cmath>

namespace sis::dsp
{
namespace
{
    constexpr double minTimeMs = 10.0;
    constexpr double smoothingMs = 120.0;   // wie schnell die Zeit nachgeführt wird
}

void Delay::prepare (double sampleRate, int numChannels)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lineLength = (int) std::ceil (maxTimeSeconds * currentSampleRate) + 4;

    lines.assign ((size_t) juce::jlimit (1, maxChannels, numChannels),
                  std::vector<float> ((size_t) lineLength, 0.0f));

    smoothingCoefficient = (float) (1.0 - std::exp (-1.0 / (smoothingMs * 0.001 * currentSampleRate)));

    setSettings (settings);
    reset();
}

void Delay::reset()
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);

    dampingState.fill (0.0f);
    writeIndex = 0;
    snapToTarget = true;
}

void Delay::setSettings (const DelaySettings& newSettings) noexcept
{
    settings = newSettings;

    const double time = juce::jlimit (minTimeMs, maxTimeSeconds * 1000.0, (double) settings.timeMs);
    targetDelaySamples = juce::jlimit (1.0, (double) lineLength - 4.0, time * 0.001 * currentSampleRate);

    /* Dämpfung als einpoliger Tiefpass: ganz links bleibt alles stehen, ganz rechts
       rutscht die Eckfrequenz auf 700 Hz herunter. */
    const double frequency = 18000.0 * std::pow (700.0 / 18000.0,
                                                 (double) juce::jlimit (0.0f, 1.0f, settings.damping));

    dampingCoefficient = frequency >= currentSampleRate * 0.49
                             ? 1.0f
                             : (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi
                                                        * frequency / currentSampleRate));
}

float Delay::readInterpolated (int channel, double delaySamples) const noexcept
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

void Delay::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (lines.empty() || lineLength <= 0 || numChannels <= 0)
        return;

    const int usableChannels = juce::jmin (numChannels, (int) lines.size());

    const float wet = juce::jlimit (0.0f, 1.0f, settings.mix);
    const float dry = 1.0f - wet;
    const float feedback = juce::jlimit (0.0f, 0.95f, settings.feedback);
    const float pingPong = juce::jlimit (0.0f, 1.0f, settings.pingPong);

    if (snapToTarget)
    {
        currentDelaySamples = targetDelaySamples;
        snapToTarget = false;
    }

    std::array<float, maxChannels> delayed {};

    for (int i = 0; i < numSamples; ++i)
    {
        currentDelaySamples += (double) smoothingCoefficient * (targetDelaySamples - currentDelaySamples);

        for (int channel = 0; channel < usableChannels; ++channel)
            delayed[(size_t) channel] = readInterpolated (channel, currentDelaySamples);

        // Dämpfung im Rückweg: jede Wiederholung wird eine Stufe dunkler
        for (int channel = 0; channel < usableChannels; ++channel)
        {
            float& state = dampingState[(size_t) channel];
            state += dampingCoefficient * (delayed[(size_t) channel] - state);
        }

        for (int channel = 0; channel < usableChannels; ++channel)
        {
            const int other = usableChannels > 1 ? 1 - channel : channel;

            // Bei vollem Ping-Pong speist jeder Kanal den anderen
            const float returned = (1.0f - pingPong) * dampingState[(size_t) channel]
                                   + pingPong * dampingState[(size_t) other];

            /* Damit sich die Seiten abwechseln, darf das Trockensignal bei vollem
               Ping-Pong nur noch links hinein - sonst starten beide gleichzeitig und
               es bliebe beim gewöhnlichen Echo. */
            const float directShare = channel == 0 ? 1.0f : 1.0f - pingPong;

            lines[(size_t) channel][(size_t) writeIndex] =
                directShare * channels[channel][i] + feedback * returned;
        }

        for (int channel = 0; channel < usableChannels; ++channel)
            channels[channel][i] = dry * channels[channel][i] + wet * delayed[(size_t) channel];

        writeIndex = (writeIndex + 1) % lineLength;
    }
}
} // namespace sis::dsp
