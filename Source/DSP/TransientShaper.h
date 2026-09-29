#pragma once

#include <JuceHeader.h>

namespace sis::dsp
{
/** Einstellungen des Transienten-Formers. Beide Werte -1 … +1, 0 = unverändert. */
struct TransientSettings
{
    float attack = 0.0f;    // positiv betont den Anschlag, negativ nimmt ihn zurück
    float sustain = 0.0f;   // positiv verlängert das Ausklingen, negativ kürzt es

    bool operator== (const TransientSettings& other) const noexcept
    {
        return juce::approximatelyEqual (attack, other.attack)
            && juce::approximatelyEqual (sustain, other.sustain);
    }

    bool operator!= (const TransientSettings& other) const noexcept { return ! (*this == other); }
};

/** Transienten-Former über zwei Hüllkurvenfolger.

    Ein schneller und ein langsamer Folger laufen auf demselben Signal. Solange der
    schnelle über dem langsamen liegt, steigt der Pegel gerade an — das ist der Anschlag;
    liegt er darunter, klingt der Ton aus. Aus dem Abstand der beiden in dB entsteht die
    Verstärkung, begrenzt auf ±18 dB, damit leise Stellen den Quotienten nicht sprengen.

    Beide Kanäle werden mit derselben Verstärkung bearbeitet, sonst wandert das Stereobild. */
class TransientShaper
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const TransientSettings&) noexcept;
    TransientSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    static float coefficientFor (double milliseconds, double sampleRate) noexcept;

    TransientSettings settings;
    double currentSampleRate = 44100.0;

    float fastEnvelope = 0.0f;
    float slowEnvelope = 0.0f;

    float fastAttack = 0.0f, fastRelease = 0.0f;
    float slowAttack = 0.0f, slowRelease = 0.0f;
};
} // namespace sis::dsp
