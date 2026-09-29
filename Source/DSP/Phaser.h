#pragma once

#include <JuceHeader.h>
#include <array>

#include "Lfo.h"

namespace sis::dsp
{
struct PhaserSettings
{
    float depth = 0.7f;       // wie weit die Kerben wandern
    float rateHz = 0.3f;
    float feedback = 0.0f;    // 0 … 0.9, schärft die Kerben
    float mix = 0.5f;

    bool operator== (const PhaserSettings& other) const noexcept
    {
        return juce::approximatelyEqual (depth, other.depth)
            && juce::approximatelyEqual (rateHz, other.rateHz)
            && juce::approximatelyEqual (feedback, other.feedback)
            && juce::approximatelyEqual (mix, other.mix);
    }

    bool operator!= (const PhaserSettings& other) const noexcept { return ! (*this == other); }
};

/** Phaser aus einer Kette von Allpässen.

    Der Unterschied zum Flanger steckt im Verfahren, nicht im Klangeindruck: der Flanger
    verzögert und erzeugt damit einen Kamm aus **gleichmäßig verteilten** Kerben; der
    Phaser dreht mit Allpässen nur die Phase und erzeugt **wenige**, die nicht harmonisch
    zueinander liegen. Deshalb klingt der eine nach Düsenjäger und der andere nach Wirbel.

    Sechs Stufen erster Ordnung ergeben drei Kerben. Ihre Eckfrequenz wandert zwischen
    200 Hz und 2 kHz; die Rückkopplung macht die Kerben tiefer. */
class Phaser
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int numStages = 6;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const PhaserSettings&) noexcept;
    PhaserSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    /** Koeffizient eines Allpasses erster Ordnung zu einer Eckfrequenz. */
    float coefficientFor (double frequency) const noexcept;

    PhaserSettings settings;
    double currentSampleRate = 44100.0;

    Lfo lfo;

    struct Stage
    {
        float lastInput = 0.0f;
        float lastOutput = 0.0f;
    };

    std::array<std::array<Stage, numStages>, maxChannels> stages {};
    std::array<float, maxChannels> feedbackState {};
};
} // namespace sis::dsp
