#include "SampleCache.h"
#include "../Model/Instrument.h"

#include <set>

namespace sis
{
SampleData::Ptr SampleCache::get (const juce::String& name) const
{
    const auto it = samples.find (name);
    return it != samples.end() ? it->second : nullptr;
}

void SampleCache::insert (SampleData::Ptr data)
{
    if (data != nullptr)
    {
        const auto name = data->name;   // vor dem Verschieben lesen
        samples[name] = std::move (data);
    }
}

SampleData::Ptr SampleCache::load (const juce::File& file, juce::AudioFormatManager& formats)
{
    if (! file.existsAsFile())
        return nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->numChannels == 0)
        return nullptr;

    const auto maxSamples = (juce::int64) (maxLengthSeconds * reader->sampleRate);
    const auto numSamples = (int) juce::jmin (reader->lengthInSamples, maxSamples);

    if (numSamples <= 0)
        return nullptr;

    SampleData::Ptr data (new SampleData());
    data->name = file.getFileName();
    data->file = file;
    data->sourceSampleRate = reader->sampleRate;
    data->buffer.setSize ((int) juce::jmin ((juce::uint32) 2, reader->numChannels), numSamples);
    reader->read (&data->buffer, 0, numSamples, 0, true, reader->numChannels > 1);

    samples[data->name] = data;
    return data;
}

void SampleCache::syncWith (const InstrumentModel& model, juce::AudioFormatManager& formats)
{
    std::set<juce::String> needed;

    for (const auto& sample : model.samples)
    {
        if (! sample.file.existsAsFile())
            continue;

        needed.insert (sample.name);

        if (auto existing = get (sample.name))
            if (existing->file == sample.file)
                continue;

        load (sample.file, formats);
    }

    for (auto it = samples.begin(); it != samples.end();)
        it = needed.count (it->first) == 0 ? samples.erase (it) : std::next (it);
}

juce::int64 SampleCache::getTotalBytes() const
{
    juce::int64 bytes = 0;

    for (const auto& [name, data] : samples)
        if (data != nullptr)
            bytes += (juce::int64) data->getNumChannels() * data->getNumSamples() * (juce::int64) sizeof (float);

    return bytes;
}

void SampleCache::clear()
{
    samples.clear();
}
} // namespace sis
