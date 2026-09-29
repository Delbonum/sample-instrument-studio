#include "Tremolo.h"

#include <cmath>

namespace sis::dsp
{
void Tremolo::prepare (double sampleRate, int)
{
    lfo.prepare (sampleRate);
    setSettings (settings);
    reset();
}

void Tremolo::reset()
{
    lfo.reset();
}

void Tremolo::setSettings (const TremoloSettings& newSettings) noexcept
{
    settings = newSettings;
    lfo.setRate (settings.rateHz);

    // Je steiler die Kennlinie, desto rechteckiger die Form; normiert, damit der Hub
    // gleich bleibt und allein die Form sich ändert
    sharpness = 1.0f + juce::jlimit (0.0f, 1.0f, settings.shape) * 11.0f;
    normalise = 1.0f / std::tanh (sharpness);
}

float Tremolo::shapedValue (double offsetTurns) const noexcept
{
    return std::tanh (sharpness * lfo.value (offsetTurns)) * normalise;
}

void Tremolo::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int usableChannels = juce::jmin (numChannels, maxChannels);

    if (usableChannels <= 0)
        return;

    const float depth = juce::jlimit (0.0f, 1.0f, settings.depth);
    const double rightOffset = juce::jlimit (0.0f, 1.0f, settings.width) * 0.5;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < usableChannels; ++channel)
        {
            const float wave = shapedValue (channel == 0 ? 0.0 : rightOffset);

            // 1 am oberen Umkehrpunkt, 1 - depth am unteren
            const float gain = 1.0f - depth * 0.5f * (1.0f - wave);

            channels[channel][i] *= gain;
        }

        lfo.advance();
    }
}
} // namespace sis::dsp
