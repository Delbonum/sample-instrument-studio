#pragma once

#include <JuceHeader.h>
#include <array>

namespace sis::dsp
{
/** Einstellungen des Bit-Crushers. */
struct BitCrusherSettings
{
    float bits = 8.0f;          // 2 … 16, Auflösung der Stufen
    float downsample = 1.0f;    // 1 = jedes Sample, 8 = nur jedes achte wird neu gelesen
    float mix = 1.0f;

    bool operator== (const BitCrusherSettings& other) const noexcept
    {
        return juce::approximatelyEqual (bits, other.bits)
            && juce::approximatelyEqual (downsample, other.downsample)
            && juce::approximatelyEqual (mix, other.mix);
    }

    bool operator!= (const BitCrusherSettings& other) const noexcept { return ! (*this == other); }
};

/** Bit-Crusher: gröbere Auflösung und gröbere Abtastung.

    Zwei getrennte Wirkungen, die man auch getrennt hören soll — die Stufen der
    Quantisierung rauschen, die gehaltene Abtastung klingt nach Aliasing. Der
    Haltezähler läuft in Bruchteilen, damit sich der Regler stufenlos anfühlt. */
class BitCrusher
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const BitCrusherSettings&) noexcept;
    BitCrusherSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    BitCrusherSettings settings;

    std::array<float, maxChannels> heldValue {};
    double holdPhase = 0.0;
};
} // namespace sis::dsp
