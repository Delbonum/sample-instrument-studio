
#include "SamplerEngine.h"
#include "EffectChain.h"

namespace sis
{
namespace
{
    /** Linear interpoliertes Frame aus den Audiodaten. */
    inline void readFrame (const SampleData& data, double position, float& left, float& right)
    {
        const int numSamples = data.getNumSamples();

        if (numSamples < 2)
        {
            left = right = 0.0f;
            return;
        }

        const double clamped = juce::jlimit (0.0, (double) (numSamples - 2), position);
        const int index = (int) clamped;
        const float fraction = (float) (clamped - (double) index);

        const auto* l = data.buffer.getReadPointer (0);
        left = l[index] + (l[index + 1] - l[index]) * fraction;

        if (data.getNumChannels() > 1)
        {
            const auto* r = data.buffer.getReadPointer (1);
            right = r[index] + (r[index + 1] - r[index]) * fraction;
        }
        else
        {
            right = left;
        }
    }
    /** Wo im Sample gelesen wird: eine Stelle, in der Überblendung einer Schleife zwei. */
    struct ReadTaps
    {
        std::array<double, 2> positions {};
        std::array<double, 2> gains {};
        int count = 0;

        void add (double position, double gain) noexcept
        {
            positions[(size_t) count] = position;
            gains[(size_t) count] = gain;
            ++count;
        }
    };

    /* Überblendung mit gleicher Leistung: die beiden Enden der Schleife sind in aller
       Regel nicht phasengleich, bei linearer Blende gäbe es in der Mitte ein Loch. */
    inline double blendIn (double t)  { return std::sin (t * juce::MathConstants<double>::halfPi); }
    inline double blendOut (double t) { return std::cos (t * juce::MathConstants<double>::halfPi); }

    /** Lesestellen für einen Zeitpunkt `advanced` (Samples seit Beginn des Ausschnitts).
        Gibt false zurück, wenn der Ausschnitt bei One-Shot bereits zu Ende ist.

        Die Schleife liegt zwischen `loopStart` und dem Ende des Ausschnitts; davor klingt
        der Anschlag genau einmal. Mit Überblendung (Länge X) wird die Naht so gelegt:

          Sustain-Loop:  … b=E−X ⟶ [Ende E ausblenden | ab L einblenden] ⟶ L+X … b …
          Vor/Rückwärts: an beiden Umkehrpunkten wird die eine Richtung aus-, die andere
                         eingeblendet, statt hart umzukehren.

        Ohne Überblendung ergibt das genau die alte, harte Schleife. */
    inline bool sourceTapsFor (const LayerPlan& layer, double advanced, ReadTaps& taps)
    {
        taps.count = 0;

        if (advanced < 0.0)
            return false;

        const auto toSource = [&layer] (double a)
        {
            return layer.reverse ? layer.endSample - a : layer.startSample + a;
        };

        const double end = layer.getRegionLength();

        if (layer.loop == LoopMode::oneShot)
        {
            if (advanced >= end)
                return false;

            taps.add (toSource (advanced), 1.0);
            return true;
        }

        const double loopStart = layer.loopStartSamples;
        const double fade = layer.loopCrossfadeSamples;
        const double beforeFade = end - fade;   // bis hierher läuft der erste Durchgang ungestört

        if (advanced < beforeFade)
        {
            taps.add (toSource (advanced), 1.0);
            return true;
        }

        if (layer.loop == LoopMode::sustainLoop)
        {
            const double cycle = end - loopStart - fade;

            if (cycle < 1.0)
            {
                taps.add (toSource (loopStart), 1.0);
                return true;
            }

            const double q = std::fmod (advanced - beforeFade, cycle);

            if (q < fade)
            {
                const double t = q / fade;
                taps.add (toSource (beforeFade + q), blendOut (t));
                taps.add (toSource (loopStart + q), blendIn (t));
            }
            else
            {
                taps.add (toSource (loopStart + q), 1.0);
            }

            return true;
        }

        // Vor/Rückwärts
        const double low = loopStart + fade;         // ab hier ungeblendet vorwärts …
        const double straight = beforeFade - low;    // … bis beforeFade, gleich lang zurück
        const double period = 2.0 * (straight + fade);

        if (period < 1.0)
        {
            taps.add (toSource (loopStart), 1.0);
            return true;
        }

        const double r = std::fmod (advanced - beforeFade, period);

        if (r < fade)                                 // oben umkehren
        {
            const double t = r / fade;
            taps.add (toSource (beforeFade + r), blendOut (t));
            taps.add (toSource (end - r), blendIn (t));
        }
        else if (r < fade + straight)                 // rückwärts
        {
            taps.add (toSource (beforeFade - (r - fade)), 1.0);
        }
        else if (r < 2.0 * fade + straight)           // unten umkehren
        {
            const double q = r - fade - straight;
            const double t = q / fade;
            taps.add (toSource (low - q), blendOut (t));
            taps.add (toSource (loopStart + q), blendIn (t));
        }
        else                                          // vorwärts
        {
            taps.add (toSource (low + (r - 2.0 * fade - straight)), 1.0);
        }

        return true;
    }

