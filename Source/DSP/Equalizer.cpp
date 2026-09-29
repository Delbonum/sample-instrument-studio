#include "Equalizer.h"

namespace sis::dsp
{
juce::String toDisplayString (BandType type)
{
    switch (type)
    {
        case BandType::lowShelf:  return "Tiefen-Kuhschwanz";
        case BandType::peak:      return "Glocke";
        case BandType::highShelf: return "Höhen-Kuhschwanz";
        case BandType::highPass:  return "Hochpass";
        case BandType::lowPass:   return "Tiefpass";
    }

    return {};
}

Equalizer::Equalizer()
{
    for (int i = 0; i < numBands; ++i)
        bands[(size_t) i] = getDefaultBand (i);
}

BandSettings Equalizer::getDefaultBand (int index)
{
    BandSettings band;

    switch (index)
    {
        case 0: band.type = BandType::lowShelf;  band.frequency = 120.0f;   band.q = 0.707f; break;
        case 1: band.type = BandType::peak;      band.frequency = 500.0f;   band.q = 1.0f;   break;
        case 2: band.type = BandType::peak;      band.frequency = 2500.0f;  band.q = 1.0f;   break;
        default: band.type = BandType::highShelf; band.frequency = 8000.0f; band.q = 0.707f; break;
    }

    band.gainDb = 0.0f;
    band.enabled = true;
    return band;
}

void Equalizer::prepare (double sampleRate, int numChannels, int maximumBlockSize)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    filters.clear();
    filters.resize ((size_t) juce::jmax (1, numChannels));

    const juce::dsp::ProcessSpec spec { currentSampleRate, (juce::uint32) juce::jmax (1, maximumBlockSize), 1 };

    for (auto& channel : filters)
        for (auto& filter : channel)
            filter.prepare (spec);

    for (int i = 0; i < numBands; ++i)
        updateCoefficients (i);

    reset();
}

void Equalizer::reset()
{
    for (auto& channel : filters)
        for (auto& filter : channel)
            filter.reset();
}

void Equalizer::setBand (int index, const BandSettings& settings)
{
    if (! juce::isPositiveAndBelow (index, numBands) || bands[(size_t) index] == settings)
        return;

    bands[(size_t) index] = settings;
    updateCoefficients (index);
}

BandSettings Equalizer::getBand (int index) const
{
    return juce::isPositiveAndBelow (index, numBands) ? bands[(size_t) index] : BandSettings();
}

Equalizer::Coefficients::Ptr Equalizer::makeCoefficients (const BandSettings& band, double sampleRate)
{
    const double rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const float frequency = juce::jlimit (minFrequency,
                                          juce::jmin (maxFrequency, (float) (rate * 0.49)),
                                          band.frequency);
    const float q = juce::jlimit (0.1f, 18.0f, band.q);
    const float gain = juce::Decibels::decibelsToGain (band.gainDb);

    switch (band.type)
    {
        case BandType::lowShelf:  return Coefficients::makeLowShelf (rate, frequency, q, gain);
        case BandType::peak:      return Coefficients::makePeakFilter (rate, frequency, q, gain);
        case BandType::highShelf: return Coefficients::makeHighShelf (rate, frequency, q, gain);
        case BandType::highPass:  return Coefficients::makeHighPass (rate, frequency, q);
        case BandType::lowPass:   return Coefficients::makeLowPass (rate, frequency, q);
    }

    return Coefficients::makePeakFilter (rate, frequency, q, 1.0f);
}

bool Equalizer::isBandAudible (const BandSettings& band)
{
    return band.enabled && ! (band.usesGain() && std::abs (band.gainDb) < 0.01f);
}

void Equalizer::setPreparedBand (int index, const Coefficients::Ptr& newCoefficients, bool audible) noexcept
{
    if (! juce::isPositiveAndBelow (index, numBands))
        return;

    bandAudible[(size_t) index] = audible && newCoefficients != nullptr;

    if (newCoefficients == nullptr || coefficients[(size_t) index] == newCoefficients)
        return;

    coefficients[(size_t) index] = newCoefficients;

    for (auto& channel : filters)
        channel[(size_t) index].coefficients = newCoefficients;
}

void Equalizer::updateCoefficients (int index)
{
    const auto& band = bands[(size_t) index];
    coefficients[(size_t) index] = makeCoefficients (band, currentSampleRate);
    bandAudible[(size_t) index] = isBandAudible (band);

    for (auto& channel : filters)
        channel[(size_t) index].coefficients = coefficients[(size_t) index];
}

void Equalizer::process (juce::AudioBuffer<float>& buffer)
{
    process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

void Equalizer::process (float* const* channels, int numChannels, int numSamples)
{
    if (numSamples <= 0 || filters.empty())
        return;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& bank = filters[(size_t) juce::jmin (channel, (int) filters.size() - 1)];
        auto* samples = channels[channel];

        for (int bandIndex = 0; bandIndex < numBands; ++bandIndex)
        {
            // Ein ausgeschaltetes Band oder eine Glocke ohne Anhebung kostet nichts
            if (! bandAudible[(size_t) bandIndex])
                continue;

            auto& filter = bank[(size_t) bandIndex];

            for (int i = 0; i < numSamples; ++i)
                samples[i] = filter.processSample (samples[i]);

            filter.snapToZero();
        }
    }
}

double Equalizer::getMagnitudeAt (double frequency) const
{
    double magnitude = 1.0;

    for (int i = 0; i < numBands; ++i)
    {
        if (! bandAudible[(size_t) i])
            continue;

        if (auto coeffs = coefficients[(size_t) i])
            magnitude *= coeffs->getMagnitudeForFrequency (frequency, currentSampleRate);
    }

    return magnitude;
}
} // namespace sis::dsp
