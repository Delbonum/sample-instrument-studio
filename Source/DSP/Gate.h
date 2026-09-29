#pragma once

#include <JuceHeader.h>

#include "EnvelopeFollower.h"

namespace sis::dsp
{
struct GateSettings
{
    float thresholdDb = -40.0f;
    float ratio = 10.0f;       // 2 heißt: 1 dB unter der Schwelle werden 2
    float rangeDb = 60.0f;     // wie weit höchstens abgesenkt wird
    float attackMs = 2.0f;     // wie schnell es aufgeht
    float holdMs = 30.0f;      // wie lange es offen bleibt, nachdem der Pegel fiel
    float releaseMs = 120.0f;  // wie schnell es zugeht

    bool operator== (const GateSettings& other) const noexcept
    {
        return juce::approximatelyEqual (thresholdDb, other.thresholdDb)
            && juce::approximatelyEqual (ratio, other.ratio)
            && juce::approximatelyEqual (rangeDb, other.rangeDb)
            && juce::approximatelyEqual (attackMs, other.attackMs)
            && juce::approximatelyEqual (holdMs, other.holdMs)
            && juce::approximatelyEqual (releaseMs, other.releaseMs);
    }

    bool operator!= (const GateSettings& other) const noexcept { return ! (*this == other); }
};

/** Absenkung unterhalb einer Schwelle – das Rechenwerk hinter Noise Gate und Expander.

    Beide sind dieselbe Rechnung mit anderen Reglern, deshalb gibt es hier **keinen
    Modus-Schalter**: das Gate ist ein Expander mit steilem Verhältnis und Haltezeit,
    der Expander ein Gate ohne Haltezeit und mit sanftem Verhältnis. Welches von beiden
    gemeint ist, entscheidet allein, was der Renderplan einstellt.

    Die **Haltezeit** ist der Grund, warum ein Gate überhaupt brauchbar ist: ohne sie
    flattert es bei jedem Durchgang durch die Schwelle, besonders bei tiefen Tönen, deren
    Hüllkurve mit der Grundfrequenz schwankt.

    Beide Kanäle werden mit **derselben** Verstärkung bearbeitet, sonst wandert das
    Stereobild, sobald nur eine Seite unter die Schwelle fällt. */
class Gate
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const GateSettings&) noexcept;
    GateSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    GateSettings settings;
    double currentSampleRate = 44100.0;

    EnvelopeFollower detector;   // folgt dem Eingangspegel
    EnvelopeFollower smoother;   // glättet die Verstärkung: aufgehen = Attack, zugehen = Release

    int holdSamples = 0;
    int holdCounter = 0;
};
} // namespace sis::dsp