    /** Alle Lesestellen gewichtet zusammengezählt. */
    inline void readTaps (const SampleData& data, const ReadTaps& taps, float& left, float& right)
    {
        left = right = 0.0f;

        for (int i = 0; i < taps.count; ++i)
        {
            float l = 0.0f, r = 0.0f;
            readFrame (data, taps.positions[(size_t) i], l, r);
            left += (float) (l * taps.gains[(size_t) i]);
            right += (float) (r * taps.gains[(size_t) i]);
        }
    }

    /** Hann-Fenster über die Kornlänge. */
    inline double hannWindow (double t, double grain)
    {
        return 0.5 * (1.0 - std::cos (2.0 * juce::MathConstants<double>::pi * t / grain));
    }
} // namespace

//==============================================================================
void SamplerVoice::readLayer (LayerState& state, float& left, float& right) const
{
    const auto& layer = *state.plan;
    left = right = 0.0f;

    // Ohne Dehnung direkt abspielen - exakt und ohne Nebenwirkungen
    if (std::abs (layer.stretch - 1.0) < 1.0e-6)
    {
        ReadTaps taps;

        if (sourceTapsFor (layer, state.position, taps))
            readTaps (*layer.sample, taps, left, right);

        return;
    }

    // Sonst überlappende Körner: der Lesezeiger wandert mit ratio (Tonhöhe),
    // die Körner selbst rücken mit timeRatio (Zeit) vor.
    const double grain = juce::jmax (64.0, layer.grainSamples);
    const double hop = grain * 0.5;
    const double p = state.outputPosition;
    const auto currentGrain = (juce::int64) std::floor (p / hop);

    for (int step = 0; step < 2; ++step)
    {
        const auto grainIndex = currentGrain - step;

        if (grainIndex < 0)
            continue;

        const double t = p - (double) grainIndex * hop;

        if (t < 0.0 || t >= grain)
            continue;

        const double advanced = (double) grainIndex * hop * state.timeRatio + t * state.ratio;
        ReadTaps taps;

        if (! sourceTapsFor (layer, advanced, taps))
            continue;

        // Das erste Korn hat keinen Vorgänger zum Überblenden
        const double window = (grainIndex == 0 && t < hop) ? 1.0 : hannWindow (t, grain);

        float sampleLeft = 0.0f, sampleRight = 0.0f;
        readTaps (*layer.sample, taps, sampleLeft, sampleRight);

        left += (float) (sampleLeft * window);
        right += (float) (sampleRight * window);
    }
}

//==============================================================================
void SamplerVoice::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    adsr.setSampleRate (newSampleRate);
    active = false;
    numLayers = 0;
    planReference = nullptr;
}

