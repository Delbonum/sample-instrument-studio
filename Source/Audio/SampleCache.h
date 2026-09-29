#pragma once

#include <JuceHeader.h>
#include <map>

namespace sis
{
class InstrumentModel;

/** Audiodaten eines Samples. Wird vom Message-Thread geladen und danach nur noch gelesen,
    damit der Audio-Thread gefahrlos darauf zugreifen kann. */
struct SampleData : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SampleData>;

    juce::String name;
    juce::File file;
    juce::AudioBuffer<float> buffer;
    double sourceSampleRate = 44100.0;

    int getNumSamples() const noexcept  { return buffer.getNumSamples(); }
    int getNumChannels() const noexcept { return buffer.getNumChannels(); }
    double getLengthSeconds() const noexcept
    {
        return sourceSampleRate > 0.0 ? (double) buffer.getNumSamples() / sourceSampleRate : 0.0;
    }
};

/** Hält die geladenen Audiodaten, nach Dateiname geschlüsselt.
    Nur auf dem Message-Thread bedienen. */
class SampleCache
{
public:
    /** Längenbegrenzung, damit ein versehentlich geladener Mitschnitt nicht den Speicher sprengt. */
    static constexpr double maxLengthSeconds = 120.0;

    SampleData::Ptr get (const juce::String& name) const;
    SampleData::Ptr load (const juce::File&, juce::AudioFormatManager&);

    /** Für Tests: fertige Audiodaten ablegen. */
    void insert (SampleData::Ptr);

    /** Lädt fehlende Dateien des Instruments und wirft nicht mehr verwendete Daten weg. */
    void syncWith (const InstrumentModel&, juce::AudioFormatManager&);

    int size() const noexcept { return (int) samples.size(); }
    juce::int64 getTotalBytes() const;
    void clear();

private:
    std::map<juce::String, SampleData::Ptr> samples;
};
} // namespace sis
