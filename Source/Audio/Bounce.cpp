#include "Bounce.h"
#include "SamplerEngine.h"
#include "../Model/Text.h"

#include <cmath>

namespace sis
{
namespace
{
    // Alles darunter gilt als Stille (etwa -90 dBFS); daran wird der Nachklang abgeschnitten.
    constexpr float silenceThreshold = 3.0e-5f;
    constexpr double minimumLengthSeconds = 0.05;

    /** Länge der Zone auf der Zeitachse, nach denselben Mute/Solo-Regeln wie im Renderplan. */
    double getBodySeconds (const Zone& zone)
    {
        const bool anySolo = std::any_of (zone.tracks.begin(), zone.tracks.end(),
                                          [] (const Track& t) { return t.solo; });
        double seconds = 0.0;

        for (const auto& track : zone.tracks)
        {
            if (track.mute || (anySolo && ! track.solo) || ! track.hasClip())
                continue;

            seconds = juce::jmax (seconds, (track.offset + track.clipLength()) * InstrumentModel::timelineSeconds);
        }

        // Clips dürfen im Editor bis 128 s reichen; so lang darf dann auch der Bounce werden
        return juce::jlimit (minimumLengthSeconds, InstrumentModel::timelineSeconds * 16.0, seconds);
    }
}

//==============================================================================
BounceResult renderZone (const BounceRequest& request)
{
    BounceResult result;
    result.sampleRate = request.sampleRate;

    if (request.model == nullptr || request.cache == nullptr || request.sampleRate <= 0.0)
    {
        result.message = "Kein Instrument zum Rendern."_u;
        return result;
    }

    const Zone* zone = nullptr;

    for (const auto& candidate : request.model->zones)
        if (candidate.id == request.zoneId)
            zone = &candidate;

    if (zone == nullptr)
    {
        result.message = "Die Zone gibt es nicht mehr."_u;
        return result;
    }

    // Ein Instrument nur für diesen Lauf: allein diese Zone, über die ganze Klaviatur,
    // damit die Probenote sie sicher trifft.
    InstrumentModel solo;
    solo.clear();
    solo.zones.push_back (*zone);

    auto& soloZone = solo.zones.front();
    soloZone.lowNote = 0;
    soloZone.highNote = 127;
    soloZone.lowVelocity = 0;
    soloZone.highVelocity = 127;
    solo.selectedZoneId = soloZone.id;

    auto plan = buildRenderPlan (solo, *request.cache, request.sampleRate, request.hostedPlugins);

    if (plan == nullptr || plan->zones.empty() || plan->zones.front().layers.empty())
    {
        result.message = "Die Zone hat keine hörbaren Spuren."_u;
        return result;
    }

    // Die Hüllkurve wirkt weiter beim Spielen - sie darf nicht mit ins Sample.
    // Das kurze Release verhindert nur den Knacks am Schnitt einer Schleife.
    juce::ADSR::Parameters neutral;
    neutral.attack = 0.0f;
    neutral.decay = 0.0f;
    neutral.sustain = 1.0f;
    neutral.release = 0.02f;
    plan->zones.front().envelope = neutral;

    const int blockSize = juce::jlimit (32, 4096, request.blockSize);
    const int bodySamples = (int) std::ceil (getBodySeconds (*zone) * request.sampleRate);
    const int tailSamples = (int) std::ceil (juce::jlimit (0.0, 30.0, request.tailSeconds) * request.sampleRate);
    const int total = bodySamples + tailSamples;

    // Der Sampler bringt 32 Signalwege mit - den nicht auf den Stapel legen.
    auto engine = std::make_unique<SamplerEngine>();
    engine->prepare (request.sampleRate, blockSize);
    engine->setPlan (plan);

    juce::AudioBuffer<float> rendered (2, total);
    rendered.clear();

    bool released = false;

    for (int offset = 0; offset < total; offset += blockSize)
    {
        const int chunk = juce::jmin (blockSize, total - offset);

        juce::MidiBuffer midi;

        if (offset == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, zone->rootNote, 1.0f), 0);

        // Am Ende des Körpers loslassen: Schleifen hören auf, der Nachklang bleibt.
        if (! released && offset + chunk > bodySamples)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, zone->rootNote, 0.0f),
                           juce::jlimit (0, chunk - 1, bodySamples - offset));
            released = true;
        }

        juce::AudioBuffer<float> view (rendered.getArrayOfWritePointers(), 2, offset, chunk);
        engine->process (view, midi);
    }

    // Hinten die Stille abschneiden - ein Hall, der früh ausklingt, soll keine Sekunden kosten
    int last = total - 1;

    while (last > 0
           && juce::jmax (std::abs (rendered.getSample (0, last)), std::abs (rendered.getSample (1, last)))
                  < silenceThreshold)
        --last;

    if (last == 0 && rendered.getMagnitude (0, total) < silenceThreshold)
    {
        result.message = "Die Zone bleibt stumm - nichts zu rendern."_u;
        return result;
    }

    const int length = juce::jmax ((int) std::ceil (minimumLengthSeconds * request.sampleRate), last + 1);

    result.audio.setSize (2, juce::jmin (length, total));
    result.audio.clear();

    for (int channel = 0; channel < 2; ++channel)
        result.audio.copyFrom (channel, 0, rendered, channel, 0, result.audio.getNumSamples());

    result.succeeded = true;
    return result;
}

//==============================================================================
bool writeBounceToFile (const BounceResult& bounce, const juce::File& file, juce::String& error)
{
    if (! bounce.succeeded || bounce.audio.getNumSamples() == 0)
    {
        error = "Es gibt nichts zu schreiben."_u;
        return false;
    }

    file.getParentDirectory().createDirectory();
    file.deleteFile();

    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

    if (stream == nullptr || ! stream->openedOk())
    {
        error = "Datei konnte nicht angelegt werden: "_u + file.getFullPathName();
        return false;
    }

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), bounce.sampleRate, 2, 24, {}, 0));

    if (writer == nullptr)
    {
        error = "WAV-Schreiber konnte nicht angelegt werden."_u;
        return false;
    }

    stream.release();   // gehört jetzt dem Schreiber

    if (! writer->writeFromAudioSampleBuffer (bounce.audio, 0, bounce.audio.getNumSamples()))
    {
        error = "Die Audiodaten konnten nicht geschrieben werden."_u;
        return false;
    }

    return true;
}
} // namespace sis