void SamplerVoice::start (RenderPlan::Ptr plan, const ZonePlan& zone, int midiNote, float velocity,
                          double skipSeconds)
{
    const double skip = juce::jmax (0.0, skipSeconds) * sampleRate;
    planReference = std::move (plan);
    note = midiNote;
    velocityGain = juce::jlimit (0.0f, 1.0f, velocity);
    numLayers = juce::jmin (maxLayers, (int) zone.layers.size());

    for (int i = 0; i < numLayers; ++i)
    {
        const auto& layer = zone.layers[(size_t) i];
        auto& state = layers[(size_t) i];

        state.plan = &layer;
        state.position = 0.0;
        state.outputPosition = 0.0;
        state.finished = false;
        state.delay = layer.delaySeconds * sampleRate;
        state.gateStart = juce::jmax (0.0, layer.gateStartSeconds) * sampleRate;
        state.gateEnd = std::isfinite (layer.gateEndSeconds) ? layer.gateEndSeconds * sampleRate
                                                             : std::numeric_limits<double>::infinity();

        /* Tonhöhe: Abstand zum Grundton, Spur-Tonhöhe und Cent, dazu die Samplerate des
           Samples. Beim Drumset entfällt der Abstand zur Taste – die Spur-Tonhöhe wirkt
           weiter, denn eine um zwei Halbtöne tiefer gestimmte Snare will man auch dort. */
        const double fromKey = zone.followsPitch ? (double) (midiNote - zone.rootNote) : 0.0;
        const double semitones = fromKey + layer.semitoneOffset;
        const double sampleRateRatio = layer.sample != nullptr ? layer.sample->sourceSampleRate / sampleRate : 1.0;
        state.ratio = std::pow (2.0, semitones / 12.0) * sampleRateRatio;
        state.timeRatio = state.ratio / juce::jmax (0.25, layer.stretch);

        /* Ein Abschnitt, der erst später hörbar wird, beginnt gleich dort – das Sample läuft
           innerlich weiter, als hätte es von Anfang an geklungen. */
        if (state.gateStart > 0.0)
        {
            state.delay += state.gateStart;
            state.outputPosition = state.gateStart;
            state.position = state.gateStart * state.timeRatio;
        }

        /* Wiedergabe ab dem Locator: was vor ihm liegt, wird übersprungen – erst der
           Versatz der Spur, dann der Anfang des Samples. */
        if (skip > 0.0)
        {
            if (state.delay >= skip)
            {
                state.delay -= skip;
            }
            else
            {
                const double into = skip - state.delay;
                state.delay = 0.0;
                state.outputPosition += into;
                state.position += into * state.timeRatio;
            }
        }
    }

    adsr.setParameters (zone.envelope);
    adsr.noteOn();
    active = numLayers > 0;
}

void SamplerVoice::stop (bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        adsr.reset();
        active = false;
        numLayers = 0;
        planReference = nullptr;
    }
}

void SamplerVoice::render (juce::AudioBuffer<float>& buses, int startSample, int numSamples)
{
    if (! active || numSamples <= 0)
        return;

    const int numBusChannels = buses.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const float envelope = adsr.getNextSample();
        const float voiceGain = envelope * velocityGain;
        bool anyPlaying = false;

        for (int layerIndex = 0; layerIndex < numLayers; ++layerIndex)
        {
            auto& state = layers[(size_t) layerIndex];

            if (state.finished || state.plan == nullptr || state.plan->sample == nullptr)
                continue;

            if (state.delay > 0.0)
            {
                state.delay -= 1.0;
                anyPlaying = true;
                continue;
            }

            const auto& layer = *state.plan;
            const double region = layer.getRegionLength();

            if (region < 2.0)
            {
                state.finished = true;
                continue;
            }

            // One-Shot endet, wenn der Zeitzeiger hinter dem Ausschnitt liegt
            if (layer.loop == LoopMode::oneShot && state.position >= region)
            {
                state.finished = true;
                continue;
            }

            // Ab hier liegt ein anderer Clip der Spur darüber
            if (state.outputPosition >= state.gateEnd)
            {
                state.finished = true;
                continue;
            }

            float sampleLeft = 0.0f, sampleRight = 0.0f;
            readLayer (state, sampleLeft, sampleRight);

            /* Fades als Anteil des Ausschnitts. Eine Schleife blendet nur einmal ein und
               nie aus – sie klingt, bis die Taste losgelassen wird. Früher liefen beide
               Fades in jeder Runde mit, und die Schleife pumpte an jeder Naht hörbar. */
            const double played = state.position;
            float fade = 1.0f;

            if (layer.fadeInSamples > 0.0 && played < layer.fadeInSamples)
                fade *= (float) (played / layer.fadeInSamples);

            if (layer.loop == LoopMode::oneShot && layer.fadeOutSamples > 0.0)
            {
                const double toEnd = region - played;

                if (toEnd < layer.fadeOutSamples)
                    fade *= (float) juce::jmax (0.0, toEnd / layer.fadeOutSamples);
            }

            /* An den Kanten eines Abschnitts (dort, wo ein anderer Clip übernimmt) kurz
               blenden statt hart schneiden – sonst knackt es. */
            const double edge = 0.003 * sampleRate;

            if (state.gateStart > 0.0 && state.outputPosition - state.gateStart < edge)
                fade *= (float) juce::jlimit (0.0, 1.0, (state.outputPosition - state.gateStart) / edge);

            if (std::isfinite (state.gateEnd) && state.gateEnd - state.outputPosition < edge)
                fade *= (float) juce::jlimit (0.0, 1.0, (state.gateEnd - state.outputPosition) / edge);

            // Jede Spur hat ihren eigenen Signalweg, damit Effekte darauf wirken können
            const int busChannel = 2 * juce::jlimit (0, numBusChannels / 2 - 1, layer.busIndex);

            buses.addSample (busChannel, startSample + i, sampleLeft * layer.gainLeft * fade * voiceGain);

            if (busChannel + 1 < numBusChannels)
                buses.addSample (busChannel + 1, startSample + i, sampleRight * layer.gainRight * fade * voiceGain);

            state.position += state.timeRatio;
            state.outputPosition += 1.0;
            anyPlaying = true;
        }

        if (! anyPlaying || ! adsr.isActive())
        {
            active = false;
            numLayers = 0;
            planReference = nullptr;
            break;
        }
    }
}

