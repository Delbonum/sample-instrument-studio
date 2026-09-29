#pragma once

#include <JuceHeader.h>
#include <array>

#include "EnvelopeFollower.h"

namespace sis::dsp
{
struct DeEsserSettings
{
    float frequencyHz = 6000.0f;   // ab hier gilt es als Zischlaut
    float thresholdDb = -30.0f;
    float rangeDb = 12.0f;         // wie weit das Band höchstens abgesenkt wird
    float releaseMs = 60.0f;

    bool operator== (const DeEsserSettings& other) const noexcept
    {
        return juce::approximatelyEqual (frequencyHz, other.frequencyHz)
            && juce::approximatelyEqual (thresholdDb, other.thresholdDb)
            && juce::approximatelyEqual (rangeDb, other.rangeDb)
            && juce::approximatelyEqual (releaseMs, other.releaseMs);
    }

    bool operator!= (const DeEsserSettings& other) const noexcept { return ! (*this == other); }
};

/** Nimmt Schärfe heraus, ohne den ganzen Ton zu ducken.

    Das Signal wird in zwei Bänder zerlegt: ein Tiefpass liefert das Untere, die
    **Differenz** dazu das Obere. Diese Zerlegung ist genau ergänzend – bei Verstärkung 1
    ergibt die Summe wieder das Eingangssignal, Bit für Bit. Abgesenkt wird dann nur das
    obere Band.

    Der Tiefpass ist **zweipolig**, nicht einpolig. Mit einem Pol ist die Flanke so flach,
    dass bei der doppelten Trennfrequenz noch gut ein Drittel des Signals im unteren Band
    steckt – und was dort steckt, wird nie abgesenkt. Der De-Esser bekäme den Zischlaut
    dann schlicht nicht zu fassen.

    Das ist der Unterschied zu einem Kompressor, der zwar auf die Höhen hört, aber alles
    leiser macht: dort sackt bei jedem Zischlaut der ganze Ton weg. Hier bleibt der
    Grundton stehen.

    Beide Kanäle werden mit derselben Verstärkung bearbeitet. */
class DeEsser
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const DeEsserSettings&) noexcept;
    DeEsserSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    DeEsserSettings settings;
    double currentSampleRate = 44100.0;

    float splitCoefficient = 0.5f;
    std::array<float, maxChannels> lowState {};
    std::array<float, maxChannels> lowState2 {};

    EnvelopeFollower detector;
};
} // namespace sis::dsp
