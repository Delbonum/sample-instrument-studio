#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace sis::dsp
{
/** Einstellungen des Kompressors in den üblichen Einheiten. */
struct CompressorSettings
{
    float thresholdDb = -18.0f;   // ab hier wird geregelt
    float ratio = 4.0f;           // 4 bedeutet 4:1
    float attackMs = 10.0f;
    float releaseMs = 120.0f;
    float makeupDb = 0.0f;

    bool operator== (const CompressorSettings& other) const noexcept
    {
        return juce::approximatelyEqual (thresholdDb, other.thresholdDb)
            && juce::approximatelyEqual (ratio, other.ratio)
            && juce::approximatelyEqual (attackMs, other.attackMs)
            && juce::approximatelyEqual (releaseMs, other.releaseMs)
            && juce::approximatelyEqual (makeupDb, other.makeupDb);
    }

    bool operator!= (const CompressorSettings& other) const noexcept { return ! (*this == other); }
};

/** Kompressor mit gekoppelter Stereo-Regelung.

    Beide Kanäle werden mit derselben Verstärkung bearbeitet – sonst wandert das Stereobild,
    sobald nur eine Seite laut wird. Der Detektor arbeitet auf dem Spitzenwert mit
    getrennten Zeiten für Ansprechen und Loslassen. */
class Compressor
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Rechnet nur ein paar Koeffizienten aus, fordert keinen Speicher an. */
    void setSettings (const CompressorSettings&) noexcept;
    CompressorSettings getSettings() const noexcept { return settings; }

    void process (juce::AudioBuffer<float>&);
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Stärkste Pegelabsenkung des letzten Blocks in dB (negativ). */
    float getGainReductionDb() const noexcept { return gainReductionDb.load (std::memory_order_relaxed); }

private:
    void updateCoefficients() noexcept;

    CompressorSettings settings;
    double currentSampleRate = 44100.0;

    float envelope = 0.0f;        // verfolgter Pegel, linear
    float attackCoefficient = 0.0f;
    float releaseCoefficient = 0.0f;
    std::atomic<float> gainReductionDb { 0.0f };
};
} // namespace sis::dsp
