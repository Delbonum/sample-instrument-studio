#include "Saturator.h"

#include <cmath>

namespace sis::dsp
{
void Saturator::prepare (double, int)
{
    setSettings (settings);
}

void Saturator::reset()
{
    // Die Kennlinie hat kein Gedächtnis - nichts zurückzusetzen.
}

void Saturator::setSettings (const SaturationSettings& newSettings) noexcept
{
    settings = newSettings;

    drive = juce::jlimit (1.0f, 40.0f, juce::Decibels::decibelsToGain (settings.driveDb));
    normalise = 1.0f / std::tanh (drive);
    outputGain = juce::Decibels::decibelsToGain (settings.outputDb);
}

void Saturator::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const float wet = juce::jlimit (0.0f, 1.0f, settings.mix);
    const float dry = 1.0f - wet;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = channels[channel];

        for (int i = 0; i < numSamples; ++i)
        {
            const float in = data[i];
            const float shaped = std::tanh (drive * in) * normalise;
            data[i] = (dry * in + wet * shaped) * outputGain;
        }
    }
}
} // namespace sis::dsp
