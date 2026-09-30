// Prüft die Sampler-Engine ohne Audiogerät: Plan bauen, Blöcke rendern, Ergebnis messen.

#include <JuceHeader.h>

#include "../Source/Audio/EffectChain.h"
#include "../Source/Audio/RenderPlan.h"
#include "../Source/DSP/Compressor.h"
#include "../Source/DSP/Equalizer.h"
#include "../Source/Audio/SamplerEngine.h"
#include "../Source/Model/ClipGeometry.h"
#include "../Source/Model/Instrument.h"
#include "../Source/Model/PresetLibrary.h"

#include <cmath>
#include <map>
#include <set>
#include <cstdio>

namespace
{
int checks = 0;
int failures = 0;

void expect (const juce::String& what, bool condition)
{
    ++checks;

    if (! condition)
    {
        std::printf ("FEHLER  %s\n", what.toRawUTF8());
        ++failures;
    }
}

void expectNear (const juce::String& what, double actual, double expected, double tolerance)
{
    ++checks;

    if (std::abs (actual - expected) > tolerance)
    {
        std::printf ("FEHLER  %s: %.6f erwartet, %.6f erhalten\n", what.toRawUTF8(), expected, actual);
        ++failures;
    }
}

constexpr double testSampleRate = 48000.0;

/** Gleichanteil 1.0 – damit lassen sich Pegel, Fades und Hüllkurve direkt ablesen. */
sis::SampleData::Ptr makeConstantSample (const juce::String& name, int numSamples, float value = 1.0f)
{
    sis::SampleData::Ptr data (new sis::SampleData());
    data->name = name;
    data->sourceSampleRate = testSampleRate;
    data->buffer.setSize (1, numSamples);
    data->buffer.clear();

    for (int i = 0; i < numSamples; ++i)
        data->buffer.setSample (0, i, value);

    return data;
}

/** Rampe 0 … 1 – die Steigung zeigt, wie schnell abgespielt wird. */
sis::SampleData::Ptr makeRampSample (const juce::String& name, int numSamples)
{
    sis::SampleData::Ptr data (new sis::SampleData());
    data->name = name;
    data->sourceSampleRate = testSampleRate;
    data->buffer.setSize (1, numSamples);

    for (int i = 0; i < numSamples; ++i)
        data->buffer.setSample (0, i, (float) i / (float) (numSamples - 1));

    return data;
}

/** Sinus fester Frequenz - die Nulldurchgänge zeigen, ob die Tonhöhe erhalten bleibt. */
sis::SampleData::Ptr makeSineSample (const juce::String& name, int numSamples, double frequency)
{
    sis::SampleData::Ptr data (new sis::SampleData());
    data->name = name;
    data->sourceSampleRate = testSampleRate;
    data->buffer.setSize (1, numSamples);

    for (int i = 0; i < numSamples; ++i)
        data->buffer.setSample (0, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                        * frequency * i / testSampleRate));

    return data;
}

/** Pegel bei einer bestimmten Frequenz (einzelne Fourier-Komponente).

    Reicht hier vollkommen: gefragt ist nicht das ganze Spektrum, sondern ob bei der
    doppelten Grundfrequenz etwas steht. */
double magnitudeAt (const juce::AudioBuffer<float>& buffer, double frequency,
                    int start, int length)
{
    double real = 0.0, imaginary = 0.0;

    for (int i = 0; i < length; ++i)
    {
        const double phase = 2.0 * juce::MathConstants<double>::pi * frequency
                             * (start + i) / testSampleRate;
        const double sample = buffer.getSample (0, start + i);
        real += sample * std::cos (phase);
        imaginary += sample * std::sin (phase);
    }

    return 2.0 * std::sqrt (real * real + imaginary * imaginary) / (double) length;
}

sis::Track makeTrack (const juce::String& clip)
{
    sis::Track track;
    track.name = clip;
    sis::Clip piece;
    piece.sample = clip;
    track.clips.push_back (piece);
    track.gain = 1.0f;
    track.pan = 0.0f;
    track.loop = sis::LoopMode::sustainLoop;
    return track;
}

/** Instrument mit einer Zone (C1–B6, Grundton C4) und den übergebenen Spuren.
    InstrumentModel ist als ChangeBroadcaster weder kopier- noch verschiebbar. */
std::unique_ptr<sis::InstrumentModel> makeModel (std::vector<sis::Track> tracks, int rootNote = 60)
{
    auto owned = std::make_unique<sis::InstrumentModel>();
    auto& model = *owned;
    model.zones.clear();

    sis::Zone zone;
    zone.id = "z";
    zone.name = "Test";
    zone.lowNote = 24;
    zone.highNote = 95;
    zone.rootNote = rootNote;
    zone.tracks = std::move (tracks);
    model.zones.push_back (std::move (zone));
    model.selectedZoneId = "z";

    // Rechteck-Hüllkurve: sofort auf vollem Pegel, damit Pegel messbar bleiben
    model.envelope.attack = 0.0f;
    model.envelope.decay = 0.0f;
    model.envelope.sustain = 1.0f;
    model.envelope.release = 0.0f;
    return owned;
}

struct Rendered
{
    juce::AudioBuffer<float> buffer;

    /** Nulldurchgänge im Bereich - ein Maß für die Frequenz. */
    int zeroCrossings (int start, int numSamples) const
    {
        int count = 0;
        for (int i = start + 1; i < start + numSamples && i < buffer.getNumSamples(); ++i)
            if ((buffer.getSample (0, i - 1) < 0.0f) != (buffer.getSample (0, i) < 0.0f))
                ++count;
        return count;
    }

    /** Letztes Sample über der Schwelle - zeigt, wann die Wiedergabe endet. */
    int lastSoundingSample (float threshold = 0.02f) const
    {
        for (int i = buffer.getNumSamples(); --i >= 0;)
            if (std::abs (buffer.getSample (0, i)) > threshold)
                return i;
        return -1;
    }

    float peak (int start, int numSamples) const
    {
        return buffer.getMagnitude (start, juce::jmin (numSamples, buffer.getNumSamples() - start));
    }

    float at (int index) const { return buffer.getSample (0, index); }
};

/** Spielt eine Note und rendert `numSamples` Samples; optional wird nach `noteOffAt` losgelassen. */
Rendered render (sis::SamplerEngine& engine, int numSamples, int note, float velocity,
                 int noteOnAt = 0, int noteOffAt = -1, int blockSize = 128)
{
    Rendered result;
    result.buffer.setSize (2, numSamples);
    result.buffer.clear();

    for (int position = 0; position < numSamples; position += blockSize)
    {
        const int thisBlock = juce::jmin (blockSize, numSamples - position);
        juce::AudioBuffer<float> block (result.buffer.getArrayOfWritePointers(), 2, position, thisBlock);
        juce::MidiBuffer midi;

        if (noteOnAt >= position && noteOnAt < position + thisBlock)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, velocity), noteOnAt - position);

        if (noteOffAt >= position && noteOffAt < position + thisBlock)
            midi.addEvent (juce::MidiMessage::noteOff (1, note), noteOffAt - position);

        engine.process (block, midi);
    }

    return result;
}

std::unique_ptr<sis::SamplerEngine> makeEngine (const sis::InstrumentModel& model, const sis::SampleCache& cache);

/** Kurzform: Modell anlegen, Engine bauen, Modell dabei am Leben halten. */
std::unique_ptr<sis::SamplerEngine> makeEngineFor (std::unique_ptr<sis::InstrumentModel> model,
                                                   const sis::SampleCache& cache)
{
    auto engine = makeEngine (*model, cache);
    return engine;   // der Plan hält alles Nötige; das Modell wird nicht mehr gebraucht
}

