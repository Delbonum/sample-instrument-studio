#pragma once

#include <JuceHeader.h>

namespace sis::dsp
{
/** Einstellungen der Sättigung. */
struct SaturationSettings
{
    float driveDb = 8.0f;     // wie weit das Signal in die Kennlinie gefahren wird
    float mix = 1.0f;         // 0 = trocken, 1 = ganz bearbeitet
    float outputDb = 0.0f;    // Ausgleich von Hand

    bool operator== (const SaturationSettings& other) const noexcept
    {
        return juce::approximatelyEqual (driveDb, other.driveDb)
            && juce::approximatelyEqual (mix, other.mix)
            && juce::approximatelyEqual (outputDb, other.outputDb);
    }

    bool operator!= (const SaturationSettings& other) const noexcept { return ! (*this == other); }
};

/** Weiche Sättigung über eine Tangens-hyperbolicus-Kennlinie.

    Die Kennlinie wird auf sich selbst normiert (`tanh(g·x) / tanh(g)`), damit ein
    voll ausgesteuertes Signal voll ausgesteuert bleibt und allein die Form sich ändert —
    sonst wäre jede Drehung am Regler vor allem eine Lautstärkeänderung, und man hörte
    nicht, was der Effekt tut. */
class Saturator
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const SaturationSettings&) noexcept;
    SaturationSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    SaturationSettings settings;
    float drive = 2.5f;          // linearer Faktor aus driveDb
    float normalise = 1.0f;      // 1 / tanh(drive)
    float outputGain = 1.0f;
};
} // namespace sis::dsp
