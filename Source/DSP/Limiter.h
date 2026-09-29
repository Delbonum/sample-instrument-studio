#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace sis::dsp
{
struct LimiterSettings
{
    float inputDb = 0.0f;       // Anhebung vor der Begrenzung
    float ceilingDb = -0.3f;    // hier ist Schluss
    float releaseMs = 120.0f;

    bool operator== (const LimiterSettings& other) const noexcept
    {
        return juce::approximatelyEqual (inputDb, other.inputDb)
            && juce::approximatelyEqual (ceilingDb, other.ceilingDb)
            && juce::approximatelyEqual (releaseMs, other.releaseMs);
    }

    bool operator!= (const LimiterSettings& other) const noexcept { return ! (*this == other); }
};

/** Begrenzer mit augenblicklichem Ansprechen.

    Die Verstärkung wird aus dem **aktuellen** Sample berechnet und auf dasselbe Sample
    angewandt. Dadurch gibt es **kein Überschwingen** – die Decke wird nie überschritten,
    auch nicht um ein Sample. Erst das Loslassen ist geglättet.

    Der übliche Weg dorthin ist ein Vorausblick: ein kurzes Verzögern des Signals, damit
    die Regelung den Spitzen zuvorkommen kann. Das ist hier bewusst **nicht** gemacht,
    denn es brächte Latenz – und da jede Spur ihren eigenen Signalweg hat, lägen Spuren
    mit und ohne Begrenzer gegeneinander verschoben. Bei übereinandergelegten Schichten
    gäbe das Kammfiltereffekte, also genau das, was ein Instrument dieser Art nicht
    gebrauchen kann.

    Der Preis dafür: bei tiefen Tönen bewegt sich die Verstärkung innerhalb einer
    Schwingung, was als leichte Verzerrung hörbar wird. Für ein Werkzeug, das Spitzen
    einfängt, ist das der bessere Tausch.

    Beide Kanäle werden mit derselben Verstärkung bearbeitet. */
class Limiter
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const LimiterSettings&) noexcept;
    LimiterSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Stärkste Absenkung des letzten Blocks in dB (negativ). */
    float getGainReductionDb() const noexcept { return gainReductionDb.load (std::memory_order_relaxed); }

private:
    LimiterSettings settings;
    double currentSampleRate = 44100.0;

    float inputGain = 1.0f;
    float ceiling = 0.97f;
    float releaseCoefficient = 0.0f;
    float currentGain = 1.0f;

    std::atomic<float> gainReductionDb { 0.0f };
};
} // namespace sis::dsp
