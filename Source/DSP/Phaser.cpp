#include "Phaser.h"

#include <cmath>

namespace sis::dsp
{
namespace
{
    constexpr double lowestHz = 200.0;
    constexpr double highestHz = 2000.0;
}

void Phaser::prepare (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lfo.prepare (currentSampleRate);
    reset();
}

void Phaser::reset()
{
    for (auto& channel : stages)
        for (auto& stage : channel)
            stage = {};

    feedbackState.fill (0.0f);
    lfo.reset();
}

void Phaser::setSettings (const PhaserSettings& newSettings) noexcept
{
    settings = newSettings;
    lfo.setRate (settings.rateHz);
}

float Phaser::coefficientFor (double frequency) const noexcept
{
    /* Allpass erster Ordnung: H(z) = (a + z⁻¹) / (1 + a z⁻¹), Pol bei z = -a.

       Das Vorzeichen ist hier das Ganze: damit eine **tiefe** Eckfrequenz den Pol in die
       Nähe von z = +1 legt, muss a gegen -1 gehen. Andersherum landet der Pol nahe der
       halben Abtastrate, und unten herum passiert nichts mehr. */
    const double limited = juce::jlimit (20.0, currentSampleRate * 0.45, frequency);
    const double t = std::tan (juce::MathConstants<double>::pi * limited / currentSampleRate);

    return (float) ((t - 1.0) / (t + 1.0));
}

void Phaser::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int usableChannels = juce::jmin (numChannels, maxChannels);

    if (usableChannels <= 0)
        return;

    const float wet = juce::jlimit (0.0f, 1.0f, settings.mix);
    const float dry = 1.0f - wet;
    const float feedback = juce::jlimit (0.0f, 0.9f, settings.feedback);
    const double depth = juce::jlimit (0.0f, 1.0f, settings.depth);

    for (int i = 0; i < numSamples; ++i)
    {
        // Eckfrequenz logarithmisch wandern lassen - linear klänge die untere Hälfte
        // des Wegs wie ein Stillstand
        const double modulation = 0.5 * (1.0 + lfo.value()) * depth;
        const double frequency = lowestHz * std::pow (highestHz / lowestHz, modulation);
        const float a = coefficientFor (frequency);

        for (int channel = 0; channel < usableChannels; ++channel)
        {
            const float in = channels[channel][i];
            float value = in + feedback * feedbackState[(size_t) channel];

            for (auto& stage : stages[(size_t) channel])
            {
                const float input = value;
                const float output = a * input + stage.lastInput - a * stage.lastOutput;

                stage.lastInput = input;
                stage.lastOutput = output;
                value = output;
            }

            feedbackState[(size_t) channel] = value;
            channels[channel][i] = dry * in + wet * value;
        }

        lfo.advance();
    }
}
} // namespace sis::dsp