std::unique_ptr<sis::SamplerEngine> makeEngine (const sis::InstrumentModel& model, const sis::SampleCache& cache)
{
    auto engine = std::make_unique<sis::SamplerEngine>();
    engine->prepare (testSampleRate, 128);
    engine->setPlan (sis::buildRenderPlan (model, cache));
    return engine;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // --- Plan: Spuren ohne geladenes Sample fallen weg -------------------------
    {
        sis::SampleCache cache;
        const auto modelPtr = makeModel ({ makeTrack ("da.wav"), makeTrack ("fehlt.wav") });
        cache.insert (makeConstantSample ("da.wav", 4800));

        const auto plan = sis::buildRenderPlan (*modelPtr, cache);
        expect ("Plan enthält die Zone", plan->zones.size() == 1);
        expect ("Nur geladene Samples werden gespielt", plan->getNumLayers() == 1);
        expect ("Zone greift für Note und Velocity", plan->zoneFor (60, 100) != nullptr);
        expect ("Note außerhalb der Zone greift nicht", plan->zoneFor (12, 100) == nullptr);
    }

    // --- Stumm- und Solo-Schaltung ---------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("a.wav", 4800));
        cache.insert (makeConstantSample ("b.wav", 4800));

        auto muted = makeTrack ("a.wav");
        muted.mute = true;
        const auto modelPtr = makeModel ({ muted, makeTrack ("b.wav") });
        expect ("Stummgeschaltete Spur fällt weg", sis::buildRenderPlan (*modelPtr, cache)->getNumLayers() == 1);

        auto solo = makeTrack ("b.wav");
        solo.solo = true;
        const auto soloModelPtr = makeModel ({ makeTrack ("a.wav"), solo });
        expect ("Solo lässt nur die Solo-Spur übrig", sis::buildRenderPlan (*soloModelPtr, cache)->getNumLayers() == 1);
    }

    // --- Grundton spielt in Originalgeschwindigkeit, Velocity skaliert ----------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("c.wav", 48000));
        const auto modelPtr = makeModel ({ makeTrack ("c.wav") });
        auto engine = makeEngine (*modelPtr, cache);

        const auto full = render (*engine, 4800, 60, 1.0f);
        expectNear ("Note auf dem Grundton spielt mit vollem Pegel", full.at (100), 1.0, 0.02);

        engine->reset();
        const auto half = render (*engine, 4800, 60, 0.5f);
        expectNear ("Velocity 0,5 halbiert den Pegel", half.at (100), 0.5, 0.02);
    }

    // --- Ohne Note bleibt es still ---------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("c.wav", 48000));
        auto engine = makeEngineFor (makeModel ({ makeTrack ("c.wav") }), cache);

        const auto rendered = render (*engine, 2048, 60, 1.0f, 1024);
        expect ("Vor dem Anschlag ist es still", rendered.peak (0, 1000) < 1.0e-6f);
        expect ("Nach dem Anschlag klingt es", rendered.peak (1100, 900) > 0.5f);
        expect ("Stimme wird gezählt", engine->getActiveVoiceCount() == 1);
    }

    // --- Tonhöhe: eine Oktave höher spielt doppelt so schnell -------------------
    {
        sis::SampleCache cache;
        cache.insert (makeRampSample ("ramp.wav", 48000));
        const auto modelPtr = makeModel ({ makeTrack ("ramp.wav") });

        auto engine = makeEngine (*modelPtr, cache);
        const auto root = render (*engine, 2048, 60, 1.0f);

        auto engineUp = makeEngine (*modelPtr, cache);
        const auto octave = render (*engineUp, 2048, 72, 1.0f);

        expectNear ("Oktave höher: doppelte Abspielgeschwindigkeit",
                    (double) octave.at (1000) / juce::jmax (1.0e-6, (double) root.at (1000)), 2.0, 0.05);

        auto engineDown = makeEngine (*modelPtr, cache);
        const auto lower = render (*engineDown, 2048, 48, 1.0f);
        expectNear ("Oktave tiefer: halbe Abspielgeschwindigkeit",
                    (double) lower.at (1000) / juce::jmax (1.0e-6, (double) root.at (1000)), 0.5, 0.05);
    }

    // --- Spur-Tonhöhe in Halbtönen und Cent ------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeRampSample ("ramp.wav", 48000));

        auto track = makeTrack ("ramp.wav");
        track.pitch = 12;
        auto engine = makeEngineFor (makeModel ({ track }), cache);
        const auto shifted = render (*engine, 2048, 60, 1.0f);

        auto plain = makeEngineFor (makeModel ({ makeTrack ("ramp.wav") }), cache);
        const auto normal = render (*plain, 2048, 60, 1.0f);

        expectNear ("+12 Halbtöne an der Spur verdoppeln das Tempo",
                    (double) shifted.at (1000) / juce::jmax (1.0e-6, (double) normal.at (1000)), 2.0, 0.05);
    }

    // --- Panorama --------------------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("c.wav", 48000));

        auto left = makeTrack ("c.wav");
        left.pan = -1.0f;
        auto engine = makeEngineFor (makeModel ({ left }), cache);
        const auto rendered = render (*engine, 1024, 60, 1.0f);

        expect ("Ganz links: linker Kanal klingt", rendered.buffer.getSample (0, 500) > 0.9f);
        expect ("Ganz links: rechter Kanal ist still",
                std::abs (rendered.buffer.getSample (1, 500)) < 1.0e-5f);
    }

    // --- Fade-in ---------------------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("c.wav", 48000));

        auto track = makeTrack ("c.wav");
        track.clips[0].fadeIn = 0.5;    // halbe Ausschnittlänge = 24000 Samples
        auto engine = makeEngineFor (makeModel ({ track }), cache);
        const auto rendered = render (*engine, 24000, 60, 1.0f);

        expect ("Fade-in beginnt bei null", std::abs (rendered.at (0)) < 0.01f);
        expectNear ("Fade-in ist nach einem Viertel halb offen", rendered.at (12000), 0.5, 0.02);
        expectNear ("Fade-in ist am Ende offen", rendered.at (23900), 0.995, 0.02);
    }

    // --- Ausschnitt (Trim) begrenzt die Wiedergabe ------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeRampSample ("ramp.wav", 48000));

        auto track = makeTrack ("ramp.wav");
        track.clips[0].trimStart = 0.5;
        track.loop = sis::LoopMode::oneShot;
        auto engine = makeEngineFor (makeModel ({ track }), cache);
        // erst nach der kurzen Einschwingzeit der Hüllkurve messen (1 ms Klickschutz)
        const auto rendered = render (*engine, 2048, 60, 1.0f);

        expectNear ("Wiedergabe startet in der Mitte des Samples", rendered.at (500), 0.5104, 0.01);
    }

    // --- One-Shot endet, Sustain-Loop läuft weiter -------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("short.wav", 4800));   // 0,1 s

        auto oneShot = makeTrack ("short.wav");
        oneShot.loop = sis::LoopMode::oneShot;
        auto engineOnce = makeEngineFor (makeModel ({ oneShot }), cache);
        const auto once = render (*engineOnce, 24000, 60, 1.0f);

        expect ("One-Shot klingt am Anfang", once.peak (0, 4000) > 0.5f);
        expect ("One-Shot endet mit dem Sample", once.peak (8000, 8000) < 1.0e-5f);
        expect ("One-Shot gibt die Stimme frei", engineOnce->getActiveVoiceCount() == 0);

        auto engineLoop = makeEngineFor (makeModel ({ makeTrack ("short.wav") }), cache);
        const auto looped = render (*engineLoop, 24000, 60, 1.0f);
        expect ("Sustain-Loop klingt weiter", looped.peak (16000, 4000) > 0.5f);
    }

    // --- Loslassen: Release blendet aus -----------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("c.wav", 48000));

        auto modelPtr = makeModel ({ makeTrack ("c.wav") });
        modelPtr->envelope.release = 0.02f;   // 0,1 s
        auto engine = makeEngine (*modelPtr, cache);

        const auto rendered = render (*engine, 24000, 60, 1.0f, 0, 4800);
        expect ("Vor dem Loslassen klingt es", rendered.peak (2000, 1000) > 0.5f);
        expect ("Kurz nach dem Loslassen klingt es leiser",
                rendered.peak (9000, 1000) < rendered.peak (2000, 1000));
        expect ("Nach dem Release ist es still", rendered.peak (14000, 4000) < 1.0e-4f);
        expect ("Stimme ist wieder frei", engine->getActiveVoiceCount() == 0);
    }

    // --- Versatz der Spur auf der Zeitachse --------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("c.wav", 48000));

        auto track = makeTrack ("c.wav");
        track.clips[0].offset = 0.0125;   // 0,1 s der 8-Sekunden-Achse = 4800 Samples
        auto engine = makeEngineFor (makeModel ({ track }), cache);
        const auto rendered = render (*engine, 12000, 60, 1.0f);

        expect ("Vor dem Versatz ist die Spur still", rendered.peak (0, 4000) < 1.0e-6f);
        expect ("Nach dem Versatz klingt sie", rendered.peak (5200, 2000) > 0.5f);
    }

    // --- Velocity wählt die Zone --------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("leise.wav", 48000, 0.25f));
        cache.insert (makeConstantSample ("laut.wav", 48000, 1.0f));

        auto modelPtr = makeModel ({ makeTrack ("leise.wav") });
        modelPtr->zones[0].highVelocity = 63;

        sis::Zone hard = modelPtr->zones[0];
        hard.id = "z2";
        hard.lowVelocity = 64;
        hard.highVelocity = 127;
        hard.tracks = { makeTrack ("laut.wav") };
        modelPtr->zones.push_back (hard);

        auto soft = makeEngine (*modelPtr, cache);
        const auto quiet = render (*soft, 1024, 60, 0.2f);      // Velocity 25
        expectNear ("Leise Velocity spielt die leise Zone", quiet.at (500), 0.25 * 0.2, 0.02);

        auto loud = makeEngine (*modelPtr, cache);
        const auto strong = render (*loud, 1024, 60, 1.0f);     // Velocity 127
        expectNear ("Hohe Velocity spielt die laute Zone", strong.at (500), 1.0, 0.02);
    }

    // --- Mehrere Spuren mischen sich ----------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("a.wav", 48000, 0.3f));
        cache.insert (makeConstantSample ("b.wav", 48000, 0.4f));

        auto engine = makeEngineFor (makeModel ({ makeTrack ("a.wav"), makeTrack ("b.wav") }), cache);
        const auto rendered = render (*engine, 1024, 60, 1.0f);

        // Panorama Mitte lässt den Pegel unverändert
        expectNear ("Spuren liegen übereinander", rendered.at (500), 0.3 + 0.4, 0.02);
    }

    // --- Polyphonie ----------------------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("c.wav", 48000));
        auto engine = makeEngineFor (makeModel ({ makeTrack ("c.wav") }), cache);

        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, 1.0f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, 1.0f), 0);
        engine->process (buffer, midi);

        expect ("Drei Noten ergeben drei Stimmen", engine->getActiveVoiceCount() == 3);

        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        buffer.clear();
        engine->process (buffer, off);
        expect ("Alle Noten aus beendet die Stimmen", engine->getActiveVoiceCount() == 0);
    }

    // --- Time-Stretch: Dauer ändert sich, Tonhöhe bleibt ------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 24000, 1000.0));   // 0,5 s, 1000 Hz

        auto normal = makeTrack ("sine.wav");
        normal.loop = sis::LoopMode::oneShot;

        auto stretched = normal;
        stretched.clips[0].stretch = 2.0;

        auto squeezed = normal;
        squeezed.clips[0].stretch = 0.5;

        auto plainEngine = makeEngineFor (makeModel ({ normal }), cache);
        const auto plain = render (*plainEngine, 72000, 60, 1.0f);

        auto longEngine = makeEngineFor (makeModel ({ stretched }), cache);
        const auto longer = render (*longEngine, 72000, 60, 1.0f);

        auto shortEngine = makeEngineFor (makeModel ({ squeezed }), cache);
        const auto shorter = render (*shortEngine, 72000, 60, 1.0f);

        expectNear ("Ohne Dehnung endet es nach 0,5 s", (double) plain.lastSoundingSample(), 24000.0, 600.0);
        expectNear ("2× gedehnt dauert es doppelt so lang", (double) longer.lastSoundingSample(), 48000.0, 1200.0);
        expectNear ("0,5× gestaucht dauert es halb so lang", (double) shorter.lastSoundingSample(), 12000.0, 600.0);

        // 1000 Hz ergeben 2000 Nulldurchgänge je Sekunde
        const int expectedCrossings = plain.zeroCrossings (2000, 8000);
        expectNear ("Dehnen lässt die Tonhöhe unverändert",
                    longer.zeroCrossings (2000, 8000), expectedCrossings, 6.0);
        expectNear ("Stauchen lässt die Tonhöhe unverändert",
                    shorter.zeroCrossings (2000, 8000), expectedCrossings, 6.0);

        // Der Pegel darf durch die Überblendung nicht einbrechen
        expect ("Gedehnt bleibt der Pegel erhalten", longer.peak (20000, 4000) > 0.7f);
    }

    // --- Dehnen und Transponieren zusammen --------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 24000, 1000.0));

        auto track = makeTrack ("sine.wav");
        track.loop = sis::LoopMode::oneShot;
        track.clips[0].stretch = 2.0;

        auto engine = makeEngineFor (makeModel ({ track }), cache);
        const auto rendered = render (*engine, 72000, 72, 1.0f);   // eine Oktave höher

        // Tonhöhe verdoppelt (r = 2), Zeit verdoppelt (S = 2) -> Dauer wie im Original
        expectNear ("Oktave höher und 2× gedehnt: Dauer wie im Original",
                    (double) rendered.lastSoundingSample(), 24000.0, 1500.0);

        auto plainEngine = makeEngineFor (makeModel ({ makeTrack ("sine.wav") }), cache);
        const auto plain = render (*plainEngine, 24000, 60, 1.0f);
        expectNear ("Oktave höher verdoppelt die Frequenz",
                    rendered.zeroCrossings (2000, 8000) / juce::jmax (1.0, (double) plain.zeroCrossings (2000, 8000)),
                    2.0, 0.08);
    }

    // --- Kette wie im Prozessor: Bildschirm-Klaviatur → MIDI → Engine -----------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("c.wav", 48000));
        auto engine = makeEngineFor (makeModel ({ makeTrack ("c.wav") }), cache);

        juce::MidiKeyboardState keyboard;
        juce::AudioBuffer<float> buffer (2, 512);

        auto renderBlock = [&engine, &keyboard, &buffer]
        {
            juce::MidiBuffer midi;
            keyboard.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
            buffer.clear();
            engine->process (buffer, midi);
            return buffer.getMagnitude (0, buffer.getNumSamples());
        };

        expect ("Ohne Tastendruck bleibt es still", renderBlock() < 1.0e-6f);

        keyboard.noteOn (1, 60, 0.8f);
        const float pressed = renderBlock();
        expect ("Tastendruck erzeugt Klang", pressed > 0.5f);
        expect ("Tastendruck erzeugt eine Stimme", engine->getActiveVoiceCount() == 1);

        keyboard.noteOff (1, 60, 0.0f);
        renderBlock();
        expect ("Loslassen beendet die Stimme", engine->getActiveVoiceCount() == 0);
    }

    // --- Datei laden und spielen ----------------------------------------------------
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("sis_engine_test.wav");
        {
            juce::AudioBuffer<float> source (1, 24000);
            for (int i = 0; i < source.getNumSamples(); ++i)
                source.setSample (0, i, 0.5f);

            juce::WavAudioFormat wav;
            file.deleteFile();
            std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> writer (
                wav.createWriterFor (stream.get(), testSampleRate, 1, 24, {}, 0));

            if (writer != nullptr)
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (source, 0, source.getNumSamples());
            }
        }

        sis::SampleCache cache;
        const auto loaded = cache.load (file, formats);
        expect ("WAV-Datei wird geladen", loaded != nullptr && loaded->getNumSamples() == 24000);

        if (loaded != nullptr)
        {
            auto modelPtr = makeModel ({ makeTrack (file.getFileName()) });
            auto engine = makeEngine (*modelPtr, cache);
            const auto rendered = render (*engine, 2048, 60, 1.0f);
            expectNear ("Geladene Datei klingt mit ihrem Pegel", rendered.at (500), 0.5, 0.02);
        }

        file.deleteFile();
    }

    // --- Equalizer -------------------------------------------------------------------
    {
        constexpr int length = 8192;

        /* Sinus erzeugen, durch den Filter schicken und den Pegel messen.
           Gemessen wird die zweite Haelfte, damit das Filter eingeschwungen ist. */
        const auto measure = [] (sis::dsp::Equalizer& eq, double frequency)
        {
            juce::AudioBuffer<float> buffer (1, length);

            for (int i = 0; i < length; ++i)
                buffer.setSample (0, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                          * frequency * i / testSampleRate));

            eq.reset();
            eq.process (buffer);

            /* Effektivwert statt Spitzenwert: bei hohen Frequenzen liegen zu wenige
               Abtastwerte auf einer Periode, der Spitzenwert waere zu klein. */
            return buffer.getRMSLevel (0, length / 2, length / 2) * juce::MathConstants<float>::sqrt2;
        };

        {
            sis::dsp::Equalizer eq;
            eq.prepare (testSampleRate, 1, 512);
            expectNear ("Flacher EQ laesst 1 kHz unveraendert", measure (eq, 1000.0), 1.0, 0.02);
        }

        {
            sis::dsp::Equalizer eq;
            eq.prepare (testSampleRate, 1, 512);

            sis::dsp::BandSettings band;
            band.type = sis::dsp::BandType::peak;
            band.frequency = 1000.0f;
            band.gainDb = 12.0f;
            band.q = 1.0f;
            eq.setBand (1, band);

            expectNear ("+12 dB bei 1 kHz vervierfachen den Pegel", measure (eq, 1000.0), 3.98, 0.2);
            expectNear ("100 Hz bleiben davon unberuehrt", measure (eq, 100.0), 1.0, 0.05);
            expectNear ("Kurvenwert passt zur Messung", eq.getMagnitudeAt (1000.0), 3.98, 0.2);
        }

        {
            sis::dsp::Equalizer eq;
            eq.prepare (testSampleRate, 1, 512);

            sis::dsp::BandSettings band;
            band.type = sis::dsp::BandType::lowShelf;
            band.frequency = 200.0f;
            band.gainDb = -12.0f;
            eq.setBand (0, band);

            expect ("Tiefen werden abgesenkt", measure (eq, 60.0) < 0.35f);
            expectNear ("Hoehen bleiben stehen", measure (eq, 8000.0), 1.0, 0.05);
        }

        {
            sis::dsp::Equalizer eq;
            eq.prepare (testSampleRate, 1, 512);

            sis::dsp::BandSettings band;
            band.type = sis::dsp::BandType::peak;
            band.frequency = 1000.0f;
            band.gainDb = 18.0f;
            band.enabled = false;
            eq.setBand (1, band);

            expectNear ("Ausgeschaltetes Band laesst das Signal in Ruhe", measure (eq, 1000.0), 1.0, 0.02);
        }

        {
            sis::dsp::Equalizer eq;
            eq.prepare (testSampleRate, 1, 512);

            sis::dsp::BandSettings band;
            band.type = sis::dsp::BandType::highPass;
            band.frequency = 500.0f;
            eq.setBand (0, band);

            expect ("Hochpass daempft 50 Hz deutlich", measure (eq, 50.0) < 0.15f);
            expectNear ("Hochpass laesst 5 kHz durch", measure (eq, 5000.0), 1.0, 0.05);
        }

        {
            sis::dsp::Equalizer eq;
            eq.prepare (testSampleRate, 2, 512);

            sis::dsp::BandSettings band;
            band.type = sis::dsp::BandType::peak;
            band.frequency = 1000.0f;
            band.gainDb = 6.0f;
            eq.setBand (1, band);

            juce::AudioBuffer<float> buffer (2, length);

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < length; ++i)
                    buffer.setSample (channel, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                                    * 1000.0 * i / testSampleRate));

            eq.process (buffer);
            expectNear ("Beide Kanaele klingen gleich",
                        buffer.getRMSLevel (0, length / 2, length / 2),
                        buffer.getRMSLevel (1, length / 2, length / 2), 0.001);
        }
    }

    // --- Equalizer in der Effektkette einer Spur -------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 48000, 1000.0));

        const auto measureThroughEngine = [&cache] (float lowMidGain)
        {
            auto track = makeTrack ("sine.wav");

            if (lowMidGain >= 0.0f)
            {
                auto equalizer = sis::Effect::makeEqualizer();
                // Band 1 liegt auf 500 Hz, Band 2 auf 2,5 kHz - 1 kHz liegt dazwischen
                equalizer.parameters[1].value = lowMidGain;
                equalizer.parameters[2].value = lowMidGain;
                track.effects.push_back (equalizer);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 12000, 60, 1.0f);
            return rendered.buffer.getRMSLevel (0, 6000, 6000);
        };

        const float plain = measureThroughEngine (-1.0f);
        const float boosted = measureThroughEngine (1.0f);    // +12 dB auf beiden Mittenbändern
        const float cut = measureThroughEngine (0.0f);        // -12 dB

        expect ("Ohne Equalizer klingt die Spur", plain > 0.05f);
        expect ("Angehobene Mitten sind lauter", boosted > plain * 1.5f);
        expect ("Abgesenkte Mitten sind leiser", cut < plain * 0.7f);

        // Ein ausgeschalteter Effekt darf nichts tun
        auto track = makeTrack ("sine.wav");
        auto equalizer = sis::Effect::makeEqualizer();
        equalizer.enabled = false;
        equalizer.parameters[1].value = 1.0f;
        track.effects.push_back (equalizer);

        auto engine = makeEngineFor (makeModel ({ track }), cache);
        const auto rendered = render (*engine, 12000, 60, 1.0f);
        expectNear ("Ausgeschalteter Effekt laesst die Spur unveraendert",
                    rendered.buffer.getRMSLevel (0, 6000, 6000), plain, plain * 0.05);
    }

    // --- Kompressor ------------------------------------------------------------------
    {
        constexpr int length = 24000;   // 0,5 s

        /* Gleichanteil mit bekanntem Pegel durchschicken und den Pegel am Ende messen,
           also nach dem Einschwingen der Regelung. */
        const auto steadyLevel = [] (const sis::dsp::CompressorSettings& settings, float inputGain)
        {
            sis::dsp::Compressor compressor;
            compressor.prepare (testSampleRate, 2);
            compressor.setSettings (settings);

            juce::AudioBuffer<float> buffer (2, length);

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < length; ++i)
                    buffer.setSample (channel, i, inputGain);

            compressor.process (buffer);
            return buffer.getSample (0, length - 1);
        };

        sis::dsp::CompressorSettings settings;
        settings.thresholdDb = -20.0f;
        settings.ratio = 4.0f;
        settings.attackMs = 1.0f;
        settings.releaseMs = 50.0f;
        settings.makeupDb = 0.0f;

        // -30 dB liegt unter der Schwelle und bleibt unangetastet
        const float quietIn = juce::Decibels::decibelsToGain (-30.0f);
        expectNear ("Leises Signal bleibt unveraendert", steadyLevel (settings, quietIn), quietIn, quietIn * 0.02);

        // -4 dB: 16 dB ueber der Schwelle, bei 4:1 bleiben davon 4 dB -> -16 dB
        const float loudIn = juce::Decibels::decibelsToGain (-4.0f);
        const float loudOut = steadyLevel (settings, loudIn);
        expectNear ("4:1 reduziert 16 dB Ueberschuss auf 4 dB",
                    juce::Decibels::gainToDecibels (loudOut), -16.0, 0.5);

        // Ausgleich hebt wieder an
        settings.makeupDb = 6.0f;
        expectNear ("Ausgleich hebt um 6 dB an",
                    juce::Decibels::gainToDecibels (steadyLevel (settings, loudIn)), -10.0, 0.5);

        // Haertere Regelung drueckt staerker
        settings.makeupDb = 0.0f;
        settings.ratio = 20.0f;
        expect ("Hoeheres Verhaeltnis drueckt staerker",
                steadyLevel (settings, loudIn) < loudOut);

        // Langsames Ansprechen laesst die erste Spitze durch
        sis::dsp::CompressorSettings slow = settings;
        slow.ratio = 8.0f;
        slow.attackMs = 200.0f;

        sis::dsp::Compressor compressor;
        compressor.prepare (testSampleRate, 2);
        compressor.setSettings (slow);

        juce::AudioBuffer<float> buffer (2, length);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < length; ++i)
                buffer.setSample (channel, i, loudIn);

        compressor.process (buffer);
        expect ("Langsames Ansprechen laesst den Anfang durch", buffer.getSample (0, 10) > loudIn * 0.9f);
        expect ("Spaeter wird dennoch geregelt", buffer.getSample (0, length - 1) < loudIn * 0.6f);
        expect ("Pegelabsenkung wird gemeldet", compressor.getGainReductionDb() < -1.0f);
    }

    // --- Kompressor und Hall in der Effektkette ---------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 48000, 440.0));

        const auto renderWith = [&cache] (const std::vector<sis::Effect>& effects, int numSamples)
        {
            auto track = makeTrack ("sine.wav");
            track.effects = effects;
            auto engine = makeEngineFor (makeModel ({ track }), cache);
            return render (*engine, numSamples, 60, 1.0f);
        };

        // Kompressor mit tiefer Schwelle und hohem Verhaeltnis macht leiser
        auto compressor = sis::Effect::makeCompressor();
        compressor.parameters[0].value = 0.1f;    // Schwelle sehr tief
        compressor.parameters[1].value = 1.0f;    // 20:1
        compressor.parameters[2].value = 0.0f;    // schnelles Ansprechen
        compressor.parameters[4].value = 0.0f;    // kein Ausgleich

        const auto plain = renderWith ({}, 12000);
        const auto compressed = renderWith ({ compressor }, 12000);

        const float plainLevel = plain.buffer.getRMSLevel (0, 6000, 6000);
        const float compressedLevel = compressed.buffer.getRMSLevel (0, 6000, 6000);

        expect ("Ohne Effekt klingt die Spur", plainLevel > 0.05f);
        expect ("Kompressor senkt den Pegel", compressedLevel < plainLevel * 0.6f);

        // Hall klingt nach dem Ende der Note weiter
        auto reverb = sis::Effect::makeReverb();
        reverb.parameters[0].value = 0.9f;   // grosser Raum
        reverb.parameters[3].value = 0.8f;   // viel Hallanteil

        auto shortTrack = makeTrack ("sine.wav");
        shortTrack.loop = sis::LoopMode::oneShot;
        shortTrack.clips[0].trimEnd = 0.1;            // nur 0,1 s Sample

        auto dryEngine = makeEngineFor (makeModel ({ shortTrack }), cache);
        const auto dry = render (*dryEngine, 24000, 60, 1.0f);

        auto wetTrack = shortTrack;
        wetTrack.effects = { reverb };
        auto wetEngine = makeEngineFor (makeModel ({ wetTrack }), cache);
        const auto wet = render (*wetEngine, 24000, 60, 1.0f);

        const float dryTail = dry.buffer.getRMSLevel (0, 12000, 8000);
        const float wetTail = wet.buffer.getRMSLevel (0, 12000, 8000);

        expect ("Ohne Hall ist es nach dem Sample still", dryTail < 1.0e-4f);
        expect ("Mit Hall klingt es nach", wetTail > 1.0e-3f);
    }

    // --- Ein externer Effekt uebersteht Speichern und Laden ------------------------
    {
        auto track = makeTrack ("sine.wav");
        auto external = sis::Effect::makeExternal ("HOFA IQ-EQ", "VST3-1234-5678-9abc");
        external.pluginState = "AAECAwQFBgc=";   // wie ein Base64-Zustand aus dem Plugin
        external.enabled = false;
        track.effects = { sis::Effect::makeEqualizer(), external };

        const auto modelPtr = makeModel ({ track });
        const auto tree = modelPtr->toValueTree();

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        sis::InstrumentModel loaded;
        expect ("Modell mit externem Effekt laedt", loaded.fromValueTree (tree, formats, juce::File()));

        const auto& effects = loaded.zones.front().tracks.front().effects;
        expect ("Beide Effekte sind da", effects.size() == 2);

        if (effects.size() == 2)
        {
            expect ("Der Equalizer bleibt ein Equalizer", effects[0].type == sis::EffectType::equalizer);
            expect ("Der fremde Effekt bleibt extern", effects[1].type == sis::EffectType::external);
            expect ("Sein Name bleibt", effects[1].name == "HOFA IQ-EQ");
            expect ("Seine Kennung bleibt", effects[1].pluginIdentifier == "VST3-1234-5678-9abc");
            expect ("Sein Zustand bleibt", effects[1].pluginState == "AAECAwQFBgc=");
            expect ("Sein Aus-Zustand bleibt", ! effects[1].enabled);
        }
    }

    // --- Kanalfilter ----------------------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("tief.wav", 48000, 80.0));     // klar unter dem Hochpass
        cache.insert (makeSineSample ("hoch.wav", 48000, 6000.0));   // klar ueber dem Tiefpass

        const auto throughHighPass = [&cache] (const juce::String& clip, bool withFilter)
        {
            auto track = makeTrack (clip);

            if (withFilter)
            {
                auto filter = sis::Effect::makeChannelFilter();
                filter.parameters[0].value = 1.0f;   // Hochpass ganz nach rechts: 2 kHz
                filter.parameters[1].value = 1.0f;   // Tiefpass offen
                track.effects.push_back (filter);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 24000, 60, 1.0f);
            return rendered.buffer.getRMSLevel (0, 12000, 12000);
        };

        const float lowPlain = throughHighPass ("tief.wav", false);
        const float lowFiltered = throughHighPass ("tief.wav", true);
        const float highPlain = throughHighPass ("hoch.wav", false);
        const float highFiltered = throughHighPass ("hoch.wav", true);

        expect ("Ohne Filter klingt der tiefe Ton", lowPlain > 0.2f);
        expect ("Der Hochpass nimmt die Tiefen weg", lowFiltered < lowPlain * 0.2f);
        expect ("Den hohen Ton laesst er durch", highFiltered > highPlain * 0.7f);

        // Beide Regler am Anschlag: der Filter gehoert gar nicht erst in die Kette
        auto track = makeTrack ("tief.wav");
        track.effects.push_back (sis::Effect::makeChannelFilter());   // Voreinstellung = offen

        auto engine = makeEngineFor (makeModel ({ track }), cache);
        const auto rendered = render (*engine, 24000, 60, 1.0f);
        expectNear ("Offener Filter laesst die Spur unveraendert",
                    rendered.buffer.getRMSLevel (0, 12000, 12000), lowPlain, lowPlain * 0.05);
    }

    // --- Saettigung -------------------------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 48000, 400.0));

        /* Ein Sinus hat ein festes Verhaeltnis von Effektiv- zu Spitzenwert (1/Wurzel 2,
           also rund 0,707). Je staerker die Saettigung, desto rechteckiger die Welle -
           und desto naeher rueckt das Verhaeltnis an 1. Das misst die Verformung
           selbst, nicht die Lautstaerke. */
        const auto crestRatio = [&cache] (float drive, float mix)
        {
            auto track = makeTrack ("sine.wav");

            if (drive >= 0.0f)
            {
                auto saturation = sis::Effect::makeSaturation();
                saturation.parameters[0].value = drive;
                saturation.parameters[1].value = mix;
                track.effects.push_back (saturation);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 24000, 60, 1.0f);

            const float rms = rendered.buffer.getRMSLevel (0, 12000, 12000);
            const float peak = rendered.peak (12000, 12000);
            return peak > 0.0f ? rms / peak : 0.0f;
        };

        const float plain = crestRatio (-1.0f, 1.0f);
        const float driven = crestRatio (1.0f, 1.0f);     // volle 24 dB
        const float bypassed = crestRatio (1.0f, 0.0f);   // Menge auf null

        expectNear ("Ohne Saettigung bleibt es ein Sinus", plain, 0.707, 0.02);
        expect ("Mit Saettigung wird die Welle eckiger", driven > plain + 0.1f);
        expect ("Aber nicht ganz rechteckig", driven < 1.0f);
        expectNear ("Menge auf null laesst den Sinus in Ruhe", bypassed, plain, 0.02);
    }

    // --- Transienten ------------------------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("c.wav", 48000));   // setzt hart ein

        const auto shaped = [&cache] (float attack, float sustain)
        {
            auto track = makeTrack ("c.wav");

            if (attack >= 0.0f)
            {
                auto transients = sis::Effect::makeTransients();
                transients.parameters[0].value = attack;
                transients.parameters[1].value = sustain;
                track.effects.push_back (transients);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            return render (*engine, 24000, 60, 1.0f);
        };

        const auto neutral = shaped (-1.0f, 0.5f);
        const auto boosted = shaped (1.0f, 0.5f);     // Anschlag voll angehoben
        const auto damped = shaped (0.0f, 0.5f);      // Anschlag voll zurueckgenommen

        // Der Einsatz liegt in den ersten Millisekunden nach der Note
        const float neutralOnset = neutral.peak (0, 600);
        const float boostedOnset = boosted.peak (0, 600);
        const float dampedOnset = damped.peak (0, 600);

        expect ("Ohne Former klingt der Einsatz", neutralOnset > 0.5f);
        expect ("Angehobener Anschlag ist lauter", boostedOnset > neutralOnset * 1.2f);
        expect ("Zurueckgenommener Anschlag ist leiser", dampedOnset < neutralOnset * 0.8f);

        // Weit hinter dem Einsatz steht der Pegel wieder, wo er war
        expectNear ("Spaeter wirkt der Anschlagregler nicht mehr",
                    boosted.peak (12000, 6000), neutral.peak (12000, 6000), 0.05);
    }

    // --- Chorus -----------------------------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 48000, 440.0));

        /* Eine modulierte Verzoegerung, die dem trockenen Signal beigemischt wird, erzeugt
           wandernde Ausloeschungen - der Pegel atmet. Genau das wird gemessen: die
           Schwankung des Effektivwerts ueber mehrere Fenster. */
        const auto levelSwing = [&cache] (float mix, float depth)
        {
            auto track = makeTrack ("sine.wav");

            if (mix >= 0.0f)
            {
                auto chorus = sis::Effect::makeChorus();
                chorus.parameters[0].value = depth;
                chorus.parameters[1].value = 1.0f;    // schnellste Modulation
                chorus.parameters[3].value = mix;
                track.effects.push_back (chorus);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 48000, 60, 1.0f);

            float lowest = 1.0e9f, highest = 0.0f;

            for (int window = 8000; window + 2000 <= 48000; window += 2000)
            {
                const float level = rendered.buffer.getRMSLevel (0, window, 2000);
                lowest = juce::jmin (lowest, level);
                highest = juce::jmax (highest, level);
            }

            return highest - lowest;
        };

        const float plainSwing = levelSwing (-1.0f, 0.0f);
        const float chorusSwing = levelSwing (0.5f, 1.0f);
        const float bypassedSwing = levelSwing (0.0f, 1.0f);

        expect ("Ein glatter Sinus schwankt nicht", plainSwing < 0.02f);
        expect ("Mit Chorus atmet der Pegel", chorusSwing > 0.05f);
        expect ("Menge auf null laesst ihn glatt", bypassedSwing < 0.02f);
    }

    // --- Bit-Crusher ------------------------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 48000, 300.0));

        /* Bei grober Aufloesung kann das Ergebnis nur noch wenige verschiedene Werte
           annehmen - die zaehlt der Test. Verglichen wird gegen den unbearbeiteten
           Sinus: der hat von sich aus nur so viele Werte, wie eine Periode Samples
           hat, eine feste Zahl waere hier also willkuerlich. */
        const auto distinctValues = [&cache] (bool withCrusher, float bits, float mix)
        {
            auto track = makeTrack ("sine.wav");

            if (withCrusher)
            {
                auto crusher = sis::Effect::makeBitCrusher();
                crusher.parameters[0].value = bits;
                crusher.parameters[1].value = 0.0f;   // keine Unterabtastung
                crusher.parameters[2].value = mix;
                track.effects.push_back (crusher);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 24000, 60, 1.0f);

            std::set<int> levels;

            for (int i = 12000; i < 24000; ++i)
                levels.insert (juce::roundToInt (rendered.at (i) * 10000.0f));

            return (int) levels.size();
        };

        const int plainLevels = distinctValues (false, 0.0f, 0.0f);
        expect ("Der nackte Sinus hat schon wenige Werte", plainLevels > 40);

        expect ("Grob quantisiert bleiben wenige Stufen uebrig",
                distinctValues (true, 1.0f, 1.0f) <= 8);
        expect ("Fein quantisiert bleibt alles erhalten",
                distinctValues (true, 0.0f, 1.0f) == plainLevels);
        expect ("Menge auf null laesst das Signal ganz",
                distinctValues (true, 1.0f, 0.0f) == plainLevels);

        // Unterabtastung haelt Werte fest: benachbarte Samples sind oefter gleich
        auto track = makeTrack ("sine.wav");
        auto crusher = sis::Effect::makeBitCrusher();
        crusher.parameters[0].value = 0.0f;    // volle Aufloesung
        crusher.parameters[1].value = 1.0f;    // groebste Abtastung
        crusher.parameters[2].value = 1.0f;
        track.effects.push_back (crusher);

        auto engine = makeEngineFor (makeModel ({ track }), cache);
        const auto rendered = render (*engine, 24000, 60, 1.0f);

        int repeats = 0;
        for (int i = 12001; i < 24000; ++i)
            if (juce::approximatelyEqual (rendered.at (i), rendered.at (i - 1)))
                ++repeats;

        expect ("Unterabtastung haelt Werte fest", repeats > 9000);
    }

    // --- Overdrive: unsymmetrisch, also auch geradzahlige Obertoene --------------------
    {
        sis::SampleCache cache;
        constexpr double fundamental = 500.0;
        cache.insert (makeSineSample ("sine.wav", 48000, fundamental));

        /* Eine symmetrische Kennlinie erzeugt nur ungeradzahlige Obertoene. Der zweite
           Oberton ist deshalb der Beweis, dass die Unsymmetrie wirkt - und nicht bloss
           irgendetwas lauter wurde. */
        struct Harmonics { double first, second, third; };

        const auto harmonicsOf = [&cache] (float character, float drive, float mix)
        {
            auto track = makeTrack ("sine.wav");

            if (drive >= 0.0f)
            {
                auto overdrive = sis::Effect::makeOverdrive();
                overdrive.parameters[0].value = drive;
                overdrive.parameters[1].value = character;
                overdrive.parameters[2].value = 1.0f;   // Klangregler offen, sonst verfaelscht er
                overdrive.parameters[3].value = mix;
                overdrive.parameters[4].value = 0.5f;
                track.effects.push_back (overdrive);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 48000, 60, 1.0f);

            return Harmonics { magnitudeAt (rendered.buffer, fundamental, 24000, 24000),
                               magnitudeAt (rendered.buffer, 2.0 * fundamental, 24000, 24000),
                               magnitudeAt (rendered.buffer, 3.0 * fundamental, 24000, 24000) };
        };

        const auto plain = harmonicsOf (0.0f, -1.0f, 1.0f);      // ohne Effekt
        const auto symmetric = harmonicsOf (0.0f, 1.0f, 1.0f);   // voll gefahren, symmetrisch
        const auto asymmetric = harmonicsOf (1.0f, 1.0f, 1.0f);  // voll gefahren, unsymmetrisch

        expect ("Der reine Sinus hat einen Grundton", plain.first > 0.5);
        expect ("Und praktisch keine Obertoene", plain.second < plain.first * 0.02
                                                 && plain.third < plain.first * 0.02);

        expect ("Symmetrisch uebersteuert entstehen ungeradzahlige Obertoene",
                symmetric.third > symmetric.first * 0.1);
        expect ("Aber kaum geradzahlige",
                symmetric.second < symmetric.first * 0.02);

        expect ("Unsymmetrisch entsteht der zweite Oberton",
                asymmetric.second > asymmetric.first * 0.1);
        expect ("Deutlich mehr als symmetrisch",
                asymmetric.second > symmetric.second * 5.0);

        // Menge auf null laesst das Signal in Ruhe
        const auto bypassed = harmonicsOf (1.0f, 1.0f, 0.0f);
        expect ("Menge auf null laesst den Sinus sauber", bypassed.second < bypassed.first * 0.02);
    }

    // --- Distortion: die Kante macht aus dem Sinus ein Rechteck -----------------------
    {
        sis::SampleCache cache;
        constexpr double fundamental = 400.0;
        cache.insert (makeSineSample ("sine.wav", 48000, fundamental));

        const auto rendered = [&cache] (bool withEffect, float drive, float edge, float mix)
        {
            auto track = makeTrack ("sine.wav");

            if (withEffect)
            {
                auto distortion = sis::Effect::makeDistortion();
                distortion.parameters[0].value = drive;
                distortion.parameters[1].value = edge;
                distortion.parameters[2].value = 1.0f;   // Klangregler offen
                distortion.parameters[3].value = mix;
                distortion.parameters[4].value = 0.5f;
                track.effects.push_back (distortion);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            return render (*engine, 48000, 60, 1.0f);
        };

        /* Erstens: die Welle wird eckiger. Ein Sinus hat ein festes Verhaeltnis von
           Effektiv- zu Spitzenwert (rund 0,707), ein Rechteck liegt bei 1. */
        const auto crestOf = [] (const Rendered& r)
        {
            const float peak = r.peak (24000, 24000);
            return peak > 0.0f ? r.buffer.getRMSLevel (0, 24000, 24000) / peak : 0.0f;
        };

        const float sineCrest = crestOf (rendered (false, 0.0f, 0.0f, 1.0f));
        const float hardCrest = crestOf (rendered (true, 1.0f, 1.0f, 1.0f));
        const float bypassedCrest = crestOf (rendered (true, 1.0f, 1.0f, 0.0f));

        expectNear ("Ohne Effekt bleibt es ein Sinus", sineCrest, 0.707, 0.02);
        expect ("Voll verzerrt wird daraus fast ein Rechteck", hardCrest > 0.9f);
        expect ("Menge auf null laesst ihn in Ruhe", std::abs (bypassedCrest - sineCrest) < 0.02f);

        /* Zweitens: die Kante tut, was sie verspricht. Gemessen wird bei massvoller
           Verstaerkung - bei voller lieferte auch der Tangens hyperbolicus ein Rechteck
           und die Kante machte nichts mehr aus.

           Einzelne Obertoene taugen dafuer nicht: ihre Pegel durchlaufen beim Verzerren
           Nullstellen, der siebte ist bei schwacher Verstaerkung hart sogar leiser als
           weich. Wie eckig die Welle insgesamt ist, steigt dagegen durchweg an. */
        const float softCrest = crestOf (rendered (true, 0.2f, 0.0f, 1.0f));
        const float sharpCrest = crestOf (rendered (true, 0.2f, 1.0f, 1.0f));

        expect ("Auch weich wird es eckiger als ein Sinus", softCrest > sineCrest + 0.02f);
        expect ("Mit voller Kante noch deutlich mehr", sharpCrest > softCrest + 0.02f);
    }

    // --- Der Klangregler nimmt die Hoehen zurueck -------------------------------------
    {
        sis::SampleCache cache;
        constexpr double fundamental = 400.0;
        cache.insert (makeSineSample ("sine.wav", 48000, fundamental));

        const auto thirdHarmonic = [&cache] (float tone)
        {
            auto track = makeTrack ("sine.wav");

            auto distortion = sis::Effect::makeDistortion();
            distortion.parameters[0].value = 1.0f;
            distortion.parameters[1].value = 1.0f;
            distortion.parameters[2].value = tone;
            distortion.parameters[3].value = 1.0f;
            distortion.parameters[4].value = 0.5f;
            track.effects.push_back (distortion);

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 48000, 60, 1.0f);

            const double first = magnitudeAt (rendered.buffer, fundamental, 24000, 24000);
            const double fifth = magnitudeAt (rendered.buffer, 5.0 * fundamental, 24000, 24000);

            return first > 0.0 ? fifth / first : 0.0;
        };

        const double open = thirdHarmonic (1.0f);
        const double closed = thirdHarmonic (0.0f);

        expect ("Offen bleiben die Obertoene stehen", open > 0.05);
        expect ("Zugedreht sind sie deutlich leiser", closed < open * 0.5);
    }

    // --- Flanger: die Rueckkopplung macht den Unterschied -----------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 48000, 900.0));

        /* Ein wandernder Kamm aus Ausloeschungen laesst den Pegel atmen. Je mehr
           Rueckkopplung, desto schmaler die Kerben und desto groesser das Atmen -
           genau das unterscheidet den Flanger vom Chorus. */
        const auto levelSwing = [&cache] (bool withEffect, float feedback, float mix)
        {
            auto track = makeTrack ("sine.wav");

            if (withEffect)
            {
                auto flanger = sis::Effect::makeFlanger();
                flanger.parameters[0].value = 1.0f;    // volle Tiefe
                flanger.parameters[1].value = 1.0f;    // schnellste Modulation
                flanger.parameters[2].value = feedback;
                flanger.parameters[3].value = mix;
                track.effects.push_back (flanger);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 48000, 60, 1.0f);

            float lowest = 1.0e9f, highest = 0.0f;

            for (int window = 8000; window + 1000 <= 48000; window += 1000)
            {
                const float level = rendered.buffer.getRMSLevel (0, window, 1000);
                lowest = juce::jmin (lowest, level);
                highest = juce::jmax (highest, level);
            }

            return highest - lowest;
        };

        const float plain = levelSwing (false, 0.0f, 1.0f);
        const float withoutFeedback = levelSwing (true, 0.0f, 0.5f);
        const float withFeedback = levelSwing (true, 1.0f, 0.5f);
        const float bypassed = levelSwing (true, 1.0f, 0.0f);

        expect ("Ein glatter Sinus schwankt nicht", plain < 0.02f);
        expect ("Der Flanger laesst den Pegel atmen", withoutFeedback > 0.05f);
        expect ("Rueckkopplung verstaerkt das deutlich", withFeedback > withoutFeedback * 1.3f);
        expect ("Menge auf null laesst ihn glatt", bypassed < 0.02f);
    }

    // --- Vibrato: Tonhoehe schwankt, Pegel nicht --------------------------------------
    {
        sis::SampleCache cache;
        constexpr double fundamental = 600.0;
        cache.insert (makeSineSample ("sine.wav", 48000, fundamental));

        /* Der Unterschied zum Tremolo in einer Zahl: beim Vibrato bleibt der Effektivwert
           stehen, aber die Energie wandert vom Grundton in Seitenbaender ab. Beim Tremolo
           waere es genau umgekehrt. */
        struct Measured { float rms; double atFundamental; };

        const auto measure = [&cache] (bool withEffect, float depth)
        {
            auto track = makeTrack ("sine.wav");

            if (withEffect)
            {
                auto vibrato = sis::Effect::makeVibrato();
                vibrato.parameters[0].value = depth;
                vibrato.parameters[1].value = 0.6f;
                track.effects.push_back (vibrato);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 48000, 60, 1.0f);

            return Measured { rendered.buffer.getRMSLevel (0, 12000, 36000),
                              magnitudeAt (rendered.buffer, fundamental, 12000, 36000) };
        };

        const auto plain = measure (false, 0.0f);
        const auto wobbling = measure (true, 1.0f);

        expect ("Ohne Vibrato steht alles auf dem Grundton",
                plain.atFundamental > plain.rms * 1.35);   // Sinus: Spitze = Effektivwert · √2

        expect ("Mit Vibrato bleibt der Pegel stehen",
                std::abs (wobbling.rms - plain.rms) < plain.rms * 0.15f);

        expect ("Aber der Grundton duennt aus",
                wobbling.atFundamental < plain.atFundamental * 0.7);
    }

    // --- Phaser: Allpaesse aendern die Phase, nicht den Pegel -------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 48000, 500.0));

        const auto measure = [&cache] (bool withEffect, float mix, float feedback, float rate = 1.0f)
        {
            auto track = makeTrack ("sine.wav");

            if (withEffect)
            {
                auto phaser = sis::Effect::makePhaser();
                phaser.parameters[0].value = 1.0f;    // volle Tiefe
                phaser.parameters[1].value = rate;
                phaser.parameters[2].value = feedback;
                phaser.parameters[3].value = mix;
                track.effects.push_back (phaser);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 48000, 60, 1.0f);

            float lowest = 1.0e9f, highest = 0.0f;

            for (int window = 12000; window + 1000 <= 48000; window += 1000)
            {
                const float level = rendered.buffer.getRMSLevel (0, window, 1000);
                lowest = juce::jmin (lowest, level);
                highest = juce::jmax (highest, level);
            }

            return std::pair<float, float> { highest - lowest, highest };
        };

        const auto plain = measure (false, 1.0f, 0.0f);
        const auto fullyWet = measure (true, 1.0f, 0.0f);
        const auto halfWet = measure (true, 0.5f, 0.0f);
        const auto bypassed = measure (true, 0.0f, 0.0f);

        /* Der Beweis, dass es wirklich Allpaesse sind: langsam durchgestimmt und ganz
           nass veraendern sie den Pegel ueberhaupt nicht - nur die Phase. Erst das
           Beimischen des Trockensignals macht daraus Kerben. */
        const auto slowlyWet = measure (true, 1.0f, 0.0f, 0.0f);

        expect ("Langsam durchgestimmt bleibt der Pegel stehen", slowlyWet.first < 0.02f);
        expectNear ("Und zwar auf seinem alten Wert", slowlyWet.second, plain.second, plain.second * 0.05);

        /* Schnell durchgestimmt bleibt ein Rest: ein Allpass ist nur bei festen
           Koeffizienten pegeltreu, ein rasch verstimmter nicht. Gegen die Kerben
           beim Mischen faellt das aber kaum ins Gewicht. */
        expect ("Schnell durchgestimmt schwankt es etwas", fullyWet.first > slowlyWet.first);
        expect ("Aber viel weniger als beim Mischen", fullyWet.first < halfWet.first * 0.25f);

        expect ("Halb gemischt entstehen wandernde Kerben", halfWet.first > 0.05f);
        expect ("Menge auf null laesst ihn glatt", bypassed.first < 0.02f);
    }

    // --- Tremolo: der Pegel schwankt, bei voller Breite gegenlaeufig ------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 48000, 700.0));

        const auto rendered = [&cache] (bool withEffect, float depth, float width)
        {
            auto track = makeTrack ("sine.wav");

            if (withEffect)
            {
                auto tremolo = sis::Effect::makeTremolo();
                tremolo.parameters[0].value = depth;
                tremolo.parameters[1].value = 0.4f;
                tremolo.parameters[2].value = 0.0f;
                tremolo.parameters[3].value = width;
                track.effects.push_back (tremolo);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            return render (*engine, 48000, 60, 1.0f);
        };

        const auto swingOf = [] (const Rendered& r, int channel)
        {
            float lowest = 1.0e9f, highest = 0.0f;

            for (int window = 8000; window + 1000 <= 48000; window += 1000)
            {
                const float level = r.buffer.getRMSLevel (channel, window, 1000);
                lowest = juce::jmin (lowest, level);
                highest = juce::jmax (highest, level);
            }

            return highest - lowest;
        };

        expect ("Tiefe auf null laesst den Pegel stehen", swingOf (rendered (true, 0.0f, 0.0f), 0) < 0.02f);

        const auto deep = rendered (true, 1.0f, 0.0f);
        expect ("Voll aufgedreht schwankt er stark", swingOf (deep, 0) > 0.3f);

        // Beide Kanaele laufen gleich, solange die Breite null ist
        float sameMax = 0.0f;

        for (int window = 8000; window + 1000 <= 48000; window += 1000)
            sameMax = juce::jmax (sameMax, std::abs (deep.buffer.getRMSLevel (0, window, 1000)
                                                     - deep.buffer.getRMSLevel (1, window, 1000)));

        expect ("Ohne Breite bewegen sich beide Kanaele gleich", sameMax < 0.02f);

        // Bei voller Breite ist der eine laut, wenn der andere leise ist
        const auto panning = rendered (true, 1.0f, 1.0f);
        float oppositeMax = 0.0f;

        for (int window = 8000; window + 1000 <= 48000; window += 1000)
            oppositeMax = juce::jmax (oppositeMax, std::abs (panning.buffer.getRMSLevel (0, window, 1000)
                                                             - panning.buffer.getRMSLevel (1, window, 1000)));

        expect ("Mit voller Breite laufen sie gegenlaeufig", oppositeMax > 0.3f);
    }

    // --- Delay: das Signal kommt spaeter noch einmal ----------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("c.wav", 48000));

        /* Die Note wird nach 50 ms losgelassen, danach ist die Quelle still. Alles, was
           spaeter noch zu hoeren ist, kommt aus der Verzoegerungsleitung. */
        const auto withDelay = [&cache] (bool withEffect, float time, float feedback,
                                         float pingPong, float mix)
        {
            auto track = makeTrack ("c.wav");
            track.loop = sis::LoopMode::oneShot;

            if (withEffect)
            {
                auto delay = sis::Effect::makeDelay();
                delay.parameters[0].value = time;
                delay.parameters[1].value = feedback;
                delay.parameters[2].value = 0.0f;     // ohne Daempfung
                delay.parameters[3].value = pingPong;
                delay.parameters[4].value = mix;
                track.effects.push_back (delay);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            return render (*engine, 48000, 60, 1.0f, 0, 2400);
        };

        /** Erster Einsatz nach dem Verstummen der Quelle. */
        const auto firstRepeat = [] (const Rendered& r)
        {
            for (int i = 3000; i < 48000; ++i)
                if (std::abs (r.at (i)) > 0.05f)
                    return i;

            return -1;
        };

        expect ("Ohne Delay bleibt es nach der Note still", firstRepeat (withDelay (false, 0.5f, 0.0f, 0.0f, 0.5f)) < 0);
        expect ("Menge auf null ebenso", firstRepeat (withDelay (true, 0.5f, 0.5f, 0.0f, 0.0f)) < 0);

        const int early = firstRepeat (withDelay (true, 0.5f, 0.0f, 0.0f, 0.5f));
        const int late = firstRepeat (withDelay (true, 0.8f, 0.0f, 0.0f, 0.5f));

        expect ("Mit Delay kommt das Signal wieder", early > 0);
        expect ("Laengere Zeit heisst spaeter", late > early * 2);

        /* Ohne Rueckkopplung genau eine Wiederholung, mit Rueckkopplung mehrere,
           die leiser werden. */
        const auto single = withDelay (true, 0.5f, 0.0f, 0.0f, 0.5f);
        const auto repeating = withDelay (true, 0.5f, 0.8f, 0.0f, 0.5f);

        /* Bei ZEIT 0.5 sind es rund 100 ms, also 4800 Samples. Die Quelle dauert 2400
           Samples, folglich liegt die erste Wiederholung bei 4800 … 7200 und die zweite
           bei 9600 … 12000. Die Fenster sitzen mit etwas Rand darin. */
        const float singleSecond = single.peak (9800, 2000);
        const float repeatingFirst = repeating.peak (5000, 2000);
        const float repeatingSecond = repeating.peak (9800, 2000);

        expect ("Ohne Rueckkopplung bleibt es bei einer Wiederholung", singleSecond < 0.02f);
        expect ("Mit Rueckkopplung kommt eine zweite", repeatingSecond > 0.05f);
        expect ("Und die ist leiser als die erste", repeatingSecond < repeatingFirst);

        // Ping-Pong: die erste Wiederholung links, die zweite rechts
        const auto pinged = withDelay (true, 0.5f, 0.8f, 1.0f, 0.5f);

        const float firstLeft = pinged.buffer.getMagnitude (0, 5000, 2000);
        const float firstRight = pinged.buffer.getMagnitude (1, 5000, 2000);
        const float secondLeft = pinged.buffer.getMagnitude (0, 9800, 2000);
        const float secondRight = pinged.buffer.getMagnitude (1, 9800, 2000);

        expect ("Die erste Wiederholung kommt von links", firstLeft > firstRight * 3.0f);
        expect ("Die zweite von rechts", secondRight > secondLeft * 3.0f);
    }

    // --- Delay: die Daempfung macht jede Wiederholung dunkler -------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("hell.wav", 48000, 6000.0));

        const auto secondRepeat = [&cache] (float damping)
        {
            auto track = makeTrack ("hell.wav");
            track.loop = sis::LoopMode::oneShot;

            auto delay = sis::Effect::makeDelay();
            delay.parameters[0].value = 0.5f;    // rund 100 ms
            delay.parameters[1].value = 0.8f;
            delay.parameters[2].value = damping;
            delay.parameters[3].value = 0.0f;
            delay.parameters[4].value = 0.6f;
            track.effects.push_back (delay);

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 48000, 60, 1.0f, 0, 2400);

            return std::pair<float, float> { rendered.buffer.getRMSLevel (0, 5000, 2000),
                                             rendered.buffer.getRMSLevel (0, 9800, 2000) };
        };

        const auto bright = secondRepeat (0.0f);
        const auto dark = secondRepeat (1.0f);

        expect ("Ohne Daempfung kommt die erste Wiederholung hell zurueck", bright.first > 0.1f);
        expect ("Und die zweite auch noch", bright.second > bright.first * 0.5f);

        /* Mit Daempfung im Rueckweg verliert ein 6-kHz-Ton mit jeder Runde: die zweite
           Wiederholung ist deutlich leiser als ohne Daempfung. */
        expect ("Mit Daempfung verliert die zweite Wiederholung deutlich",
                dark.second < bright.second * 0.3f);
    }

    // --- Noise Gate: leise Stellen verstummen, laute nicht ----------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("laut.wav", 48000, 0.5f));
        cache.insert (makeConstantSample ("leise.wav", 48000, 0.005f));   // rund -46 dB

        const auto throughGate = [&cache] (const juce::String& clip, bool withGate)
        {
            auto track = makeTrack (clip);

            if (withGate)
            {
                auto gate = sis::Effect::makeNoiseGate();
                gate.parameters[0].value = 0.6f;    // Schwelle bei -32 dB
                gate.parameters[1].value = 0.0f;    // schnell auf
                gate.parameters[2].value = 0.0f;    // ohne Haltezeit
                gate.parameters[3].value = 0.0f;    // schnell zu
                gate.parameters[4].value = 1.0f;    // ganz zu
                track.effects.push_back (gate);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 24000, 60, 1.0f);

            // Am Ende messen, wenn die Regelung eingeschwungen ist
            return rendered.buffer.getRMSLevel (0, 18000, 6000);
        };

        const float loudPlain = throughGate ("laut.wav", false);
        const float loudGated = throughGate ("laut.wav", true);
        const float quietPlain = throughGate ("leise.wav", false);
        const float quietGated = throughGate ("leise.wav", true);

        expect ("Ueber der Schwelle laesst das Gate durch", loudGated > loudPlain * 0.95f);
        expect ("Das leise Signal war vorher da", quietPlain > 0.001f);
        expect ("Unter der Schwelle macht es zu", quietGated < quietPlain * 0.05f);
    }

    // --- Noise Gate: die Haltezeit verhindert das Rasseln -----------------------------
    {
        sis::SampleCache cache;

        /* Ein Ton, der zwischen laut und leise springt. Ohne Haltezeit folgt das Gate
           jedem Sprung, mit Haltezeit bleibt es ueber die Luecken hinweg offen. */
        sis::SampleData::Ptr pulsing (new sis::SampleData());
        pulsing->name = "puls.wav";
        pulsing->sourceSampleRate = testSampleRate;
        pulsing->buffer.setSize (1, 48000);

        for (int i = 0; i < 48000; ++i)
        {
            const bool loud = (i / 2400) % 2 == 0;   // 50 ms laut, 50 ms sehr leise
            pulsing->buffer.setSample (0, i, loud ? 0.5f : 0.002f);
        }

        cache.insert (pulsing);

        const auto withHold = [&cache] (float hold)
        {
            auto track = makeTrack ("puls.wav");

            auto gate = sis::Effect::makeNoiseGate();
            gate.parameters[0].value = 0.6f;

            /* Traeger Attack: nur dann kostet jedes Wiederaufgehen etwas, und nur dann
               ist ueberhaupt messbar, was die Haltezeit einspart. */
            gate.parameters[1].value = 0.3f;    // rund 15 ms
            gate.parameters[2].value = hold;
            gate.parameters[3].value = 0.0f;
            gate.parameters[4].value = 1.0f;
            track.effects.push_back (gate);

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 24000, 60, 1.0f);

            return rendered.buffer.getRMSLevel (0, 12000, 12000);
        };

        const float chattering = withHold (0.0f);
        const float held = withHold (0.4f);   // rund 200 ms, laenger als die Luecke

        expect ("Mit Haltezeit bleibt mehr stehen", held > chattering * 1.1f);
    }

    // --- Expander: unter der Schwelle wird der Abstand groesser ------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("a.wav", 48000, 0.1f));    // -20 dB
        cache.insert (makeConstantSample ("b.wav", 48000, 0.05f));   // -26 dB

        const auto throughExpander = [&cache] (const juce::String& clip, bool withEffect)
        {
            auto track = makeTrack (clip);

            if (withEffect)
            {
                auto expander = sis::Effect::makeExpander();
                /* Schwelle bei -18 dB, beide Pegel liegen darunter. Das Verhaeltnis
                   bleibt massvoll: bei 8:1 liefe die Absenkung beider Pegel in die
                   Begrenzung von 40 dB, und ihr Abstand bliebe gerade gleich. */
                expander.parameters[0].value = 0.775f;
                expander.parameters[1].value = 0.5f;    // Verhaeltnis rund 2,75:1
                expander.parameters[2].value = 0.0f;
                expander.parameters[3].value = 0.0f;
                track.effects.push_back (expander);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 24000, 60, 1.0f);
            return rendered.buffer.getRMSLevel (0, 18000, 6000);
        };

        const float plainRatio = throughExpander ("a.wav", false) / throughExpander ("b.wav", false);
        const float expandedRatio = throughExpander ("a.wav", true) / throughExpander ("b.wav", true);

        expectNear ("Ohne Expander betraegt der Abstand 2:1", plainRatio, 2.0, 0.1);
        expect ("Der Expander zieht die beiden weiter auseinander", expandedRatio > plainRatio * 2.0f);
    }

    // --- Limiter: die Decke wird nie ueberschritten -----------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 48000, 220.0));

        const auto peakWithCeiling = [&cache] (bool withEffect, float ceiling, float input)
        {
            auto track = makeTrack ("sine.wav");

            if (withEffect)
            {
                auto limiter = sis::Effect::makeLimiter();
                limiter.parameters[0].value = input;
                limiter.parameters[1].value = ceiling;
                limiter.parameters[2].value = 0.3f;
                track.effects.push_back (limiter);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 24000, 60, 1.0f);
            return rendered.peak (2000, 22000);
        };

        const float plain = peakWithCeiling (false, 0.0f, 0.0f);
        expect ("Ohne Limiter steht der Sinus voll an", plain > 0.9f);

        // mapRange(0.75, -24, 0) ergibt -6 dB - und das sind 0,5012, nicht 0,5
        const float ceiling = juce::Decibels::decibelsToGain (-6.0f);
        const float limited = peakWithCeiling (true, 0.75f, 0.0f);

        expect ("Die Decke wird eingehalten", limited <= ceiling + 1.0e-4f);
        expect ("Aber auch erreicht", limited > ceiling * 0.9f);

        // Auch mit kraeftiger Anhebung davor darf nichts darueber
        const float pushed = peakWithCeiling (true, 0.75f, 1.0f);   // +24 dB hinein
        expect ("Auch voll hineingefahren haelt die Decke", pushed <= ceiling + 1.0e-4f);
    }

    // --- De-Esser: nimmt die Hoehen, laesst den Grundton stehen ------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("zisch.wav", 48000, 9000.0));
        cache.insert (makeSineSample ("grund.wav", 48000, 300.0));

        const auto throughDeEsser = [&cache] (const juce::String& clip, bool withEffect)
        {
            auto track = makeTrack (clip);

            if (withEffect)
            {
                auto deEsser = sis::Effect::makeDeEsser();
                deEsser.parameters[0].value = 0.3f;   // Trennung bei rund 4 kHz
                deEsser.parameters[1].value = 0.2f;   // niedrige Schwelle
                deEsser.parameters[2].value = 1.0f;   // volle Staerke
                deEsser.parameters[3].value = 0.3f;
                track.effects.push_back (deEsser);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 24000, 60, 1.0f);
            return rendered.buffer.getRMSLevel (0, 12000, 12000);
        };

        const float sibilantPlain = throughDeEsser ("zisch.wav", false);
        const float sibilantTreated = throughDeEsser ("zisch.wav", true);
        const float bodyPlain = throughDeEsser ("grund.wav", false);
        const float bodyTreated = throughDeEsser ("grund.wav", true);

        expect ("Der Zischlaut wird deutlich leiser", sibilantTreated < sibilantPlain * 0.4f);
        expect ("Der Grundton bleibt fast unangetastet", bodyTreated > bodyPlain * 0.9f);
    }

    // --- Envelope Filter: folgt dem Anschlag ------------------------------------------
    {
        sis::SampleCache cache;

        /* Derselbe Ton in zwei Lautstaerken. Gemessen wird, wie viel davon durchkommt -
           bezogen auf den Eingangspegel, sonst waere nur die Lautstaerke gemessen. */
        cache.insert (makeSineSample ("laut.wav", 48000, 1500.0));

        sis::SampleData::Ptr quiet (new sis::SampleData());
        quiet->name = "leise.wav";
        quiet->sourceSampleRate = testSampleRate;
        quiet->buffer.setSize (1, 48000);

        for (int i = 0; i < 48000; ++i)
            quiet->buffer.setSample (0, i, 0.06f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                                     * 1500.0 * i / testSampleRate));

        cache.insert (quiet);

        const auto transmission = [&cache] (const juce::String& clip, float inputLevel)
        {
            auto track = makeTrack (clip);

            auto filter = sis::Effect::makeEnvelopeFilter();
            filter.parameters[0].value = 0.0f;    // Grundton ganz unten: leise bleibt zu
            filter.parameters[1].value = 1.0f;    // volle Empfindlichkeit
            filter.parameters[2].value = 0.6f;
            filter.parameters[3].value = 0.0f;    // schnell auf
            filter.parameters[4].value = 0.2f;
            filter.parameters[5].value = 1.0f;    // ganz nass
            track.effects.push_back (filter);

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 48000, 60, 1.0f);

            return rendered.buffer.getRMSLevel (0, 24000, 24000) / inputLevel;
        };

        const float quietShare = transmission ("leise.wav", 0.06f);
        const float loudShare = transmission ("laut.wav", 1.0f);

        expect ("Leise gespielt bleibt das Filter zu", quietShare < 0.4f);
        expect ("Laut gespielt geht es auf", loudShare > quietShare * 2.0f);
    }

    // --- Auto-Wah: laeuft gleichmaessig, unabhaengig vom Anschlag ---------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 48000, 1200.0));

        /* Der Unterschied zum Envelope Filter in einer Messung: bei **gleichbleibendem**
           Pegel muss das Auto-Wah trotzdem schwingen - es folgt der Uhr, nicht dem
           Anschlag. Das Envelope Filter steht bei gleichbleibendem Pegel still. */
        const auto levelSwing = [&cache] (sis::Effect effect, bool withEffect)
        {
            auto track = makeTrack ("sine.wav");

            if (withEffect)
                track.effects.push_back (effect);

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 48000, 60, 1.0f);

            float lowest = 1.0e9f, highest = 0.0f;

            for (int window = 12000; window + 1500 <= 48000; window += 1500)
            {
                const float level = rendered.buffer.getRMSLevel (0, window, 1500);
                lowest = juce::jmin (lowest, level);
                highest = juce::jmax (highest, level);
            }

            return highest - lowest;
        };

        auto wah = sis::Effect::makeAutoWah();
        wah.parameters[0].value = 0.0f;    // von ganz unten
        wah.parameters[1].value = 1.0f;    // bis ganz oben
        wah.parameters[2].value = 0.6f;    // zuegiges Tempo
        wah.parameters[3].value = 0.8f;    // hohe Resonanz
        wah.parameters[4].value = 1.0f;

        auto steady = sis::Effect::makeEnvelopeFilter();
        steady.parameters[5].value = 1.0f;

        const float plain = levelSwing (wah, false);
        const float swept = levelSwing (wah, true);
        const float followed = levelSwing (steady, true);

        expect ("Ein glatter Sinus schwankt nicht", plain < 0.02f);
        expect ("Das Auto-Wah laeuft auch bei gleichem Pegel", swept > 0.05f);
        expect ("Das Envelope Filter steht dabei still", followed < swept * 0.25f);

        // Menge auf null laesst beides in Ruhe
        auto silentWah = wah;
        silentWah.parameters[4].value = 0.0f;
        expect ("Menge auf null laesst den Sinus glatt", levelSwing (silentWah, true) < 0.02f);
    }

    // --- Kanal-Streifen: drei Werkzeuge in einer Karte --------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("tief.wav", 48000, 100.0));
        cache.insert (makeSineSample ("hoch.wav", 48000, 9000.0));
        cache.insert (makeConstantSample ("laut.wav", 48000, 0.8f));
        cache.insert (makeConstantSample ("mittel.wav", 48000, 0.2f));

        /** Streifen mit allen sechs Reglern; -1 bedeutet: gar keinen Streifen. */
        const auto throughStrip = [&cache] (const juce::String& clip, float gate, float low,
                                            float mid, float high, float compression, float output)
        {
            auto track = makeTrack (clip);

            if (gate >= 0.0f)
            {
                auto strip = sis::Effect::makeChannelStrip();
                strip.parameters[0].value = gate;
                strip.parameters[1].value = low;
                strip.parameters[2].value = mid;
                strip.parameters[3].value = high;
                strip.parameters[4].value = compression;
                strip.parameters[5].value = output;
                track.effects.push_back (strip);
            }

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 24000, 60, 1.0f);
            return rendered.buffer.getRMSLevel (0, 12000, 12000);
        };

        // In Grundstellung tut der Streifen nichts - man darf ihn bedenkenlos einhaengen
        const float plain = throughStrip ("tief.wav", -1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f);
        const float neutral = throughStrip ("tief.wav", 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f);
        expectNear ("In Grundstellung bleibt alles, wie es war", neutral, plain, plain * 0.05);

        // Klangregelung
        const float bassBoosted = throughStrip ("tief.wav", 0.0f, 1.0f, 0.5f, 0.5f, 0.0f, 0.5f);
        const float bassCut = throughStrip ("tief.wav", 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.5f);
        expect ("Angehobene Tiefen sind lauter", bassBoosted > plain * 2.0f);
        expect ("Abgesenkte Tiefen sind leiser", bassCut < plain * 0.5f);

        const float plainHigh = throughStrip ("hoch.wav", -1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f);
        const float trebleBoosted = throughStrip ("hoch.wav", 0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f);
        expect ("Angehobene Hoehen sind lauter", trebleBoosted > plainHigh * 2.0f);

        // Ausgang
        const float quieter = throughStrip ("tief.wav", 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f);
        expect ("Der Ausgang senkt um 12 dB", quieter < plain * 0.3f);

        // Kompression: zwei Pegel ruecken zusammen
        const float loudPlain = throughStrip ("laut.wav", -1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f);
        const float midPlain = throughStrip ("mittel.wav", -1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f);
        const float loudPressed = throughStrip ("laut.wav", 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f);
        const float midPressed = throughStrip ("mittel.wav", 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f);

        expectNear ("Unbehandelt betraegt der Abstand 4:1", loudPlain / midPlain, 4.0, 0.2);
        expect ("Der Kompressor rueckt die beiden zusammen",
                loudPressed / midPressed < (loudPlain / midPlain) * 0.6f);
    }

    // --- Kanal-Streifen: das Gate sitzt vor der Klangregelung --------------------------
    {
        sis::SampleCache cache;

        /* Ein tiefer Ton knapp unter der Gate-Schwelle. Saesse die Klangregelung vorn,
           koennte eine Anhebung der Tiefen ihn ueber die Schwelle heben und das Gate
           wieder oeffnen - mit dem Gate zuerst bleibt er weg. Genau diese Reihenfolge
           macht den Streifen brauchbar. */
        sis::SampleData::Ptr faint (new sis::SampleData());
        faint->name = "schwach.wav";
        faint->sourceSampleRate = testSampleRate;
        faint->buffer.setSize (1, 48000);

        for (int i = 0; i < 48000; ++i)
            faint->buffer.setSample (0, i, 0.004f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                                     * 100.0 * i / testSampleRate));

        cache.insert (faint);

        const auto withStrip = [&cache] (float bass)
        {
            auto track = makeTrack ("schwach.wav");

            auto strip = sis::Effect::makeChannelStrip();
            strip.parameters[0].value = 0.5f;    // Schwelle bei rund -42 dB
            strip.parameters[1].value = bass;
            strip.parameters[2].value = 0.5f;
            strip.parameters[3].value = 0.5f;
            strip.parameters[4].value = 0.0f;
            strip.parameters[5].value = 0.5f;
            track.effects.push_back (strip);

            auto engine = makeEngineFor (makeModel ({ track }), cache);
            const auto rendered = render (*engine, 24000, 60, 1.0f);
            return rendered.buffer.getRMSLevel (0, 12000, 12000);
        };

        // Der Ton liegt bei -48 dB, die Schwelle bei -42 dB: das Gate haelt zu
        expect ("Unter der Schwelle bleibt es zu", withStrip (0.5f) < 0.0005f);

        /* Volle Anhebung der Tiefen waeren +12 dB und damit ueber der Schwelle -
           das Gate darf davon nichts merken, weil es vorher gerechnet hat. */
        expect ("Auch angehobene Tiefen reissen es nicht auf", withStrip (1.0f) < 0.0005f);
    }

    // --- Das Menue laesst keinen Effekt liegen ----------------------------------------
    {
        const auto available = sis::Effect::builtIn();

        expect ("Es gibt eingebaute Effekte", available.size() > 20);

        /* Jeder Eintrag braucht eine Familie, sonst landet er im Menue unten im
           Sammelbecken statt in seinem Untermenue. Der Uebersetzer warnt zwar, wenn in
           familyOf eine Art fehlt - aber nicht, wenn sie dort versehentlich bei
           `generic` landet. */
        std::set<juce::String> named;
        int withoutFamily = 0;

        for (const auto& effect : available)
        {
            expect ("Jeder Effekt hat einen Namen", effect.name.isNotEmpty());
            expect ("Und der kommt nur einmal vor: " + effect.name, named.insert (effect.name).second);

            if (! sis::familyOf (effect.type).has_value())
                ++withoutFamily;
        }

        expect ("Jeder Eintrag hat eine Familie", withoutFamily == 0);

        // Die Untermenues zusammen muessen genau die Liste ergeben
        size_t grouped = 0;

        for (const auto family : sis::effectFamilyOrder())
        {
            size_t inFamily = 0;

            for (const auto& effect : available)
                if (sis::familyOf (effect.type) == family)
                    ++inFamily;

            expect ("Keine Familie bleibt leer: " + sis::toDisplayString (family), inFamily > 0);
            expect ("Jede Familie hat eine Ueberschrift", sis::toDisplayString (family).isNotEmpty());
            grouped += inFamily;
        }

        expect ("Die Familien decken die ganze Liste ab", grouped == available.size());

        // Platzhalter und fremde Plugins gehoeren in kein Untermenue
        expect ("Der Platzhalter hat keine Familie",
                ! sis::familyOf (sis::EffectType::generic).has_value());
        expect ("Fremde Plugins auch nicht",
                ! sis::familyOf (sis::EffectType::external).has_value());
    }

    // --- Presets: sichern, laden, loeschen --------------------------------------------
    {
        // Eigene Datei, damit die echten Presets des Anwenders unberuehrt bleiben
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("sis_preset_test.xml");
        file.deleteFile();

        {
            sis::PresetLibrary library (file);

            expect ("Am Anfang gibt es keine Presets",
                    library.getNames (sis::EffectType::overdrive).isEmpty());

            auto overdrive = sis::Effect::makeOverdrive();
            overdrive.parameters[0].value = 0.91f;
            overdrive.parameters[1].value = 0.13f;

            library.save ("Warm", overdrive);

            const auto names = library.getNames (sis::EffectType::overdrive);
            expect ("Das Preset steht in der Liste", names.contains ("Warm"));
            expect ("Und zwar genau einmal", names.size() == 1);

            // Presets gehoeren zu einer Effektart, nicht zu allen
            expect ("Eine andere Effektart sieht es nicht",
                    library.getNames (sis::EffectType::distortion).isEmpty());

            // Auf einen frischen Effekt anwenden
            auto fresh = sis::Effect::makeOverdrive();
            expect ("Frisch steht er auf der Voreinstellung",
                    ! juce::approximatelyEqual (fresh.parameters[0].value, 0.91f));

            expect ("Das Preset laesst sich laden", library.apply ("Warm", fresh));
            expectNear ("Der erste Wert stimmt", fresh.parameters[0].value, 0.91, 1.0e-4);
            expectNear ("Der zweite auch", fresh.parameters[1].value, 0.13, 1.0e-4);

            expect ("Ein unbekannter Name schlaegt fehl", ! library.apply ("Gibt es nicht", fresh));

            // Zurueck auf die Werte, mit denen der Effekt eingefuegt wird
            expect ("Grundstellung gelingt", sis::PresetLibrary::applyFactoryDefaults (fresh));
            expectNear ("Und stellt die Voreinstellung her", fresh.parameters[0].value,
                        sis::Effect::makeOverdrive().parameters[0].value, 1.0e-4);

            /* Zugeordnet wird ueber die Beschriftung, nicht ueber die Nummer: ein Effekt
               mit vertauschten Reglern bekommt trotzdem die richtigen Werte. */
            auto shuffled = sis::Effect::makeOverdrive();
            std::swap (shuffled.parameters[0], shuffled.parameters[1]);
            library.apply ("Warm", shuffled);

            expectNear ("Auch vertauscht landet STAERKE richtig", shuffled.parameters[1].value, 0.91, 1.0e-4);
            expectNear ("Und CHARAKTER auch", shuffled.parameters[0].value, 0.13, 1.0e-4);
        }

        // Eine zweite Sammlung auf derselben Datei muss dasselbe sehen
        {
            sis::PresetLibrary reopened (file);
            expect ("Das Preset ueberlebt den Neustart",
                    reopened.getNames (sis::EffectType::overdrive).contains ("Warm"));

            auto effect = sis::Effect::makeOverdrive();
            reopened.apply ("Warm", effect);
            expectNear ("Mit seinen Werten", effect.parameters[0].value, 0.91, 1.0e-4);

            // Gleicher Name: ueberschreiben statt verdoppeln
            effect.parameters[0].value = 0.42f;
            reopened.save ("Warm", effect);
            expect ("Gleicher Name legt kein zweites an",
                    reopened.getNames (sis::EffectType::overdrive).size() == 1);

            auto check = sis::Effect::makeOverdrive();
            reopened.apply ("Warm", check);
            expectNear ("Sondern ueberschreibt", check.parameters[0].value, 0.42, 1.0e-4);

            reopened.remove (sis::EffectType::overdrive, "Warm");
            expect ("Geloescht ist es weg", reopened.getNames (sis::EffectType::overdrive).isEmpty());
        }

        {
            sis::PresetLibrary afterDelete (file);
            expect ("Und bleibt es auch nach dem Neustart",
                    afterDelete.getNames (sis::EffectType::overdrive).isEmpty());
        }

        file.deleteFile();
    }

    // --- Makros steuern zugewiesene Regler --------------------------------------------
    {
        auto track = makeTrack ("sine.wav");
        track.effects = { sis::Effect::makeOverdrive(), sis::Effect::makeDelay() };

        const auto modelPtr = makeModel ({ track });
        auto& model = *modelPtr;

        sis::MacroTarget target;
        target.zoneId = model.zones.front().id;
        target.trackIndex = 0;
        target.effectIndex = 0;
        target.parameterIndex = 1;   // CHARAKTER des Overdrive

        expect ("Vorher folgt der Regler keinem Makro", model.macroFor (target) < 0);

        model.assignMacro (0, target);
        expect ("Nach dem Zuweisen schon", model.macroFor (target) == 0);

        model.macros[0] = 0.9f;
        model.applyMacro (0);

        expectNear ("Das Makro setzt den Regler",
                    model.zones.front().tracks.front().effects[0].parameters[1].value, 0.9, 1.0e-4);

        // Ein anderer Regler bleibt unberuehrt
        const float untouched = model.zones.front().tracks.front().effects[1].parameters[0].value;
        model.macros[0] = 0.1f;
        model.applyMacro (0);
        expectNear ("Andere Regler bleiben stehen",
                    model.zones.front().tracks.front().effects[1].parameters[0].value, untouched, 1.0e-4);

        /* Ein Regler gehoert zu hoechstens einem Makro: das zweite Zuweisen verschiebt,
           es verdoppelt nicht. */
        model.assignMacro (2, target);
        expect ("Das Zuweisen verschiebt", model.macroFor (target) == 2);
        expect ("Beim alten Makro haengt nichts mehr", model.macroTargets[0].empty());
        expect ("Beim neuen genau eines", model.macroTargets[2].size() == 1);

        // Ueber Speichern und Laden hinweg
        const auto tree = model.toValueTree();

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        sis::InstrumentModel loaded;
        expect ("Das Instrument laedt", loaded.fromValueTree (tree, formats, juce::File()));
        expect ("Die Zuweisung ueberlebt", loaded.macroFor (target) == 2);

        loaded.macros[2] = 0.33f;
        loaded.applyMacro (2);
        expectNear ("Und steuert weiterhin",
                    loaded.zones.front().tracks.front().effects[0].parameters[1].value, 0.33, 1.0e-4);

        // Loesen
        loaded.assignMacro (-1, target);
        expect ("Geloest folgt der Regler nichts mehr", loaded.macroFor (target) < 0);

        const float frozen = loaded.zones.front().tracks.front().effects[0].parameters[1].value;
        loaded.macros[2] = 1.0f;
        loaded.applyMacro (2);
        expectNear ("Und bleibt stehen",
                    loaded.zones.front().tracks.front().effects[0].parameters[1].value, frozen, 1.0e-4);
    }

    // --- Drumset: die Taste bestimmt die Tonhoehe nicht -------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeRampSample ("ramp.wav", 48000));

        auto drumsPtr = makeModel ({ makeTrack ("ramp.wav") });
        drumsPtr->kind = sis::InstrumentKind::drumKit;

        auto atRoot = makeEngine (*drumsPtr, cache);
        const auto root = render (*atRoot, 2048, 60, 1.0f);

        auto anOctaveUp = makeEngine (*drumsPtr, cache);
        const auto octave = render (*anOctaveUp, 2048, 72, 1.0f);

        expectNear ("Beim Drumset spielt eine Oktave hoeher genauso schnell",
                    (double) octave.at (1000) / juce::jmax (1.0e-6, (double) root.at (1000)), 1.0, 0.02);

        auto twoOctavesDown = makeEngine (*drumsPtr, cache);
        const auto low = render (*twoOctavesDown, 2048, 36, 1.0f);

        expectNear ("Auch zwei Oktaven tiefer nicht",
                    (double) low.at (1000) / juce::jmax (1.0e-6, (double) root.at (1000)), 1.0, 0.02);

        /* Die Spur-Tonhoehe muss trotzdem wirken - eine tiefer gestimmte Snare will man
           auch im Drumset. Sonst waere der Regler dort stillschweigend tot. */
        auto tuned = makeTrack ("ramp.wav");
        tuned.pitch = 12;
        auto tunedModel = makeModel ({ tuned });
        tunedModel->kind = sis::InstrumentKind::drumKit;

        auto tunedEngine = makeEngineFor (std::move (tunedModel), cache);
        const auto shifted = render (*tunedEngine, 2048, 60, 1.0f);

        expectNear ("Die Spur-Tonhoehe wirkt im Drumset weiter",
                    (double) shifted.at (1000) / juce::jmax (1.0e-6, (double) root.at (1000)), 2.0, 0.05);

        // Und zum Vergleich: als Tonhoehen-Instrument verstimmt dieselbe Zone sehr wohl
        auto melodicPtr = makeModel ({ makeTrack ("ramp.wav") });
        auto melodicRoot = makeEngine (*melodicPtr, cache);
        const auto mRoot = render (*melodicRoot, 2048, 60, 1.0f);
        auto melodicUp = makeEngine (*melodicPtr, cache);
        const auto mOctave = render (*melodicUp, 2048, 72, 1.0f);

        expectNear ("Als Tonhoehen-Instrument dagegen doppelt so schnell",
                    (double) mOctave.at (1000) / juce::jmax (1.0e-6, (double) mRoot.at (1000)), 2.0, 0.05);
    }

    // --- Kit-Teile: anlegen, wiederfinden, speichern -----------------------------------
    {
        sis::InstrumentModel model;
        model.zones.clear();
        model.kind = sis::InstrumentKind::drumKit;

        const auto& parts = sis::drumParts();
        expect ("Das Kit hat Teile", ! parts.empty());

        const auto* snare = sis::findDrumPart ("snare");
        expect ("Die Snare ist dabei", snare != nullptr);
        expect ("Sie sitzt auf der General-MIDI-Note", snare->note == 38);
        expect ("Ein erfundenes Teil gibt es nicht", sis::findDrumPart ("kazoo") == nullptr);

        auto& zone = model.addDrumZone (*snare);
        expect ("Das Teil sitzt auf genau einer Taste", zone.lowNote == 38 && zone.highNote == 38);
        expect ("Und diese Taste ist sein Grundton", zone.rootNote == 38);
        expect ("Es ist gewaehlt", model.getSelectedZone() != nullptr
                                       && model.getSelectedZone()->drumPart == "snare");

        /* Zweimal anlegen darf keine zweite Snare geben - sonst haengen zwei Zonen auf
           derselben Taste und man hoert beide. */
        model.addDrumZone (*snare);
        expect ("Zweimal anlegen legt nicht zweimal an", model.zones.size() == 1);

        model.addDrumZone (*sis::findDrumPart ("kick"));
        expect ("Ein anderes Teil schon", model.zones.size() == 2);
        expect ("Und ist wiederzufinden", model.findZoneForDrumPart ("kick") != nullptr);
        expect ("Ein leeres Teil nicht", model.findZoneForDrumPart ("ride") == nullptr);

        // Ueber Speichern und Laden hinweg
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        sis::InstrumentModel loaded;
        expect ("Das Kit laedt", loaded.fromValueTree (model.toValueTree(), formats, juce::File()));
        expect ("Die Art ueberlebt", loaded.kind == sis::InstrumentKind::drumKit);
        expect ("Die Zuordnung zum Kit-Teil auch", loaded.findZoneForDrumPart ("snare") != nullptr);

        // Eine alte Projektdatei ohne die Angabe bleibt ein Tonhoehen-Instrument
        auto tree = model.toValueTree();
        tree.removeProperty ("kind", nullptr);

        sis::InstrumentModel old;
        expect ("Eine Datei ohne Angabe laedt", old.fromValueTree (tree, formats, juce::File()));
        expect ("Und ist ein Tonhoehen-Instrument", old.kind == sis::InstrumentKind::melodic);
    }

    // --- Jeder interne Effekt taugt auch als eigenstaendiges Plugin -------------------
    {
        /* Die Effekt-Plugins bauen ihren Plan mit `buildSingleEffect` und rechnen mit
           `applyEffectChain`. Vergisst jemand beim Hinzufuegen einer Effektart einen der
           beiden Faelle, bleibt das Plugin **still** - und das faellt sonst erst in einer
           fremden DAW auf. Deshalb hier ueber alle Arten auf einmal. */
        sis::BusProcessors processors;
        processors.prepare (testSampleRate, 512);

        int checked = 0;

        for (const auto& prototype : sis::Effect::builtIn())
        {
            const auto name = prototype.name;

            // Eindeutig: das Plugin sucht sich seinen Prototyp ueber die Art heraus
            int sameType = 0;
            for (const auto& other : sis::Effect::builtIn())
                if (other.type == prototype.type)
                    ++sameType;

            expect ((name + ": genau ein Prototyp"), sameType == 1);

            expect (name + ": hat Regler", ! prototype.parameters.empty());

            /* Nicht die Grundstellung pruefen: Equalizer, Kanalfilter und Transienten sind
               dort **mit Absicht** neutral, und ein flacher EQ ist zu Recht ein Durchgang.
               Gefaehrlich waere ein Effekt, der bei *keiner* Einstellung etwas tut - das
               heisst, jemand hat beim Hinzufuegen der Art einen Fall vergessen, und das
               zugehoerige Plugin bliebe in jeder DAW still. */
            bool everActive = false;

            for (const float setting : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                auto turned = prototype;

                for (auto& parameter : turned.parameters)
                    parameter.value = setting;

                const auto plan = sis::buildSingleEffect (turned, testSampleRate);

                if (plan.numSteps == 1 && plan.steps[0] == turned.type)
                    everActive = true;

                expect (name + ": ergibt hoechstens einen Schritt", plan.numSteps <= 1);
            }

            expect (name + ": wird irgendwann wirksam", everActive);

            const auto plan = sis::buildSingleEffect (prototype, testSampleRate);

            /* Und die Kette laeuft damit wirklich durch. Geprueft wird nicht, *dass* sich
               etwas aendert - manche Effekte sind in Grundstellung absichtlich neutral -,
               sondern dass nichts Unerhoertes herauskommt. */
            juce::AudioBuffer<float> buffer (2, 512);

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 512; ++i)
                    buffer.setSample (channel, i, 0.3f * std::sin (juce::MathConstants<float>::twoPi
                                                                      * 440.0f * (float) i / (float) testSampleRate));

            processors.reset();

            float* channels[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
            sis::applyEffectChain (plan, processors, channels, 512);

            bool sane = true;
            for (int channel = 0; channel < 2 && sane; ++channel)
                for (int i = 0; i < 512; ++i)
                {
                    const float value = buffer.getSample (channel, i);

                    if (! std::isfinite (value) || std::abs (value) > 8.0f)
                    {
                        sane = false;
                        break;
                    }
                }

            expect ((name + ": liefert brauchbare Werte"), sane);
            ++checked;
        }

        expect ("Alle Effektarten geprueft", checked == (int) sis::Effect::builtIn().size());
    }

    // --- Schleifen: Naht mit und ohne Ueberblendung, Loop-Beginn, einmalige Fades --------
    {
        /* Eine Rampe 0 ... 1 springt an der Naht einer Vorwaerts-Schleife von 1 auf 0 zurueck:
           der hoerbare Knacks. Mit Ueberblendung darf von Sample zu Sample kein grosser
           Sprung mehr bleiben. */
        constexpr int length = 4800;
        const auto largestStep = [] (const Rendered& r, int from, int to)
        {
            float largest = 0.0f;
            for (int i = from + 1; i < to; ++i)
                largest = juce::jmax (largest, std::abs (r.at (i) - r.at (i - 1)));
            return largest;
        };

        sis::SampleCache cache;
        cache.insert (makeRampSample ("ramp.wav", length));

        auto hardTrack = makeTrack ("ramp.wav");
        hardTrack.loop = sis::LoopMode::sustainLoop;
        auto hard = makeModel ({ hardTrack });
        auto hardEngine = makeEngine (*hard, cache);
        const auto hardOut = render (*hardEngine, 4 * length, 60, 1.0f);
        expect ("Ohne Ueberblendung springt die Schleife an der Naht", largestStep (hardOut, 0, 4 * length) > 0.9f);

        auto softTrack = hardTrack;
        softTrack.loopCrossfade = 0.3;
        auto soft = makeModel ({ softTrack });
        auto softEngine = makeEngine (*soft, cache);
        const auto softOut = render (*softEngine, 4 * length, 60, 1.0f);
        expect ("Mit Ueberblendung ist die Naht glatt", largestStep (softOut, 0, 4 * length) < 0.01f);
        expect ("Und die Schleife klingt weiter", softOut.peak (3 * length, length) > 0.3f);

        // Vor/Rueckwaerts mit Ueberblendung: ebenfalls ohne Spruenge und ohne Aussetzer
        auto pingTrack = hardTrack;
        pingTrack.loop = sis::LoopMode::pingPong;
        pingTrack.loopCrossfade = 0.2;
        auto ping = makeModel ({ pingTrack });
        auto pingEngine = makeEngine (*ping, cache);
        const auto pingOut = render (*pingEngine, 5 * length, 60, 1.0f);
        expect ("Vor/Rueckwaerts mit Ueberblendung bleibt glatt", largestStep (pingOut, 0, 5 * length) < 0.01f);
        expect ("Vor/Rueckwaerts klingt weiter", pingOut.peak (4 * length, length) > 0.3f);

        // Loop-Beginn in der Mitte: nach dem ersten Durchgang kommt der Anfang nie wieder
        auto startTrack = hardTrack;
        startTrack.loopStart = 0.5;
        auto started = makeModel ({ startTrack });
        auto startEngine = makeEngine (*started, cache);
        const auto startOut = render (*startEngine, 4 * length, 60, 1.0f);

        float lowest = 1.0f;
        for (int i = length + 10; i < 4 * length; ++i)
            lowest = juce::jmin (lowest, startOut.at (i));
        expect ("Der Anschlag vor dem Loop-Beginn klingt nur einmal", lowest > 0.49f);
        expect ("Beim ersten Durchgang klingt er aber", startOut.at (100) < 0.1f);

        // Fades einer Schleife: einmal ein, nie aus - sonst pumpt jede Runde
        sis::SampleCache constantCache;
        constantCache.insert (makeConstantSample ("dc.wav", length));
        auto fadedTrack = makeTrack ("dc.wav");
        fadedTrack.loop = sis::LoopMode::pingPong;
        fadedTrack.clips[0].fadeIn = 0.1;
        fadedTrack.clips[0].fadeOut = 0.1;
        auto faded = makeModel ({ fadedTrack });
        auto fadedEngine = makeEngine (*faded, constantCache);
        const auto fadedOut = render (*fadedEngine, 4 * length, 60, 1.0f);

        float dip = 1.0f;
        for (int i = length / 2; i < 4 * length; ++i)
            dip = juce::jmin (dip, fadedOut.at (i));
        expect ("Eine Schleife blendet nicht in jeder Runde aus und ein", dip > 0.99f);
        expect ("Das Einblenden am Anfang bleibt", fadedOut.at (10) < 0.05f);

        // Speichern und Laden
        juce::AudioFormatManager formats;
        sis::InstrumentModel loaded;
        loaded.fromValueTree (soft->toValueTree(), formats);
        expectNear ("Die Ueberblendung ueberlebt das Speichern",
                    loaded.zones.front().tracks.front().loopCrossfade, 0.3, 1.0e-9);
    }

    // --- Wiedergabe ab dem Locator ----------------------------------------------------
    {
        constexpr int length = 48000;
        sis::SampleCache cache;
        cache.insert (makeRampSample ("ramp.wav", length));

        auto track = makeTrack ("ramp.wav");
        track.loop = sis::LoopMode::oneShot;
        auto modelPtr = makeModel ({ track });
        auto engine = makeEngine (*modelPtr, cache);

        engine->setStartOffset (60, 0.5);   // eine halbe Sekunde = 24000 Samples hinein
        const auto fromLocator = render (*engine, 256, 60, 1.0f);
        // Ab Sample 200, damit die kurze Anstiegszeit der Hüllkurve nicht mitmisst
        expectNear ("Der Anschlag beginnt am Locator", fromLocator.at (200), 24200.0 / (length - 1), 0.01);

        auto fresh = makeEngine (*modelPtr, cache);
        const auto normal = render (*fresh, 256, 60, 1.0f);
        expect ("Ohne Locator beginnt er vorn", normal.at (0) < 0.001f);

        // Eine Spur mit Versatz hinter dem Locator wartet entsprechend kuerzer
        auto late = track;
        late.clips[0].offset = 1.0 / sis::InstrumentModel::timelineSeconds;   // 1 s
        auto lateModel = makeModel ({ late });
        auto lateEngine = makeEngine (*lateModel, cache);
        lateEngine->setStartOffset (60, 0.75);
        const auto lateOut = render (*lateEngine, 24000, 60, 1.0f);
        expect ("Vor dem Versatz ist es still", lateOut.peak (0, 11900) < 0.001f);
        expect ("Nach 0,25 s setzt die Spur ein", lateOut.peak (12100, 2000) > 0.0f);
    }

    // --- Kit: Teile aufbauen und abbauen ------------------------------------------------
    {
        sis::InstrumentModel model;
        model.clear();
        model.kind = sis::InstrumentKind::drumKit;

        expect ("Ein neues Kit hat die Grundausstattung", model.hasKitPiece ("kick") && model.hasKitPiece ("hihat")
                                                            && model.hasKitPiece ("crash") && model.hasKitPiece ("ride"));
        expect ("Drei Toms zu Beginn", model.numKitToms() == 3);
        expect ("Ein China noch nicht", ! model.hasKitPiece ("china"));

        for (const auto& piece : sis::drumPieces())
            for (const auto* part : piece.articulations)
                expect (juce::String ("Jede Spielweise gibt es: ") + part, sis::findDrumPart (part) != nullptr);

        std::set<int> notes;
        for (const auto& part : sis::drumParts())
            expect (juce::String ("Jede Note nur einmal: ") + part.id, notes.insert (part.note).second);

        expect ("Rim-Click gehoert zur Snare", sis::findPieceForPart ("sidestick") == sis::findDrumPiece ("snare"));
        expect ("Hoechstens fuenf Toms im Angebot",
                std::count_if (sis::drumPieces().begin(), sis::drumPieces().end(),
                               [] (const sis::DrumPiece& p) { return sis::isTom (p.kind); }) == sis::maxDrumToms);

        model.addKitPiece (*sis::findDrumPiece ("crash2"));
        expect ("Ein zweites Crash kommt dazu", model.hasKitPiece ("crash2"));

        // Klaenge der HiHat bekommen Zonen, dann kommt die HiHat ganz weg
        model.addDrumZone (*sis::findDrumPart ("hihatOpen"));
        model.addDrumZone (*sis::findDrumPart ("hihatPedal"));
        model.addDrumZone (*sis::findDrumPart ("kick"));
        model.macroTargets[0].push_back ({ model.findZoneForDrumPart ("hihatOpen")->id, 0, 0, 0 });
        expect ("Drei Zonen", model.zones.size() == 3);

        model.removeKitPiece (*sis::findDrumPiece ("hihat"));
        expect ("Die HiHat ist weg", ! model.hasKitPiece ("hihat"));
        expect ("Mit allen ihren Zonen", model.zones.size() == 1 && model.zones.front().drumPart == "kick");
        expect ("Und ihrer Makro-Zuweisung", model.macroTargets[0].empty());

        // Speichern und Laden, und eine alte Datei mit Clap, aber ohne Liste
        juce::AudioFormatManager formats;
        sis::InstrumentModel loaded;
        loaded.fromValueTree (model.toValueTree(), formats);
        expect ("Das Crash 2 ueberlebt das Speichern", loaded.hasKitPiece ("crash2"));
        expect ("Die entfernte HiHat bleibt entfernt", ! loaded.hasKitPiece ("hihat"));

        model.addDrumZone (*sis::findDrumPart ("clap"));
        auto tree = model.toValueTree();
        tree.removeProperty ("kitPieces", nullptr);
        sis::InstrumentModel old;
        old.fromValueTree (tree, formats);
        expect ("Eine alte Datei bekommt das Standard-Kit", old.hasKitPiece ("snare") && old.hasKitPiece ("hihat"));
        expect ("Und behaelt ihre Clap", old.hasKitPiece ("clap"));
    }

    // --- Spuren: loeschen, fuellen, neue Zone um eine Taste -------------------------------
    {
        sis::InstrumentModel model;
        model.clear();
        const auto zoneId = model.addZone().id;
        auto* zone = model.findZone (zoneId);

        for (int i = 0; i < 3; ++i)
        {
            sis::Track track;
            track.name = "T" + juce::String (i);
            sis::Clip piece;
            piece.sample = "s" + juce::String (i) + ".wav";
            track.clips.push_back (piece);
            zone->tracks.push_back (track);
        }

        model.macroTargets[1].push_back ({ zoneId, 1, 0, 0 });
        model.macroTargets[1].push_back ({ zoneId, 2, 0, 0 });

        model.removeTrack (*zone, 1);
        expect ("Die Spur ist weg", zone->tracks.size() == 2 && zone->tracks[1].name == "T2");
        expect ("Ihre Makro-Zuweisung auch, die dahinter rueckt nach",
                model.macroTargets[1].size() == 1 && model.macroTargets[1].front().trackIndex == 1);

        // Eine leere Spur wird gefuellt statt einer weiteren
        zone->tracks[0].clips.clear();
        sis::SampleFile sample;
        sample.name = "neu.wav";
        sample.lengthSeconds = 2.0;
        const int filled = model.addSampleTrack (*zone, sample);
        expect ("Die leere Spur bekommt das Sample",
                filled == 0 && zone->tracks.size() == 2 && zone->tracks[0].clips.size() == 1
                    && zone->tracks[0].clips[0].sample == "neu.wav");
        expectNear ("Mit der Laenge des Samples", zone->tracks[0].clips[0].natural,
                    2.0 / sis::InstrumentModel::timelineSeconds, 1.0e-9);

        model.addSampleTrack (*zone, sample);
        expect ("Ohne leere Spur kommt eine dazu", zone->tracks.size() == 3);

        expect ("Eine neue Spur hat keinen Clip", ! sis::Track().hasClips());
        sis::Clip legacy;
        legacy.sample = "leer";
        expect ("Der alte Platzhalter ist kein Sample", ! legacy.hasSample());

        // Neue Zone neben der vorhandenen (C4-B4): sie waechst nur in den freien Platz
        const auto& fresh = model.addZoneAt (58, 100);
        expect ("Die neue Zone liegt auf der Taste", fresh.containsNote (58) && fresh.rootNote == 58);
        expect ("Und ragt nicht in die vorhandene", fresh.highNote < 60);
        expect ("Hoechstens eine Oktave breit", fresh.highNote - fresh.lowNote + 1 <= 12);

        const auto& layered = model.addZoneAt (64, 100);
        expect ("Ueber einer belegten Taste bleibt nur eine Velocity-Stufe frei",
                layered.lowVelocity == 100 && layered.highVelocity == 100);
    }

    // --- Mehrere Clips auf einer Spur: der obere klingt, im Crossfade beide ------------
    {
        constexpr double axis = sis::InstrumentModel::timelineSeconds;
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("low.wav", 48000, 0.25f));    // 1 s bei 0,25
        cache.insert (makeConstantSample ("high.wav", 48000, 1.0f));    // 1 s bei 1,0

        const auto clipOf = [] (const juce::String& name, double startSeconds)
        {
            sis::Clip c;
            c.sample = name;
            c.natural = 1.0 / sis::InstrumentModel::timelineSeconds;
            c.offset = startSeconds / sis::InstrumentModel::timelineSeconds;
            return c;
        };

        sis::Track track = makeTrack ("low.wav");
        track.loop = sis::LoopMode::oneShot;
        track.clips = { clipOf ("low.wav", 0.0), clipOf ("high.wav", 0.5) };   // high liegt oben ab 0,5 s

        auto stacked = makeModel ({ track });
        auto stackedEngine = makeEngine (*stacked, cache);
        const auto out = render (*stackedEngine, 96000, 60, 1.0f);

        expectNear ("Vor dem oberen Clip klingt der untere", out.at (12000), 0.25, 0.01);
        expectNear ("In der Überschneidung nur der obere", out.at (36000), 1.0, 0.01);
        expectNear ("Danach der obere allein", out.at (60000), 1.0, 0.01);
        expect ("Hinter dem oberen ist Ruhe", out.peak (74000, 20000) < 0.001f);

        // Umgekehrt gestapelt: der untere (high) ist in der Überschneidung stumm
        auto swapped = track;
        std::swap (swapped.clips[0], swapped.clips[1]);
        auto swappedModel = makeModel ({ swapped });
        auto swappedEngine = makeEngine (*swappedModel, cache);
        const auto swappedOut = render (*swappedEngine, 96000, 60, 1.0f);
        expectNear ("Liegt low oben, klingt in der Überschneidung low", swappedOut.at (36000), 0.25, 0.01);
        expectNear ("Danach wieder high", swappedOut.at (60000), 1.0, 0.01);

        // Crossfade über die Überschneidung 0,5 … 1,0 s: beide klingen, linear gemischt
        auto crossed = track;
        auto first = crossed.clips[0];
        auto second = crossed.clips[1];
        sis::geometry::ClipState a { first.offset, first.natural, first.stretch, first.trimStart, first.trimEnd, first.fadeIn, first.fadeOut };
        sis::geometry::ClipState b { second.offset, second.natural, second.stretch, second.trimStart, second.trimEnd, second.fadeIn, second.fadeOut };
        expect ("Crossfade laesst sich legen", sis::geometry::crossfade (a, b));
        crossed.clips[0].fadeOut = a.fadeOut;
        crossed.clips[1].fadeIn = b.fadeIn;

        auto crossedModel = makeModel ({ crossed });
        auto crossedEngine = makeEngine (*crossedModel, cache);
        const auto crossedOut = render (*crossedEngine, 96000, 60, 1.0f);
        expectNear ("Mitte des Crossfades: halb low, halb high", crossedOut.at (36000), 0.5 * 0.25 + 0.5 * 1.0, 0.02);
        expectNear ("Anfang: noch ganz low", crossedOut.at (24100), 0.25, 0.02);
        expectNear ("Ende: ganz high", crossedOut.at (47900), 1.0, 0.02);

        // Speichern und Laden: zwei Clips bleiben zwei Clips, in derselben Stapelfolge
        juce::AudioFormatManager formats;
        sis::InstrumentModel loaded;
        loaded.fromValueTree (stacked->toValueTree(), formats);
        const auto& loadedClips = loaded.zones.front().tracks.front().clips;
        expect ("Zwei Clips nach dem Laden", loadedClips.size() == 2);
        expect ("In derselben Reihenfolge", loadedClips.size() == 2 && loadedClips[1].sample == "high.wav");
        expectNear ("Mit ihrem Versatz", loadedClips.size() == 2 ? loadedClips[1].offset * axis : 0.0, 0.5, 1.0e-9);

        // Eine Datei aus 1.11: ein Clip, der in der Spur selbst steht
        juce::ValueTree legacyTree ("Instrument");
        juce::ValueTree legacyZone ("Zone");
        legacyZone.setProperty ("id", "alt", nullptr);
        juce::ValueTree legacyTrack ("Track");
        legacyTrack.setProperty ("name", "Alt", nullptr);
        legacyTrack.setProperty ("clip", "low.wav", nullptr);
        legacyTrack.setProperty ("offset", 0.25, nullptr);
        legacyTrack.setProperty ("trimEnd", 0.5, nullptr);
        legacyZone.appendChild (legacyTrack, nullptr);
        juce::ValueTree emptyTrack ("Track");
        emptyTrack.setProperty ("clip", "leer", nullptr);
        legacyZone.appendChild (emptyTrack, nullptr);
        legacyTree.appendChild (legacyZone, nullptr);

        sis::InstrumentModel legacy;
        expect ("Die alte Datei laedt", legacy.fromValueTree (legacyTree, formats));
        const auto& legacyTracks = legacy.zones.front().tracks;
        expect ("Ihr Clip wird ein Clip der Spur", legacyTracks.size() == 2 && legacyTracks[0].clips.size() == 1
                                                     && legacyTracks[0].clips[0].sample == "low.wav"
                                                     && legacyTracks[0].clips[0].offset == 0.25
                                                     && legacyTracks[0].clips[0].trimEnd == 0.5);
        expect ("Der alte Platzhalter „leer“ wird keiner", legacyTracks.size() == 2 && legacyTracks[1].clips.empty());
    }

    // --- Schleife bis zum nächsten Clip, Loop-Überblendung im zugeschnittenen Teil ------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("dc.wav", 4800, 0.5f));
        cache.insert (makeRampSample ("ramp.wav", 4800));

        sis::Clip looped;
        looped.sample = "dc.wav";
        sis::Clip next;
        next.sample = "dc.wav";
        next.offset = 1.0 / sis::InstrumentModel::timelineSeconds;   // bei 1 s
        next.trimEnd = 0.1;                                            // 480 Samples lang

        auto track = makeTrack ("dc.wav");
        track.loop = sis::LoopMode::sustainLoop;
        track.clips = { looped, next };

        auto model = makeModel ({ track });
        auto engine = makeEngine (*model, cache);
        const auto out = render (*engine, 72000, 60, 1.0f);
        expectNear ("Die Schleife klingt bis zum nächsten Clip", out.at (40000), 0.5, 0.01);
        expectNear ("Dort klingt nur noch der nächste (nicht beide)", out.at (48200), 0.5, 0.01);

        /* Zugeschnitten auf 25 … 75 % des Samples: Schleife und Überblendung lesen nur dort,
           nie aus dem weggeschnittenen Teil. Auf der Rampe heißt das: kein Wert unter 0,25
           oder über 0,75. */
        sis::Clip trimmed;
        trimmed.sample = "ramp.wav";
        trimmed.trimStart = 0.25;
        trimmed.trimEnd = 0.75;

        auto rampTrack = makeTrack ("ramp.wav");
        rampTrack.loop = sis::LoopMode::sustainLoop;
        rampTrack.loopCrossfade = 0.5;
        rampTrack.clips = { trimmed };

        auto rampModel = makeModel ({ rampTrack });
        auto rampEngine = makeEngine (*rampModel, cache);
        const auto rampOut = render (*rampEngine, 20000, 60, 1.0f);

        float lowest = 1.0f, highest = 0.0f;
        for (int i = 200; i < 20000; ++i)
        {
            lowest = juce::jmin (lowest, rampOut.at (i));
            highest = juce::jmax (highest, rampOut.at (i));
        }

        /* Die Überblendung mit gleicher Leistung darf in der Mitte etwas unter den kleineren
           der beiden Werte sinken, aber nie an den weggeschnittenen Anfang heranreichen. */
        expect ("Die Schleife liest nichts nach dem Zuschnitt", highest < 0.76f);
        expect ("Und nichts vor dem Zuschnitt", lowest > 0.2f);
    }

    // --- Transponieren mit gehaltenem Tempo -------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeSineSample ("sine.wav", 24000, 500.0));   // 0,5 s, 500 Hz

        auto classic = makeTrack ("sine.wav");
        classic.loop = sis::LoopMode::oneShot;

        auto held = classic;
        held.keepTempo = true;

        auto classicModel = makeModel ({ classic });
        auto heldModel = makeModel ({ held });

        auto classicRoot = makeEngine (*classicModel, cache);
        const auto rootOut = render (*classicRoot, 36000, 60, 1.0f);
        auto classicUp = makeEngine (*classicModel, cache);
        const auto classicOctave = render (*classicUp, 36000, 72, 1.0f);
        auto heldUp = makeEngine (*heldModel, cache);
        const auto heldOctave = render (*heldUp, 36000, 72, 1.0f);

        const int rootEnd = rootOut.lastSoundingSample();
        const int classicEnd = classicOctave.lastSoundingSample();
        const int heldEnd = heldOctave.lastSoundingSample();

        // Klassisch: eine Oktave höher ist halb so lang
        expect ("Klassisch halbiert die Oktave die Dauer", std::abs (classicEnd - rootEnd / 2) < 600);

        // Mit gehaltenem Tempo: gleich lang wie am Grundton ...
        expect ("Mit gehaltenem Tempo bleibt die Dauer", std::abs (heldEnd - rootEnd) < 2400);

        // ... und trotzdem eine Oktave höher
        const int window = 9600;
        const double rootCrossings = rootOut.zeroCrossings (4800, window);
        const double heldCrossings = heldOctave.zeroCrossings (4800, window);
        expectNear ("Mit gehaltenem Tempo trotzdem eine Oktave höher", heldCrossings / rootCrossings, 2.0, 0.1);

        // Am Grundton ändert der Schalter nichts: dort wird weiter direkt gelesen
        auto heldRoot = makeEngine (*heldModel, cache);
        const auto heldRootOut = render (*heldRoot, 4800, 60, 1.0f);
        float largest = 0.0f;
        for (int i = 0; i < 4800; ++i)
            largest = juce::jmax (largest, std::abs (heldRootOut.at (i) - rootOut.at (i)));
        expect ("Am Grundton klingt es mit und ohne Schalter gleich", largest < 1.0e-5f);

        // Speichern und Laden
        juce::AudioFormatManager formats;
        sis::InstrumentModel loaded;
        loaded.fromValueTree (heldModel->toValueTree(), formats);
        expect ("Der Schalter übersteht das Speichern", loaded.zones.front().tracks.front().keepTempo);
    }

    // --- Klassisch transponiert läuft die ganze Anordnung schneller ----------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("low.wav", 48000, 0.25f));
        cache.insert (makeConstantSample ("high.wav", 48000, 1.0f));

        const auto clipOf = [] (const juce::String& name, double startSeconds)
        {
            sis::Clip c;
            c.sample = name;
            c.natural = 1.0 / sis::InstrumentModel::timelineSeconds;
            c.offset = startSeconds / sis::InstrumentModel::timelineSeconds;
            return c;
        };

        auto track = makeTrack ("low.wav");
        track.loop = sis::LoopMode::oneShot;
        track.clips = { clipOf ("low.wav", 0.0), clipOf ("high.wav", 0.5) };

        // Eine Oktave höher: alles doppelt so schnell – high übernimmt schon bei 0,25 s
        auto model = makeModel ({ track });
        auto engine = makeEngine (*model, cache);
        const auto octave = render (*engine, 48000, 72, 1.0f);
        expectNear ("Vor der Übernahme klingt low", octave.at (9000), 0.25, 0.01);
        expectNear ("Ab 0,25 s klingt high (die Übernahme ist mitgewandert)", octave.at (15000), 1.0, 0.01);
        expect ("Nach 0,75 s ist Ruhe (auch high läuft doppelt so schnell)", octave.peak (37000, 10000) < 0.001f);

        // Mit gehaltenem Tempo bleibt die Anordnung in echten Sekunden
        auto held = track;
        held.keepTempo = true;
        auto heldModel = makeModel ({ held });
        auto heldEngine = makeEngine (*heldModel, cache);
        const auto heldOut = render (*heldEngine, 48000, 72, 1.0f);
        expectNear ("Mit gehaltenem Tempo klingt bei 0,3 s noch low", heldOut.at (14400), 0.25, 0.02);
        expectNear ("Und ab 0,5 s high", heldOut.at (30000), 1.0, 0.05);
    }

    // --- Zwischenablage und Verschieben über Spuren (Modell) -----------------------------
    {
        sis::Zone zone;
        zone.tracks.resize (2);

        const auto clipAt = [] (const juce::String& name, double offset)
        {
            sis::Clip c;
            c.sample = name;
            c.offset = offset;
            c.natural = 0.1;
            return c;
        };

        zone.tracks[0].clips = { clipAt ("a.wav", 0.30), clipAt ("b.wav", 0.10) };
        zone.tracks[1].clips = { clipAt ("c.wav", 0.05) };

        const std::set<juce::uint32> all { zone.tracks[0].clips[0].uid, zone.tracks[0].clips[1].uid,
                                           zone.tracks[1].clips[0].uid };
        const auto copied = zone.copyClips (all);

        expect ("Drei Clips kopiert", copied.size() == 3);

        // Bezug: der früheste Clip der obersten Spur (b bei 0,10)
        for (const auto& entry : copied)
        {
            if (entry.clip.sample == "b.wav")
                expect ("Der Bezugsclip hat Zeit 0 und Spur 0", entry.clip.offset == 0.0 && entry.trackOffset == 0);
            if (entry.clip.sample == "a.wav")
                expectNear ("a bleibt 0,2 hinter dem Bezug", entry.clip.offset, 0.20, 1.0e-12);
            if (entry.clip.sample == "c.wav")
                expect ("c liegt eine Spur darunter und vor dem Bezug",
                        entry.trackOffset == 1 && std::abs (entry.clip.offset + 0.05) < 1.0e-12);
        }

        // Auf der zweiten Spur bei 0,5 einfügen: die dritte Spur entsteht
        int newTracks = 0;
        const auto pasted = zone.pasteClips (copied, 1, 0.5, &newTracks);
        expect ("Drei Clips eingefügt", pasted.size() == 3);
        expect ("Eine neue Spur entstanden", newTracks == 1 && zone.tracks.size() == 3);
        expect ("c landet in der neuen Spur", zone.tracks[2].clips.size() == 1 && zone.tracks[2].clips[0].sample == "c.wav");
        const auto* pastedB = [&zone]() -> const sis::Clip*
        {
            for (const auto& c : zone.tracks[1].clips)
                if (c.sample == "b.wav")
                    return &c;
            return nullptr;
        }();
        expect ("b liegt auf der Zielspur", pastedB != nullptr);
        expectNear ("b landet am Einfügepunkt", pastedB != nullptr ? pastedB->offset : -1.0, 0.5, 1.0e-12);
        expectNear ("c im selben Abstand", zone.tracks[2].clips[0].offset, 0.45, 1.0e-12);
        expect ("Neue Kennungen", std::none_of (pasted.begin(), pasted.end(),
                                                [&all] (juce::uint32 id) { return all.count (id) > 0; }));

        // Ganz vorn eingefügt rückt alles so weit, dass c bei 0 beginnt
        sis::Zone early;
        early.tracks.resize (1);
        early.pasteClips (copied, 0, 0.0);
        double earliest = 1.0;
        for (const auto& t : early.tracks)
            for (const auto& c : t.clips)
                earliest = std::min (earliest, c.offset);
        expectNear ("Nichts landet vor 0", earliest, 0.0, 1.0e-12);

        // Verschieben: a und c eine Spur tiefer, 0,1 später
        sis::Zone moving;
        moving.tracks.resize (2);
        moving.tracks[0].clips = { clipAt ("a.wav", 0.30) };
        moving.tracks[1].clips = { clipAt ("c.wav", 0.05) };
        const auto idA = moving.tracks[0].clips[0].uid;
        const auto idC = moving.tracks[1].clips[0].uid;
        const std::map<juce::uint32, sis::ClipOrigin> origins { { idA, { 0.30, 0 } }, { idC, { 0.05, 1 } } };

        // Der Zeiger steht weit unten: trotzdem nur eine neue Spur hinter der letzten
        int created = 0;
        moving.moveClips (origins, 0.1, 5, created);
        expect ("Unten entsteht genau eine Spur", created == 1 && moving.tracks.size() == 3);
        expect ("a ist auf Spur 2", moving.trackOfClip (idA) == 1);
        expect ("c ist auf Spur 3", moving.trackOfClip (idC) == 2);
        expectNear ("Und 0,1 später", moving.tracks[1].findClip (idA)->offset, 0.40, 1.0e-12);

        // Zurück: die entstandene Spur verschwindet wieder, und nichts geht vor 0 oder über die erste Spur
        moving.moveClips (origins, -1.0, -3, created);
        expect ("Zurückgezogen verschwindet die neue Spur", created == 0 && moving.tracks.size() == 2);
        expect ("a wieder oben", moving.trackOfClip (idA) == 0 && moving.trackOfClip (idC) == 1);
        expectNear ("Nicht vor 0 (c war am weitesten vorn)", moving.tracks[1].findClip (idC)->offset, 0.0, 1.0e-12);
        expectNear ("a um dasselbe Stück", moving.tracks[0].findClip (idA)->offset, 0.25, 1.0e-12);
    }

    // --- Normalisieren: Pegel des Clips ---------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("quiet.wav", 4800, 0.25f));

        auto track = makeTrack ("quiet.wav");
        track.loop = sis::LoopMode::oneShot;
        track.clips[0].gain = 4.0f;

        auto model = makeModel ({ track });
        auto engine = makeEngine (*model, cache);
        const auto out = render (*engine, 2400, 60, 1.0f);
        expectNear ("Der Clip-Pegel wirkt", out.at (1200), 1.0, 0.01);

        juce::AudioFormatManager formats;
        sis::InstrumentModel loaded;
        loaded.fromValueTree (model->toValueTree(), formats);
        expectNear ("Er übersteht das Speichern", loaded.zones.front().tracks.front().clips.front().gain, 4.0, 1.0e-6);

        // Eine alte Datei: „gain“ an der Spur ist der Spurpegel, nicht der des Clips
        juce::ValueTree legacyTree ("Instrument");
        juce::ValueTree legacyZone ("Zone");
        juce::ValueTree legacyTrack ("Track");
        legacyTrack.setProperty ("clip", "quiet.wav", nullptr);
        legacyTrack.setProperty ("gain", 0.3f, nullptr);
        legacyZone.appendChild (legacyTrack, nullptr);
        legacyTree.appendChild (legacyZone, nullptr);
        sis::InstrumentModel legacy;
        legacy.fromValueTree (legacyTree, formats);
        const auto& legacyTrackRead = legacy.zones.front().tracks.front();
        expect ("Alter Spurpegel bleibt Spurpegel, der Clip bleibt bei 1",
                std::abs (legacyTrackRead.gain - 0.3f) < 1.0e-6f && legacyTrackRead.clips.front().gain == 1.0f);
    }

    // --- Überlappende Velocity-Zonen: Überblendung mit gleicher Leistung ------------------
    {
        sis::SampleCache cache;
        cache.insert (makeConstantSample ("soft.wav", 4800, 0.25f));
        cache.insert (makeConstantSample ("hard.wav", 4800, 1.0f));

        auto model = makeModel ({});
        model->zones.clear();

        const auto zoneOf = [] (const juce::String& id, const juce::String& sample, int low, int high)
        {
            sis::Zone z;
            z.id = id;
            z.lowNote = 24;
            z.highNote = 95;
            z.rootNote = 60;
            z.lowVelocity = low;
            z.highVelocity = high;
            auto track = makeTrack (sample);
            track.loop = sis::LoopMode::oneShot;
            z.tracks = { track };
            return z;
        };

        model->zones = { zoneOf ("soft", "soft.wav", 0, 90), zoneOf ("hard", "hard.wav", 60, 127) };

        const auto levelAt = [&] (int velocity)
        {
            auto engine = makeEngine (*model, cache);
            return (double) render (*engine, 2400, 60, (float) velocity / 127.0f).at (1200);
        };

        const double v30 = 30.0 / 127.0, v75 = 75.0 / 127.0, v110 = 110.0 / 127.0;
        expectNear ("Unterhalb der Überschneidung nur die leise Zone", levelAt (30), 0.25 * v30, 0.002);
        expectNear ("Oberhalb nur die laute", levelAt (110), 1.0 * v110, 0.002);

        // Mitte der Überschneidung 60 … 90: beide mit cos/sin(45°)
        const double half = std::sqrt (0.5);
        expectNear ("In der Mitte beide, mit gleicher Leistung", levelAt (75), (0.25 * half + 1.0 * half) * v75, 0.003);

        // Die Gewichte selbst: an den Rändern der Überschneidung ganz die eine, ganz die andere
        expectNear ("Am unteren Rand klingt die leise voll", sis::velocityWeight (0, 90, 60, 127, 60), 1.0, 1.0e-9);
        expectNear ("Am oberen Rand ist sie still", sis::velocityWeight (0, 90, 60, 127, 90), 0.0, 1.0e-9);
        expectNear ("Die laute blendet gegenläufig ein", sis::velocityWeight (60, 127, 0, 90, 60), 0.0, 1.0e-9);
        expectNear ("Leistung bleibt gleich",
                    std::pow (sis::velocityWeight (0, 90, 60, 127, 70), 2.0) + std::pow (sis::velocityWeight (60, 127, 0, 90, 70), 2.0),
                    1.0, 1.0e-9);
        expectNear ("Eine Zone ganz in der anderen: Schichtung, beide voll", sis::velocityWeight (40, 80, 0, 127, 60), 1.0, 1.0e-9);

        // Schichtung: dieselben Grenzen – beide klingen voll zusammen
        model->zones = { zoneOf ("a", "soft.wav", 0, 127), zoneOf ("b", "hard.wav", 0, 127) };
        expectNear ("Gleiche Bereiche werden geschichtet", levelAt (127), 1.25, 0.003);

        // Eine Zone ohne Spuren nimmt der anderen nichts weg
        auto empty = zoneOf ("leer", "soft.wav", 60, 127);
        empty.tracks.clear();
        model->zones = { zoneOf ("soft", "soft.wav", 0, 90), empty };
        expectNear ("Eine leere Zone blendet nichts aus", levelAt (85), 0.25 * 85.0 / 127.0, 0.002);
    }

    // --- Umkehren je Clip --------------------------------------------------------------
    {
        sis::SampleCache cache;
        cache.insert (makeRampSample ("ramp.wav", 4800));

        sis::Clip forward;
        forward.sample = "ramp.wav";
        forward.natural = 0.1 / sis::InstrumentModel::timelineSeconds;
        auto backward = forward;
        backward.uid = sis::Clip::nextUid();
        backward.reverse = true;
        backward.offset = 0.2 / sis::InstrumentModel::timelineSeconds;   // bei 0,2 s

        auto track = makeTrack ("ramp.wav");
        track.loop = sis::LoopMode::oneShot;
        track.clips = { forward, backward };

        auto model = makeModel ({ track });
        auto engine = makeEngine (*model, cache);
        const auto out = render (*engine, 16000, 60, 1.0f);

        expect ("Der erste Clip läuft vorwärts (steigt)", out.at (1000) < out.at (3000));
        expect ("Der zweite rückwärts (fällt)", out.at (10600) > out.at (12600));

        // Speichern und Laden; eine Datei bis 1.15 mit „Umkehren“ an der Spur
        juce::AudioFormatManager formats;
        sis::InstrumentModel loaded;
        loaded.fromValueTree (model->toValueTree(), formats);
        const auto& clips = loaded.zones.front().tracks.front().clips;
        expect ("Die Richtung je Clip übersteht das Speichern", clips.size() == 2 && ! clips[0].reverse && clips[1].reverse);

        juce::ValueTree legacyTree ("Instrument");
        juce::ValueTree legacyZone ("Zone");
        juce::ValueTree legacyTrack ("Track");
        legacyTrack.setProperty ("reverse", true, nullptr);
        for (const auto* name : { "a.wav", "b.wav" })
        {
            juce::ValueTree clipNode ("Clip");
            clipNode.setProperty ("clip", name, nullptr);
            legacyTrack.appendChild (clipNode, nullptr);
        }
        legacyZone.appendChild (legacyTrack, nullptr);
        legacyTree.appendChild (legacyZone, nullptr);

        sis::InstrumentModel legacy;
        legacy.fromValueTree (legacyTree, formats);
        const auto& legacyClips = legacy.zones.front().tracks.front().clips;
        expect ("Alte Datei: „Umkehren“ der Spur gilt für jeden ihrer Clips",
                legacyClips.size() == 2 && legacyClips[0].reverse && legacyClips[1].reverse);
    }

    std::printf ("%d Prüfungen, %d Fehler\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
