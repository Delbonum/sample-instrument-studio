#pragma once

#include <JuceHeader.h>
#include <vector>

namespace sis::dsp
{
/** Einstellungen des Chorus. */
struct ChorusSettings
{
    float depth = 0.5f;     // 0 … 1, Auslenkung der Verzögerung
    float rateHz = 0.6f;    // Geschwindigkeit der Modulation
    float width = 0.8f;     // 0 = beide Kanäle gleich, 1 = gegenläufig
    float mix = 0.4f;       // 0 = trocken, 1 = ganz bearbeitet

    bool operator== (const ChorusSettings& other) const noexcept
    {
        return juce::approximatelyEqual (depth, other.depth)
            && juce::approximatelyEqual (rateHz, other.rateHz)
            && juce::approximatelyEqual (width, other.width)
            && juce::approximatelyEqual (mix, other.mix);
    }

    bool operator!= (const ChorusSettings& other) const noexcept { return ! (*this == other); }
};

/** Chorus über eine modulierte Verzögerung.

    Je Kanal eine kurze Verzögerungsleitung (Grundwert 12 ms, Auslenkung bis 7 ms), deren
    Länge ein Sinus moduliert; gelesen wird zwischen zwei Stützstellen interpoliert. Die
    Kanäle laufen um `width` gegeneinander versetzt — daher die Breite.

    Der Speicher entsteht in `prepare`; `process` fordert nichts an. */
class Chorus
{
public:
    static constexpr double baseDelayMs = 12.0;
    static constexpr double maxSweepMs = 7.0;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const ChorusSettings&) noexcept;
    ChorusSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    float readInterpolated (int channel, double delaySamples) const noexcept;

    ChorusSettings settings;
    double currentSampleRate = 44100.0;

    std::vector<std::vector<float>> lines;   // je Kanal eine Leitung
    int lineLength = 0;
    int writeIndex = 0;
    double phase = 0.0;                      // 0 … 1
};
} // namespace sis::dsp
