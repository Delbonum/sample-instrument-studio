#include "BitCrusher.h"

#include <cmath>

namespace sis::dsp
{
void BitCrusher::prepare (double, int)
{
    reset();
}

void BitCrusher::reset()
{
    heldValue.fill (0.0f);
    holdPhase = 0.0;
}

void BitCrusher::setSettings (const BitCrusherSettings& newSettings) noexcept
{
    settings = newSettings;
}

void BitCrusher::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int usableChannels = juce::jmin (numChannels, maxChannels);

    if (usableChannels <= 0)
        return;

    const float wet = juce::jlimit (0.0f, 1.0f, settings.mix);
    const float dry = 1.0f - wet;

    // Stufen der Quantisierung: bei 8 Bit sind das 128 Schritte je Richtung
    const float bits = juce::jlimit (2.0f, 16.0f, settings.bits);
    const float levels = std::pow (2.0f, bits - 1.0f);

    const double hold = juce::jlimit (1.0, 64.0, (double) settings.downsample);

    for (int i = 0; i < numSamples; ++i)
    {
        // Erst wenn der Zähler eine ganze Haltezeit weiter ist, wird neu abgetastet
        holdPhase += 1.0;
        const bool takeNewSample = holdPhase >= hold;

        if (takeNewSample)
            holdPhase -= hold;

        for (int channel = 0; channel < usableChannels; ++channel)
        {
            const float in = channels[channel][i];

            if (takeNewSample)
            {
                const float clamped = juce::jlimit (-1.0f, 1.0f, in);
                heldValue[(size_t) channel] = std::round (clamped * levels) / levels;
            }

            channels[channel][i] = dry * in + wet * heldValue[(size_t) channel];
        }
    }
}
} // namespace sis::dsp