//==============================================================================
void BusProcessors::prepare (double sampleRate, int blockSize)
{
    const int safeBlock = juce::jmax (32, blockSize);

    equalizer.prepare (sampleRate, 2, safeBlock);
    channelFilter.prepare (sampleRate, 2, safeBlock);
    compressor.prepare (sampleRate, 2);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    saturator.prepare (sampleRate, 2);
    transients.prepare (sampleRate, 2);
    chorus.prepare (sampleRate, 2);
    bitCrusher.prepare (sampleRate, 2);
    overdrive.prepare (sampleRate, 2);
    distortion.prepare (sampleRate, 2);
    flanger.prepare (sampleRate, 2);
    vibrato.prepare (sampleRate, 2);
    phaser.prepare (sampleRate, 2);
    tremolo.prepare (sampleRate, 2);
    delay.prepare (sampleRate, 2);
    noiseGate.prepare (sampleRate, 2);
    expander.prepare (sampleRate, 2);
    limiter.prepare (sampleRate, 2);
    deEsser.prepare (sampleRate, 2);
    envelopeFilter.prepare (sampleRate, 2);
    autoWah.prepare (sampleRate, 2);
    channelStrip.prepare (sampleRate, 2, safeBlock);
}

void BusProcessors::reset()
{
    equalizer.reset();
    channelFilter.reset();
    compressor.reset();
    reverb.reset();
    saturator.reset();
    transients.reset();
    chorus.reset();
    bitCrusher.reset();
    overdrive.reset();
    distortion.reset();
    flanger.reset();
    vibrato.reset();
    phaser.reset();
    tremolo.reset();
    delay.reset();
    noiseGate.reset();
    expander.reset();
    limiter.reset();
    deEsser.reset();
    envelopeFilter.reset();
    autoWah.reset();
    channelStrip.reset();
}

void SamplerEngine::prepare (double newSampleRate, int blockSize)
{
    sampleRate = newSampleRate;

    for (auto& voice : voices)
        voice.prepare (newSampleRate);

    // Zwei Kanäle je Spur, vorab angelegt - im Audio-Thread wird nichts mehr angefordert
    busBuffer.setSize (2 * maxBuses, juce::jmax (32, blockSize), false, true, true);
    busBuffer.clear();

    for (auto& bus : busProcessors)
        bus.prepare (newSampleRate, blockSize);

    activeVoices.store (0, std::memory_order_relaxed);
}

void SamplerEngine::reset()
{
    for (auto& voice : voices)
        voice.stop (false);

    busBuffer.clear();

    for (auto& bus : busProcessors)
        bus.reset();

    activeVoices.store (0, std::memory_order_relaxed);
}

void SamplerEngine::setStartOffset (int note, double seconds)
{
    startOffsetSeconds.store (juce::jmax (0.0, seconds));
    startOffsetNote.store (note);
}

void SamplerEngine::setPlan (RenderPlan::Ptr plan)
{
    const juce::SpinLock::ScopedLockType lock (planLock);
    pendingPlan = std::move (plan);
}

void SamplerEngine::updatePlan()
{
    const juce::SpinLock::ScopedTryLockType lock (planLock);

    if (lock.isLocked() && pendingPlan != activePlan)
    {
        // Klingende Stimmen halten ihren alten Plan selbst am Leben
        activePlan = pendingPlan;
    }
}

SamplerVoice* SamplerEngine::findFreeVoice()
{
    for (auto& voice : voices)
        if (! voice.isActive())
            return &voice;

    // Sonst die älteste Stimme übernehmen
    SamplerVoice* oldest = &voices[0];

    for (auto& voice : voices)
        if (voice.getAge() < oldest->getAge())
            oldest = &voice;

    oldest->stop (false);
    return oldest;
}

