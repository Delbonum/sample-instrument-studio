#include "Drive.h"

#include <cmath>

namespace sis::dsp
{
namespace
{
    constexpr float maxOffset = 0.3f;   // darüber wird das Tastverhältnis grotesk
    /* Tief angesetzt: ein Sperrfilter laesst das Dach eines Rechtecks absacken, und zwar
       umso staerker, je hoeher seine Grenzfrequenz liegt. Bei 5 Hz bleibt davon ueber eine
       Halbwelle bei 400 Hz nur noch ein Prozentbruchteil uebrig. */
    constexpr double dcBlockerHz = 5.0;
}

float Drive::softClip (float x) noexcept
{
    const float magnitude = std::abs (x);

    if (magnitude < 1.0f / 3.0f)
        return 2.0f * x;

    if (magnitude < 2.0f / 3.0f)
    {
        const float t = 2.0f - 3.0f * magnitude;
        return std::copysign ((3.0f - t * t) / 3.0f, x);
    }

    return std::copysign (1.0f, x);
}

void Drive::prepare (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateCoefficients();
    reset();
}

void Drive::reset()
{
    toneState.fill (0.0f);
    dcLastInput.fill (0.0f);
    dcLastOutput.fill (0.0f);
}

void Drive::setSettings (const DriveSettings& newSettings) noexcept
{
    if (settings == newSettings)
        return;

    settings = newSettings;
    updateCoefficients();
}

void Drive::updateCoefficients() noexcept
{
    drive = juce::jlimit (1.0f, 256.0f, juce::Decibels::decibelsToGain (settings.driveDb));
    outputGain = juce::Decibels::decibelsToGain (settings.outputDb);

    inputOffset = settings.mode == DriveMode::overdrive
                      ? juce::jlimit (0.0f, 1.0f, settings.character) * maxOffset
                      : 0.0f;

    // Einpoliger Tiefpass: a = 1 - e^(-2π f / fs)
    const double frequency = juce::jlimit (200.0, 20000.0, (double) settings.toneHz);
    const double nyquist = currentSampleRate * 0.5;

    toneCoefficient = frequency >= nyquist * 0.98
                          ? 1.0f   // offen: der Filter tut nichts
                          : (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi
                                                     * frequency / currentSampleRate));

    dcCoefficient = (float) (1.0 - 2.0 * juce::MathConstants<double>::pi
                                   * dcBlockerHz / currentSampleRate);
}

float Drive::shape (float driven) const noexcept
{
    if (settings.mode == DriveMode::overdrive)
        return softClip (driven);

    // Von weich nach hart überblenden: bei voller Kante bleibt ein Rechteck
    const float soft = std::tanh (driven);
    const float hard = juce::jlimit (-1.0f, 1.0f, driven);
    const float edge = juce::jlimit (0.0f, 1.0f, settings.character);

    return soft + edge * (hard - soft);
}

void Drive::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int usableChannels = juce::jmin (numChannels, maxChannels);

    if (usableChannels <= 0)
        return;

    const float wet = juce::jlimit (0.0f, 1.0f, settings.mix);
    const float dry = 1.0f - wet;

    for (int channel = 0; channel < usableChannels; ++channel)
    {
        auto* data = channels[channel];

        float& tone = toneState[(size_t) channel];
        float& lastInput = dcLastInput[(size_t) channel];
        float& lastOutput = dcLastOutput[(size_t) channel];

        for (int i = 0; i < numSamples; ++i)
        {
            const float in = data[i];
            const float shaped = shape (drive * (in + inputOffset));

            // Gleichanteil sperren, den die unsymmetrische Kennlinie erzeugt
            const float blocked = shaped - lastInput + dcCoefficient * lastOutput;
            lastInput = shaped;
            lastOutput = blocked;

            // Klangregler dahinter
            tone += toneCoefficient * (blocked - tone);

            data[i] = (dry * in + wet * tone) * outputGain;
        }
    }
}
} // namespace sis::dsp
