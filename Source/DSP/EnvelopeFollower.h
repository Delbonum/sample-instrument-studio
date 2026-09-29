#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace sis::dsp
{
/** Verfolgt den Pegel eines Signals mit getrennten Zeiten für Steigen und Fallen.

    Kopf-nur und winzig, weil Gate, Expander, Limiter und De-Esser alle dasselbe brauchen.
    Der Kompressor ist älter und bringt seinen eigenen mit; zusammenlegen ließe sich das,
    ist aber unterblieben – sein Folger hängt an der Meldung der Pegelabsenkung, und ein
    Umbau an getestetem Code ohne hörbaren Gewinn wäre nur Risiko. */
class EnvelopeFollower
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        setTimes (attackMs, releaseMs);
        reset();
    }

    void reset() noexcept { envelope = 0.0f; }

    void setTimes (double newAttackMs, double newReleaseMs) noexcept
    {
        attackMs = newAttackMs;
        releaseMs = newReleaseMs;

        attackCoefficient = coefficientFor (attackMs);
        releaseCoefficient = coefficientFor (releaseMs);
    }

    /** Einen gleichgerichteten Wert einrechnen und den neuen Pegel zurückgeben. */
    float process (float rectified) noexcept
    {
        const float coefficient = rectified > envelope ? attackCoefficient : releaseCoefficient;
        envelope = coefficient * (envelope - rectified) + rectified;
        return envelope;
    }

    float getValue() const noexcept { return envelope; }

private:
    float coefficientFor (double milliseconds) const noexcept
    {
        // Null bedeutet: sofort folgen
        const double samples = milliseconds * 0.001 * sampleRate;
        return samples < 1.0 ? 0.0f : (float) std::exp (-1.0 / samples);
    }

    double sampleRate = 44100.0;
    double attackMs = 5.0;
    double releaseMs = 100.0;

    float attackCoefficient = 0.0f;
    float releaseCoefficient = 0.0f;
    float envelope = 0.0f;
};
} // namespace sis::dsp
