#pragma once

#include <JuceHeader.h>
#include <array>

#include "EnvelopeFollower.h"
#include "Lfo.h"

namespace sis::dsp
{
/** Was die Eckfrequenz bewegt. */
enum class SweepSource
{
    envelope,   // der Anschlag – lauter heißt offener
    lfo         // gleichmäßig zwischen zwei Frequenzen hin und her
};

struct SweptFilterSettings
{
    SweepSource source = SweepSource::envelope;

    float lowHz = 300.0f;        // unteres Ende des Intervalls
    float highHz = 2400.0f;      // oberes Ende
    float resonance = 3.0f;      // Güte des Bandes

    float sensitivity = 0.7f;    // nur Hüllkurve: wie weit der Anschlag zieht
    float attackMs = 10.0f;      // nur Hüllkurve
    float releaseMs = 150.0f;    // nur Hüllkurve
    float rateHz = 1.2f;         // nur Oszillator

    float mix = 1.0f;

    bool operator== (const SweptFilterSettings& other) const noexcept
    {
        return source == other.source
            && juce::approximatelyEqual (lowHz, other.lowHz)
            && juce::approximatelyEqual (highHz, other.highHz)
            && juce::approximatelyEqual (resonance, other.resonance)
            && juce::approximatelyEqual (sensitivity, other.sensitivity)
            && juce::approximatelyEqual (attackMs, other.attackMs)
            && juce::approximatelyEqual (releaseMs, other.releaseMs)
            && juce::approximatelyEqual (rateHz, other.rateHz)
            && juce::approximatelyEqual (mix, other.mix);
    }

    bool operator!= (const SweptFilterSettings& other) const noexcept { return ! (*this == other); }
};

/** Resonantes Bandfilter, dessen Eckfrequenz wandert – das Rechenwerk hinter
    Envelope Filter und Auto-Wah.

    Der Unterschied zwischen beiden steckt allein darin, **was** die Frequenz bewegt:
    beim Envelope Filter der Anschlag (lauter heißt offener, das Instrument antwortet
    auf das Spiel), beim Auto-Wah ein Oszillator, der gleichmäßig zwischen zwei
    Frequenzen hin und her läuft. Deshalb eine Klasse mit einer Quelle statt zweier
    fast gleicher.

    Gefiltert wird mit einem **Zustandsvariablenfilter** und nicht mit den Biquads des
    Equalizers. Deren Koeffizienten entstehen bewusst außerhalb des Audio-Threads – hier
    wandert die Frequenz aber im Takt des Signals, sie müssen also laufend neu entstehen.
    Der Zustandsvariablenfilter ist dafür gebaut: zwei Additionen je Abtastwert, und den
    einen Sinus für die Frequenz braucht er nur **alle 16 Abtastwerte** neu. Bei 48 kHz
    sind das 3000 Stützstellen je Sekunde, für eine Filterfahrt weit mehr als genug.

    Ausgegeben wird das **Bandsignal**, nicht das Tiefpasssignal: das ist der Klang, den
    man von einem Wah kennt. Wer den Körper behalten will, mischt über MENGE trocken dazu. */
class SweptFilter
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int coefficientInterval = 16;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const SweptFilterSettings&) noexcept;
    SweptFilterSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    void updateCoefficients (double cutoffHz) noexcept;

    SweptFilterSettings settings;
    double currentSampleRate = 44100.0;

    EnvelopeFollower follower;
    Lfo lfo;

    struct State { float low = 0.0f; float band = 0.0f; };
    std::array<State, maxChannels> states {};

    float frequencyCoefficient = 0.1f;   // „f“ im Zustandsvariablenfilter
    float dampingCoefficient = 0.5f;     // „q“, also 1 / Güte
    int samplesUntilUpdate = 0;
};
} // namespace sis::dsp
