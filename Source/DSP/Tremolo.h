#pragma once

#include <JuceHeader.h>

#include "Lfo.h"

namespace sis::dsp
{
struct TremoloSettings
{
    float depth = 0.6f;     // 0 = nichts, 1 = bis zur Stille herunter
    float rateHz = 5.0f;
    float shape = 0.0f;     // 0 = Sinus, 1 = fast rechteckig
    float width = 0.0f;     // 0 = beide Kanäle gleich, 1 = gegenläufig (Auto-Panorama)

    bool operator== (const TremoloSettings& other) const noexcept
    {
        return juce::approximatelyEqual (depth, other.depth)
            && juce::approximatelyEqual (rateHz, other.rateHz)
            && juce::approximatelyEqual (shape, other.shape)
            && juce::approximatelyEqual (width, other.width);
    }

    bool operator!= (const TremoloSettings& other) const noexcept { return ! (*this == other); }
};

/** Tremolo: der Pegel schwankt im Takt eines Oszillators.

    Die Form geht vom Sinus zum Rechteck, aber **nie zum harten Rechteck** – die Kanten
    laufen durch eine Tangens-Kennlinie und bleiben dadurch stetig. Ein echter Sprung im
    Pegel knackt, und zwar bei jedem Durchgang.

    Bei voller Breite laufen die Kanäle gegenläufig; dann ist es kein Tremolo mehr,
    sondern ein Auto-Panorama. */
class Tremolo
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const TremoloSettings&) noexcept;
    TremoloSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    float shapedValue (double offsetTurns) const noexcept;

    TremoloSettings settings;
    Lfo lfo;

    float sharpness = 1.0f;     // Steilheit der Kennlinie, aus `shape`
    float normalise = 1.0f;     // damit die Form den Hub nicht verändert
};
} // namespace sis::dsp
