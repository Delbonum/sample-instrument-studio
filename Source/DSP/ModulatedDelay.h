#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

#include "Lfo.h"

namespace sis::dsp
{
enum class DelayMode
{
    flanger,   // sehr kurze Verzögerung mit Rückkopplung, dem Trockensignal beigemischt
    vibrato    // reine Tonhöhenschwankung, ohne Trockenanteil
};

struct ModulatedDelaySettings
{
    DelayMode mode = DelayMode::flanger;
    float depth = 0.6f;       // 0 … 1
    float rateHz = 0.4f;
    float feedback = 0.0f;    // nur Flanger, 0 … 0.9
    float mix = 0.5f;         // nur Flanger; das Vibrato ist immer ganz nass

    bool operator== (const ModulatedDelaySettings& other) const noexcept
    {
        return mode == other.mode
            && juce::approximatelyEqual (depth, other.depth)
            && juce::approximatelyEqual (rateHz, other.rateHz)
            && juce::approximatelyEqual (feedback, other.feedback)
            && juce::approximatelyEqual (mix, other.mix);
    }

    bool operator!= (const ModulatedDelaySettings& other) const noexcept { return ! (*this == other); }
};

/** Modulierte Verzögerung in zwei Ausprägungen.

    **Flanger**: eine sehr kurze Verzögerung (0,2 … 8 ms), die dem Trockensignal beigemischt
    wird. Aus der Überlagerung entsteht ein Kamm aus Auslöschungen, der mit der Verzögerung
    wandert. Die **Rückkopplung** schickt das Verzögerte noch einmal hinein und macht die
    Kerben schmaler – sie ist das, was den Flanger vom Chorus unterscheidet, nicht die
    Verzögerungszeit allein.

    **Vibrato**: dieselbe Leitung, aber **ohne Trockenanteil**. Gehört wird dann nicht der
    Kamm, sondern die Tonhöhe, die mit der Leselänge schwankt. Genau deshalb gibt es hier
    keinen Mischregler: mischt man trocken dazu, ist es ein Chorus – und den gibt es schon.

    Gelesen wird zwischen zwei Stützstellen interpoliert; der Speicher entsteht in
    `prepare`, `process` fordert nichts an.

    *Anmerkung:* `Chorus` ist älter und bringt seine eigene Leitung mit. Beide ließen sich
    zusammenlegen; das ist nicht geschehen, weil der Chorus getestet ausgeliefert ist und
    ein Umbau ohne hörbaren Gewinn Risiko wäre. */
class ModulatedDelay
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double maxDelayMs = 9.0;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const ModulatedDelaySettings&) noexcept;
    ModulatedDelaySettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    float readInterpolated (int channel, double delaySamples) const noexcept;

    ModulatedDelaySettings settings;
    double currentSampleRate = 44100.0;

    Lfo lfo;
    std::vector<std::vector<float>> lines;
    std::array<float, maxChannels> feedbackState {};
    int lineLength = 0;
    int writeIndex = 0;
};
} // namespace sis::dsp
