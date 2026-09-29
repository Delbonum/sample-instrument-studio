#include "SweptFilter.h"

#include <cmath>

namespace sis::dsp
{
namespace
{
    /* Der Zustandsvariablenfilter bleibt nur stabil, solange f klein genug bleibt;
       das begrenzt die Eckfrequenz auf etwa ein Sechstel der Abtastrate. Für eine
       Filterfahrt ist das reichlich – ein Wah bewegt sich zwischen 200 Hz und 3 kHz. */
    constexpr float maxFrequencyCoefficient = 0.9f;
    constexpr float minDamping = 0.15f;       // entspricht Güte 6,7
    constexpr double lowestCutoffHz = 100.0;
}

void SweptFilter::prepare (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    follower.prepare (currentSampleRate);
    lfo.prepare (currentSampleRate);

    setSettings (settings);
    reset();
}

void SweptFilter::reset()
{
    for (auto& state : states)
        state = {};

    follower.reset();
    lfo.reset();
    samplesUntilUpdate = 0;
}

void SweptFilter::setSettings (const SweptFilterSettings& newSettings) noexcept
{
    settings = newSettings;

    follower.setTimes (juce::jmax (0.1f, settings.attackMs), juce::jmax (1.0f, settings.releaseMs));
    lfo.setRate (settings.rateHz);

    dampingCoefficient = juce::jmax (minDamping, 1.0f / juce::jmax (0.5f, settings.resonance));
}

void SweptFilter::updateCoefficients (double cutoffHz) noexcept
{
    const double limited = juce::jlimit (lowestCutoffHz, currentSampleRate / 6.0, cutoffHz);

    frequencyCoefficient = juce::jmin (maxFrequencyCoefficient,
                                       (float) (2.0 * std::sin (juce::MathConstants<double>::pi
                                                                * limited / currentSampleRate)));
}

void SweptFilter::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int usableChannels = juce::jmin (numChannels, maxChannels);

    if (usableChannels <= 0)
        return;

    const float wet = juce::jlimit (0.0f, 1.0f, settings.mix);
    const float dry = 1.0f - wet;

    const double low = juce::jmax (lowestCutoffHz, (double) settings.lowHz);
    const double high = juce::jmax (low * 1.05, (double) settings.highHz);
    const double span = high / low;

    const bool fromEnvelope = settings.source == SweepSource::envelope;
    const double sensitivity = juce::jlimit (0.0f, 1.0f, settings.sensitivity);

    for (int i = 0; i < numSamples; ++i)
    {
        // Lage im Intervall, 0 … 1 - danach logarithmisch in eine Frequenz umgerechnet
        double position = 0.0;

        if (fromEnvelope)
        {
            float level = 0.0f;

            for (int channel = 0; channel < usableChannels; ++channel)
                level = juce::jmax (level, std::abs (channels[channel][i]));

            // Der Anschlag zieht die Frequenz nach oben; volle Aussteuerung ganz hinauf
            position = juce::jlimit (0.0, 1.0, (double) follower.process (level) * sensitivity * 1.5);
        }
        else
        {
            position = 0.5 * (1.0 + lfo.value());
            lfo.advance();
        }

        if (samplesUntilUpdate <= 0)
        {
            updateCoefficients (low * std::pow (span, position));
            samplesUntilUpdate = coefficientInterval;
        }

        --samplesUntilUpdate;

        for (int channel = 0; channel < usableChannels; ++channel)
        {
            auto& state = states[(size_t) channel];
            const float in = channels[channel][i];

            state.low += frequencyCoefficient * state.band;
            const float bandInput = in - state.low - dampingCoefficient * state.band;
            state.band += frequencyCoefficient * bandInput;

            channels[channel][i] = dry * in + wet * state.band;
        }
    }
}
} // namespace sis::dsp
