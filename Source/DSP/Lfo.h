#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace sis::dsp
{
/** Niederfrequenz-Oszillator: zählt Phase und gibt einen Sinus aus.

    Bewusst winzig und kopf-nur – Flanger, Vibrato, Phaser und Tremolo brauchen alle
    dasselbe, und drei eigene Phasenzähler nebeneinander wären drei Gelegenheiten,
    denselben Fehler zu machen.

    Die Phase läuft in **Umdrehungen** (0 … 1), nicht im Bogenmaß. So lässt sich ein
    zweiter Kanal einfach um einen Bruchteil versetzt abfragen, ohne mit 2π zu rechnen. */
class Lfo
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        updateStep();
        reset();
    }

    void reset() noexcept { phase = 0.0; }

    void setRate (float hz) noexcept
    {
        const double limited = juce::jlimit (0.01, 40.0, (double) hz);

        if (! juce::approximatelyEqual (limited, rateHz))
        {
            rateHz = limited;
            updateStep();
        }
    }

    /** Wert des Sinus, optional um `offsetTurns` Umdrehungen versetzt. */
    float value (double offsetTurns = 0.0) const noexcept
    {
        return (float) std::sin (2.0 * juce::MathConstants<double>::pi * (phase + offsetTurns));
    }

    /** Einen Abtastschritt weiterdrehen. */
    void advance() noexcept
    {
        phase += step;

        if (phase >= 1.0)
            phase -= 1.0;
    }

private:
    void updateStep() noexcept { step = rateHz / sampleRate; }

    double sampleRate = 44100.0;
    double rateHz = 1.0;
    double step = 1.0 / 44100.0;
    double phase = 0.0;
};
} // namespace sis::dsp
