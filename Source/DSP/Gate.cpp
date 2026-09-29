#include "Gate.h"

namespace sis::dsp
{
namespace
{
    /* Der Detektor folgt schnell und fällt auch schnell. Ein träger Rückweg wäre
       bequem gegen das Rasseln, macht aber den Halten-Regler wirkungslos: mit 40 ms
       kommt die Hüllkurve in einer 50-ms-Lücke gar nicht erst unter die Schwelle, und
       das Gate schließt nie – ganz gleich, was eingestellt ist. Gegen das Rasseln ist
       die Haltezeit da, und die gehört dem Anwender. */
    constexpr double detectorAttackMs = 0.5;
    constexpr double detectorReleaseMs = 10.0;

    constexpr float floorDb = -100.0f;
}

void Gate::prepare (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    detector.prepare (currentSampleRate);
    detector.setTimes (detectorAttackMs, detectorReleaseMs);

    smoother.prepare (currentSampleRate);
    setSettings (settings);
    reset();
}

void Gate::reset()
{
    detector.reset();
    smoother.reset();
    holdCounter = 0;
}

void Gate::setSettings (const GateSettings& newSettings) noexcept
{
    settings = newSettings;

    smoother.setTimes (juce::jmax (0.0f, settings.attackMs), juce::jmax (0.0f, settings.releaseMs));
    holdSamples = (int) (juce::jmax (0.0f, settings.holdMs) * 0.001 * currentSampleRate);
}

void Gate::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (numChannels <= 0)
        return;

    const float ratio = juce::jmax (1.0f, settings.ratio);
    const float range = juce::jmax (0.0f, settings.rangeDb);

    for (int i = 0; i < numSamples; ++i)
    {
        // Gekoppelt: der lauteste Kanal entscheidet
        float level = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            level = juce::jmax (level, std::abs (channels[channel][i]));

        const float envelope = detector.process (level);
        const float levelDb = juce::Decibels::gainToDecibels (envelope, floorDb);

        float targetGain = 1.0f;

        if (levelDb >= settings.thresholdDb)
        {
            holdCounter = holdSamples;
        }
        else if (holdCounter > 0)
        {
            --holdCounter;   // noch offen halten
        }
        else
        {
            // Unter der Schwelle: je Dezibel darunter (Verhältnis − 1) Dezibel weniger
            const float belowDb = levelDb - settings.thresholdDb;
            const float gainDb = juce::jmax (-range, belowDb * (ratio - 1.0f));

            targetGain = juce::Decibels::decibelsToGain (gainDb);
        }

        const float gain = smoother.process (targetGain);

        for (int channel = 0; channel < numChannels; ++channel)
            channels[channel][i] *= gain;
    }
}
} // namespace sis::dsp
