#pragma once

#include <JuceHeader.h>
#include <array>

namespace sis::dsp
{
enum class BandType
{
    lowShelf,
    peak,
    highShelf,
    highPass,
    lowPass
};

juce::String toDisplayString (BandType);

/** Einstellungen eines Bandes. Frequenz in Hz, Anhebung in dB, Güte als Q. */
struct BandSettings
{
    BandType type = BandType::peak;
    float frequency = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.707f;
    bool enabled = true;

    bool usesGain() const noexcept
    {
        return type == BandType::lowShelf || type == BandType::peak || type == BandType::highShelf;
    }

    bool operator== (const BandSettings& other) const noexcept
    {
        return type == other.type
            && juce::approximatelyEqual (frequency, other.frequency)
            && juce::approximatelyEqual (gainDb, other.gainDb)
            && juce::approximatelyEqual (q, other.q)
            && enabled == other.enabled;
    }

    bool operator!= (const BandSettings& other) const noexcept { return ! (*this == other); }
};

/** Vierbandiger Equalizer aus Biquad-Filtern.

    Wird sowohl in der Effektkette einer Spur als auch vom eigenständigen
    Plugin „SIS Equalizer" verwendet. Die Koeffizienten werden nur neu berechnet,
    wenn sich ein Band tatsächlich ändert. */
class Equalizer
{
public:
    static constexpr int numBands = 4;
    static constexpr float minFrequency = 20.0f;
    static constexpr float maxFrequency = 20000.0f;

    Equalizer();

    void prepare (double sampleRate, int numChannels, int maximumBlockSize);
    void reset();

    void setBand (int index, const BandSettings&);
    BandSettings getBand (int index) const;

    /** Bearbeitet den Puffer an Ort und Stelle. */
    void process (juce::AudioBuffer<float>&);
    void process (float* const* channels, int numChannels, int numSamples);

    /** Verstärkung bei einer Frequenz als Faktor – für die Kurve in der Oberfläche. */
    double getMagnitudeAt (double frequency) const;

    /** Sinnvolle Voreinstellung: Tiefen, zwei Mitten, Höhen. */
    static BandSettings getDefaultBand (int index);

    using Coefficients = juce::dsp::IIR::Coefficients<float>;

    /** Koeffizienten vorab berechnen – das gehört nicht in den Audio-Thread. */
    static Coefficients::Ptr makeCoefficients (const BandSettings&, double sampleRate);

    /** Ob ein Band überhaupt etwas tut (aus, oder Glocke ohne Anhebung). */
    static bool isBandAudible (const BandSettings&);

    /** Fertige Koeffizienten übernehmen: keine Berechnung, keine Speicheranforderung. */
    void setPreparedBand (int index, const Coefficients::Ptr&, bool audible) noexcept;

private:
    void updateCoefficients (int index);

    using Filter = juce::dsp::IIR::Filter<float>;

    std::array<BandSettings, numBands> bands;
    std::array<bool, numBands> bandAudible { true, true, true, true };
    std::array<Coefficients::Ptr, numBands> coefficients;
    std::vector<std::array<Filter, numBands>> filters;   // je Kanal ein Satz

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Equalizer)
};
} // namespace sis::dsp