void SamplerEngine::noteOn (int note, float velocity)
{
    if (activePlan == nullptr)
        return;

    const int midiVelocity = juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f));
    const auto* zone = activePlan->zoneFor (note, midiVelocity);

    if (zone == nullptr || zone->layers.empty())
        return;

    // Eine Wiedergabe ab dem Locator hat ihren Versatz vorher angemeldet – nur für diese Note
    double skipSeconds = 0.0;
    int expected = note;

    if (startOffsetNote.compare_exchange_strong (expected, -1))
        skipSeconds = startOffsetSeconds.load();

    auto* voice = findFreeVoice();
    voice->start (activePlan, *zone, note, velocity, skipSeconds);
    voice->startOrder = ++voiceCounter;
}

void SamplerEngine::noteOff (int note, bool allowTailOff)
{
    for (auto& voice : voices)
        if (voice.isActive() && voice.getNote() == note)
            voice.stop (allowTailOff);
}

void SamplerEngine::handleMessage (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        noteOn (message.getNoteNumber(), message.getFloatVelocity());
    else if (message.isNoteOff())
        noteOff (message.getNoteNumber(), true);
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        for (auto& voice : voices)
            voice.stop (! message.isAllSoundOff());
}

void SamplerEngine::renderVoices (int startSample, int numSamples)
{
    if (numSamples <= 0)
        return;

    for (auto& voice : voices)
        voice.render (busBuffer, startSample, numSamples);
}

void SamplerEngine::applyEffects (int startSample, int numSamples)
{
    if (activePlan == nullptr || numSamples <= 0)
        return;

    for (int bus = 0; bus < numActiveBuses; ++bus)
    {
        const auto& effects = activePlan->buses[(size_t) bus];

        if (effects.isEmpty())
            continue;

        float* channels[2] = { busBuffer.getWritePointer (2 * bus, startSample),
                               busBuffer.getWritePointer (2 * bus + 1, startSample) };

        applyEffectChain (effects, busProcessors[(size_t) bus], channels, numSamples);
    }
}

void SamplerEngine::mixBusesInto (juce::AudioBuffer<float>& buffer, int destinationOffset, int numSamples)
{
    if (numSamples <= 0)
        return;

    const int numChannels = buffer.getNumChannels();

    for (int bus = 0; bus < numActiveBuses; ++bus)
    {
        for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
            buffer.addFrom (channel, destinationOffset, busBuffer, 2 * bus + channel, 0, numSamples);

        // Mono-Ausgang: rechten Kanal dazumischen
        if (numChannels == 1)
            buffer.addFrom (0, destinationOffset, busBuffer, 2 * bus + 1, 0, numSamples);
    }
}

void SamplerEngine::processChunk (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi,
                                  int offset, int numSamples)
{
    for (int bus = 0; bus < numActiveBuses; ++bus)
    {
        busBuffer.clear (2 * bus, 0, numSamples);
        busBuffer.clear (2 * bus + 1, 0, numSamples);
    }

    int position = 0;

    for (const auto metadata : midi)
    {
        const int time = metadata.samplePosition - offset;

        if (time < 0 || time >= numSamples)
            continue;

        if (time > position)
        {
            renderVoices (position, time - position);
            position = time;
        }

        handleMessage (metadata.getMessage());
    }

    renderVoices (position, numSamples - position);

    // Effektkette je Spur, danach alles zusammenmischen
    applyEffects (0, numSamples);
    mixBusesInto (buffer, offset, numSamples);
}

void SamplerEngine::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    updatePlan();

    numActiveBuses = activePlan != nullptr ? juce::jmin (maxBuses, (int) activePlan->buses.size()) : 0;

    // Größere Blöcke als vorbereitet werden in Abschnitten verarbeitet,
    // damit im Audio-Thread nichts nachträglich angefordert werden muss.
    const int total = buffer.getNumSamples();
    const int maxChunk = juce::jmax (1, busBuffer.getNumSamples());

    for (int offset = 0; offset < total; offset += maxChunk)
        processChunk (buffer, midi, offset, juce::jmin (maxChunk, total - offset));

    int active = 0;

    for (const auto& voice : voices)
        if (voice.isActive())
            ++active;

    activeVoices.store (active, std::memory_order_relaxed);
}

} // namespace sis
