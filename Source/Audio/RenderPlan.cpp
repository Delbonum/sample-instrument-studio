#include "RenderPlan.h"
#include "../Model/ClipGeometry.h"

namespace sis
{
double grainSizeFor (StretchAlgorithm algorithm)
{
    // Ein Überlappungsverfahren, vier Körnungen: kurze Körner halten Transienten besser,
    // lange klingen bei Flächen ruhiger.
    switch (algorithm)
    {
        case StretchAlgorithm::transientPreserving: return 768.0;
        case StretchAlgorithm::monophonic:          return 1536.0;
        case StretchAlgorithm::smooth:              return 4096.0;
        case StretchAlgorithm::granular:            return 6144.0;
    }

    return 2048.0;
}

juce::ADSR::Parameters toAdsrParameters (const Envelope& envelope)
{
    // Gleiche Skalierung wie die Anzeige: A bis 2 s, D bis 3 s, S als Pegel, R bis 5 s
    juce::ADSR::Parameters p;
    p.attack = juce::jmax (0.001f, envelope.attack * 2.0f);
    p.decay = juce::jmax (0.001f, envelope.decay * 3.0f);
    p.sustain = juce::jlimit (0.0f, 1.0f, envelope.sustain);
    p.release = juce::jmax (0.005f, envelope.release * 5.0f);
    return p;
}

namespace
{
    /** Parameterwert 0 … 1 als Anhebung -12 … +12 dB auf ein festes Band legen. */
    dsp::BandSettings bandFromParameter (int index, float value)
    {
        auto band = dsp::Equalizer::getDefaultBand (index);
        band.gainDb = (juce::jlimit (0.0f, 1.0f, value) - 0.5f) * 24.0f;
        return band;
    }

    /** Reglerwert 0 … 1 auf einen Bereich abbilden. */
    float mapRange (float value, float minimum, float maximum)
    {
        return minimum + juce::jlimit (0.0f, 1.0f, value) * (maximum - minimum);
    }

    float parameterValue (const Effect& effect, int index, float fallback = 0.5f)
    {
        return index < (int) effect.parameters.size() ? effect.parameters[(size_t) index].value : fallback;
    }

    /** Reglerwert 0 … 1 logarithmisch auf einen Frequenzbereich abbilden -
        linear wäre der halbe Weg schon bei 10 kHz. */
    float mapFrequency (float value, float minimum, float maximum)
    {
        const float t = juce::jlimit (0.0f, 1.0f, value);
        return minimum * std::pow (maximum / minimum, t);
    }

    void addChannelFilter (TrackEffectsPlan& effects, const Effect& effect, double sampleRate)
    {
        const float highPassHz = mapFrequency (parameterValue (effect, 0, 0.0f), 20.0f, 2000.0f);
        const float lowPassHz = mapFrequency (parameterValue (effect, 1, 1.0f), 200.0f, 20000.0f);
        const float q = mapRange (parameterValue (effect, 2, 0.2f), 0.5f, 4.0f);

        // Am Anschlag tut ein Filter nichts - dann bleibt er auch aus der Kette
        const bool cutsLows = highPassHz > 21.0f;
        const bool cutsHighs = lowPassHz < 19000.0f;

        dsp::BandSettings high;
        high.type = dsp::BandType::highPass;
        high.frequency = highPassHz;
        high.q = q;
        high.enabled = cutsLows;

        dsp::BandSettings low;
        low.type = dsp::BandType::lowPass;
        low.frequency = lowPassHz;
        low.q = q;
        low.enabled = cutsHighs;

        effects.filterCoefficients[0] = dsp::Equalizer::makeCoefficients (high, sampleRate);
        effects.filterAudible[0] = cutsLows;
        effects.filterCoefficients[1] = dsp::Equalizer::makeCoefficients (low, sampleRate);
        effects.filterAudible[1] = cutsHighs;

        for (int band = 2; band < dsp::Equalizer::numBands; ++band)
            effects.filterAudible[(size_t) band] = false;

        effects.hasChannelFilter = cutsLows || cutsHighs;
    }

    void addSaturation (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::SaturationSettings settings;
        settings.driveDb = mapRange (parameterValue (effect, 0, 0.35f), 0.0f, 24.0f);
        settings.mix = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 1, 1.0f));
        settings.outputDb = (juce::jlimit (0.0f, 1.0f, parameterValue (effect, 2, 0.5f)) - 0.5f) * 24.0f;

        effects.saturation = settings;
        effects.hasSaturation = settings.mix > 0.001f;
    }

    void addTransients (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::TransientSettings settings;
        settings.attack = (juce::jlimit (0.0f, 1.0f, parameterValue (effect, 0)) - 0.5f) * 2.0f;
        settings.sustain = (juce::jlimit (0.0f, 1.0f, parameterValue (effect, 1)) - 0.5f) * 2.0f;

        effects.transients = settings;
        effects.hasTransients = std::abs (settings.attack) > 0.01f || std::abs (settings.sustain) > 0.01f;
    }

    void addChorus (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::ChorusSettings settings;
        settings.depth = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 0, 0.45f));
        settings.rateHz = mapRange (parameterValue (effect, 1, 0.25f), 0.05f, 6.0f);
        settings.width = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 2, 0.8f));
        settings.mix = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 3, 0.4f));

        effects.chorus = settings;
        effects.hasChorus = settings.mix > 0.001f && settings.depth > 0.001f;
    }

    void addBitCrusher (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::BitCrusherSettings settings;

        // Der Regler läuft andersherum: rechts ist gröber
        settings.bits = mapRange (1.0f - juce::jlimit (0.0f, 1.0f, parameterValue (effect, 0, 0.55f)),
                                  2.0f, 16.0f);
        settings.downsample = mapRange (parameterValue (effect, 1, 0.0f), 1.0f, 32.0f);
        settings.mix = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 2, 0.6f));

        const bool changesAnything = settings.bits < 15.5f || settings.downsample > 1.01f;
        effects.bitCrusher = settings;
        effects.hasBitCrusher = changesAnything && settings.mix > 0.001f;
    }

    /** Gemeinsame Regler von Overdrive und Distortion. */
    dsp::DriveSettings driveFromParameters (const Effect& effect, dsp::DriveMode mode,
                                            float maxDriveDb, float defaultTone)
    {
        dsp::DriveSettings settings;
        settings.mode = mode;
        settings.driveDb = mapRange (parameterValue (effect, 0), 0.0f, maxDriveDb);
        settings.character = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 1));
        settings.toneHz = mapFrequency (parameterValue (effect, 2, defaultTone), 800.0f, 20000.0f);
        settings.mix = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 3, 1.0f));
        settings.outputDb = (juce::jlimit (0.0f, 1.0f, parameterValue (effect, 4, 0.5f)) - 0.5f) * 24.0f;
        return settings;
    }

    void addOverdrive (TrackEffectsPlan& effects, const Effect& effect)
    {
        effects.overdrive = driveFromParameters (effect, dsp::DriveMode::overdrive, 30.0f, 0.7f);
        effects.hasOverdrive = effects.overdrive.mix > 0.001f;
    }

    void addDistortion (TrackEffectsPlan& effects, const Effect& effect)
    {
        effects.distortion = driveFromParameters (effect, dsp::DriveMode::distortion, 48.0f, 0.5f);
        effects.hasDistortion = effects.distortion.mix > 0.001f;
    }

    void addFlanger (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::ModulatedDelaySettings settings;
        settings.mode = dsp::DelayMode::flanger;
        settings.depth = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 0, 0.6f));
        settings.rateHz = mapRange (parameterValue (effect, 1, 0.18f), 0.05f, 8.0f);
        settings.feedback = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 2, 0.5f)) * 0.9f;
        settings.mix = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 3, 0.5f));

        effects.flanger = settings;
        effects.hasFlanger = settings.mix > 0.001f;
    }

    void addVibrato (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::ModulatedDelaySettings settings;
        settings.mode = dsp::DelayMode::vibrato;
        settings.depth = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 0, 0.3f));
        settings.rateHz = mapRange (parameterValue (effect, 1, 0.45f), 0.3f, 12.0f);

        effects.vibrato = settings;
        effects.hasVibrato = settings.depth > 0.001f;
    }

    void addPhaser (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::PhaserSettings settings;
        settings.depth = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 0, 0.75f));
        settings.rateHz = mapRange (parameterValue (effect, 1, 0.2f), 0.05f, 6.0f);
        settings.feedback = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 2, 0.45f)) * 0.9f;
        settings.mix = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 3, 0.5f));

        effects.phaser = settings;
        effects.hasPhaser = settings.mix > 0.001f;
    }

    void addTremolo (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::TremoloSettings settings;
        settings.depth = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 0, 0.6f));
        settings.rateHz = mapRange (parameterValue (effect, 1, 0.35f), 0.1f, 20.0f);
        settings.shape = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 2, 0.0f));
        settings.width = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 3, 0.0f));

        effects.tremolo = settings;
        effects.hasTremolo = settings.depth > 0.001f;
    }

    void addDelay (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::DelaySettings settings;

        // Zeit logarithmisch: linear läge die untere Hälfte des Wegs im Flattern
        settings.timeMs = mapFrequency (parameterValue (effect, 0, 0.35f), 10.0f,
                                        (float) (dsp::Delay::maxTimeSeconds * 1000.0));
        settings.feedback = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 1, 0.4f)) * 0.95f;
        settings.damping = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 2, 0.4f));
        settings.pingPong = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 3, 0.0f));
        settings.mix = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 4, 0.3f));

        effects.delay = settings;
        effects.hasDelay = settings.mix > 0.001f;
    }

    void addNoiseGate (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::GateSettings settings;
        settings.thresholdDb = mapRange (parameterValue (effect, 0, 0.3f), -80.0f, 0.0f);
        settings.ratio = 10.0f;   // steil - das Gate soll schließen, nicht dosieren
        settings.attackMs = mapRange (parameterValue (effect, 1, 0.1f), 0.1f, 50.0f);
        settings.holdMs = mapRange (parameterValue (effect, 2, 0.3f), 0.0f, 500.0f);
        settings.releaseMs = mapRange (parameterValue (effect, 3, 0.3f), 5.0f, 1000.0f);
        settings.rangeDb = mapRange (parameterValue (effect, 4, 0.8f), 0.0f, 80.0f);

        effects.noiseGate = settings;
        effects.hasNoiseGate = settings.rangeDb > 0.1f;
    }

    void addExpander (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::GateSettings settings;
        settings.thresholdDb = mapRange (parameterValue (effect, 0, 0.4f), -80.0f, 0.0f);

        // Unten fein, oben bis 8:1 - wie beim Kompressor ungleichmäßig aufgeteilt
        const float ratioValue = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 1, 0.3f));
        settings.ratio = 1.0f + 7.0f * ratioValue * ratioValue;

        settings.attackMs = mapRange (parameterValue (effect, 2, 0.2f), 0.1f, 50.0f);
        settings.releaseMs = mapRange (parameterValue (effect, 3, 0.35f), 5.0f, 1000.0f);
        settings.holdMs = 0.0f;     // der Expander dosiert, er hält nicht
        settings.rangeDb = 40.0f;

        effects.expander = settings;
        effects.hasExpander = settings.ratio > 1.01f;
    }

    void addLimiter (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::LimiterSettings settings;
        settings.inputDb = mapRange (parameterValue (effect, 0, 0.0f), 0.0f, 24.0f);
        settings.ceilingDb = mapRange (parameterValue (effect, 1, 0.95f), -24.0f, 0.0f);
        settings.releaseMs = mapRange (parameterValue (effect, 2, 0.25f), 1.0f, 500.0f);

        effects.limiter = settings;
        effects.hasLimiter = true;
    }

    void addDeEsser (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::DeEsserSettings settings;
        settings.frequencyHz = mapFrequency (parameterValue (effect, 0, 0.5f), 2000.0f, 14000.0f);
        settings.thresholdDb = mapRange (parameterValue (effect, 1, 0.45f), -60.0f, 0.0f);
        settings.rangeDb = mapRange (parameterValue (effect, 2, 0.5f), 0.0f, 30.0f);
        settings.releaseMs = mapRange (parameterValue (effect, 3, 0.3f), 5.0f, 300.0f);

        effects.deEsser = settings;
        effects.hasDeEsser = settings.rangeDb > 0.1f;
    }

    void addEnvelopeFilter (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::SweptFilterSettings settings;
        settings.source = dsp::SweepSource::envelope;
        settings.lowHz = mapFrequency (parameterValue (effect, 0, 0.25f), 120.0f, 1200.0f);
        settings.highHz = settings.lowHz * 8.0f;   // der Anschlag zieht bis drei Oktaven hinauf
        settings.sensitivity = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 1, 0.6f));
        settings.resonance = mapRange (parameterValue (effect, 2, 0.5f), 0.7f, 6.0f);
        settings.attackMs = mapRange (parameterValue (effect, 3, 0.2f), 1.0f, 100.0f);
        settings.releaseMs = mapRange (parameterValue (effect, 4, 0.35f), 20.0f, 800.0f);
        settings.mix = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 5, 0.8f));

        effects.envelopeFilter = settings;
        effects.hasEnvelopeFilter = settings.mix > 0.001f;
    }

    void addAutoWah (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::SweptFilterSettings settings;
        settings.source = dsp::SweepSource::lfo;
        settings.lowHz = mapFrequency (parameterValue (effect, 0, 0.2f), 120.0f, 2000.0f);
        settings.highHz = mapFrequency (parameterValue (effect, 1, 0.7f), 300.0f, 5000.0f);
        settings.rateHz = mapRange (parameterValue (effect, 2, 0.25f), 0.05f, 8.0f);
        settings.resonance = mapRange (parameterValue (effect, 3, 0.55f), 0.7f, 6.0f);
        settings.mix = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 4, 0.8f));

        effects.autoWah = settings;
        effects.hasAutoWah = settings.mix > 0.001f;
    }

    void addChannelStrip (TrackEffectsPlan& effects, const Effect& effect, double sampleRate)
    {
        dsp::ChannelStripSettings settings;

        // --- Gate: ganz links praktisch nie, ganz rechts schon bei leisem Material ---
        const float gateValue = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 0, 0.0f));
        settings.gateActive = gateValue > 0.02f;
        settings.gate.thresholdDb = mapRange (gateValue, -70.0f, -15.0f);
        settings.gate.ratio = 10.0f;
        settings.gate.rangeDb = 40.0f;
        settings.gate.attackMs = 2.0f;
        settings.gate.holdMs = 40.0f;
        settings.gate.releaseMs = 150.0f;

        // --- Klangregelung: drei Bänder, eigens für den Streifen gewählt ---
        const dsp::BandType types[] = { dsp::BandType::lowShelf, dsp::BandType::peak,
                                        dsp::BandType::highShelf };
        const float frequencies[] = { 150.0f, 1000.0f, 6000.0f };
        const float qualities[] = { 0.707f, 0.9f, 0.707f };

        for (int band = 0; band < 3; ++band)
        {
            dsp::BandSettings setting;
            setting.type = types[band];
            setting.frequency = frequencies[band];
            setting.q = qualities[band];
            setting.gainDb = (juce::jlimit (0.0f, 1.0f, parameterValue (effect, band + 1)) - 0.5f) * 24.0f;

            settings.equalizer[(size_t) band] = dsp::Equalizer::makeCoefficients (setting, sampleRate);
            settings.bandAudible[(size_t) band] = dsp::Equalizer::isBandAudible (setting);
        }

        settings.bandAudible[3] = false;   // das vierte Band bleibt ungenutzt

        /* --- Kompression an einem Regler ---
           Schwelle, Verhältnis und Ausgleich laufen mit; getrennt einstellbar sind sie
           im eigenen Kompressor, hier geht es um das schnelle „mehr davon“. */
        const float amount = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 4, 0.0f));
        settings.compressorActive = amount > 0.02f;
        settings.compressor.thresholdDb = -6.0f - amount * 30.0f;
        settings.compressor.ratio = 1.5f + amount * 4.5f;
        settings.compressor.attackMs = 12.0f;
        settings.compressor.releaseMs = 180.0f;
        settings.compressor.makeupDb = amount * 8.0f;

        settings.outputGain = juce::Decibels::decibelsToGain (
            (juce::jlimit (0.0f, 1.0f, parameterValue (effect, 5, 0.5f)) - 0.5f) * 24.0f);

        effects.channelStrip = settings;
        effects.hasChannelStrip = true;
    }

    void addEqualizer (TrackEffectsPlan& effects, const Effect& effect, double sampleRate)
    {
        for (int band = 0; band < dsp::Equalizer::numBands; ++band)
        {
            const auto settings = bandFromParameter (band, parameterValue (effect, band));
            effects.equalizerCoefficients[(size_t) band] = dsp::Equalizer::makeCoefficients (settings, sampleRate);
            effects.bandAudible[(size_t) band] = dsp::Equalizer::isBandAudible (settings);
            effects.hasEqualizer = effects.hasEqualizer || effects.bandAudible[(size_t) band];
        }
    }

    void addCompressor (TrackEffectsPlan& effects, const Effect& effect)
    {
        dsp::CompressorSettings settings;
        settings.thresholdDb = mapRange (parameterValue (effect, 0), -48.0f, 0.0f);

        // Verhältnis ungleichmäßig: unten fein, oben bis 20:1
        const float ratioValue = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 1));
        settings.ratio = 1.0f + 19.0f * ratioValue * ratioValue;

        settings.attackMs = mapRange (parameterValue (effect, 2), 1.0f, 200.0f);
        settings.releaseMs = mapRange (parameterValue (effect, 3), 20.0f, 1000.0f);
        settings.makeupDb = mapRange (parameterValue (effect, 4, 0.0f), 0.0f, 24.0f);

        effects.compressor = settings;
        effects.hasCompressor = true;
    }

    void addReverb (TrackEffectsPlan& effects, const Effect& effect)
    {
        juce::Reverb::Parameters parameters;
        parameters.roomSize = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 0));
        parameters.damping = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 1));
        parameters.width = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 2, 1.0f));

        const float mix = juce::jlimit (0.0f, 1.0f, parameterValue (effect, 3, 0.25f));
        parameters.wetLevel = mix;
        parameters.dryLevel = 1.0f - mix;
        parameters.freezeMode = 0.0f;

        effects.reverb = parameters;
        effects.hasReverb = mix > 0.001f;
    }

    TrackEffectsPlan buildEffects (const Track& track, double sampleRate,
                                   const HostedPluginProvider& hostedPlugins,
                                   const juce::String& zoneId, int trackIndex)
    {
        TrackEffectsPlan effects;

        for (const auto& effect : track.effects)
        {
            if (! effect.enabled || effects.numSteps >= TrackEffectsPlan::maxSteps)
                continue;

            const bool alreadyUsed = std::any_of (effects.steps.begin(),
                                                  effects.steps.begin() + effects.numSteps,
                                                  [&effect] (EffectType type) { return type == effect.type; });

            if (alreadyUsed)
                continue;   // je Art wirkt der erste Effekt der Kette

            switch (effect.type)
            {
                case EffectType::equalizer:  addEqualizer (effects, effect, sampleRate); break;
                case EffectType::compressor: addCompressor (effects, effect); break;
                case EffectType::reverb:     addReverb (effects, effect); break;

                case EffectType::channelFilter: addChannelFilter (effects, effect, sampleRate); break;
                case EffectType::saturation:    addSaturation (effects, effect); break;
                case EffectType::transients:    addTransients (effects, effect); break;
                case EffectType::chorus:        addChorus (effects, effect); break;
                case EffectType::bitCrusher:    addBitCrusher (effects, effect); break;
                case EffectType::overdrive:     addOverdrive (effects, effect); break;
                case EffectType::distortion:    addDistortion (effects, effect); break;
                case EffectType::flanger:       addFlanger (effects, effect); break;
                case EffectType::vibrato:       addVibrato (effects, effect); break;
                case EffectType::phaser:        addPhaser (effects, effect); break;
                case EffectType::tremolo:       addTremolo (effects, effect); break;
                case EffectType::delay:         addDelay (effects, effect); break;
                case EffectType::noiseGate:     addNoiseGate (effects, effect); break;
                case EffectType::expander:      addExpander (effects, effect); break;
                case EffectType::limiter:       addLimiter (effects, effect); break;
                case EffectType::deEsser:       addDeEsser (effects, effect); break;
                case EffectType::envelopeFilter: addEnvelopeFilter (effects, effect); break;
                case EffectType::autoWah:       addAutoWah (effects, effect); break;
                case EffectType::channelStrip:  addChannelStrip (effects, effect, sampleRate); break;

                case EffectType::external:
                    if (hostedPlugins != nullptr)
                        effects.external = hostedPlugins (zoneId, trackIndex, effect);
                    break;

                case EffectType::generic:    continue;   // noch ohne Klang
            }

            const bool active = (effect.type == EffectType::equalizer && effects.hasEqualizer)
                             || (effect.type == EffectType::compressor && effects.hasCompressor)
                             || (effect.type == EffectType::reverb && effects.hasReverb)
                             || (effect.type == EffectType::channelFilter && effects.hasChannelFilter)
                             || (effect.type == EffectType::saturation && effects.hasSaturation)
                             || (effect.type == EffectType::transients && effects.hasTransients)
                             || (effect.type == EffectType::chorus && effects.hasChorus)
                             || (effect.type == EffectType::bitCrusher && effects.hasBitCrusher)
                             || (effect.type == EffectType::overdrive && effects.hasOverdrive)
                             || (effect.type == EffectType::distortion && effects.hasDistortion)
                             || (effect.type == EffectType::flanger && effects.hasFlanger)
                             || (effect.type == EffectType::vibrato && effects.hasVibrato)
                             || (effect.type == EffectType::phaser && effects.hasPhaser)
                             || (effect.type == EffectType::tremolo && effects.hasTremolo)
                             || (effect.type == EffectType::delay && effects.hasDelay)
                             || (effect.type == EffectType::noiseGate && effects.hasNoiseGate)
                             || (effect.type == EffectType::expander && effects.hasExpander)
                             || (effect.type == EffectType::limiter && effects.hasLimiter)
                             || (effect.type == EffectType::deEsser && effects.hasDeEsser)
                             || (effect.type == EffectType::envelopeFilter && effects.hasEnvelopeFilter)
                             || (effect.type == EffectType::autoWah && effects.hasAutoWah)
                             || (effect.type == EffectType::channelStrip && effects.hasChannelStrip)
                             || (effect.type == EffectType::external && effects.external != nullptr);

            if (active)
                effects.steps[(size_t) effects.numSteps++] = effect.type;
        }

        return effects;
    }
}

TrackEffectsPlan buildSingleEffect (const Effect& effect, double sampleRate)
{
    /* Bewusst über denselben Weg wie die Kette im Studio: eine zweite Umrechnung daneben
       wäre die Stelle, an der ein Plugin eines Tages anders klingt als der Effekt, von dem
       es abstammt. */
    Track track;
    track.effects = { effect };
    return buildEffects (track, sampleRate, {}, {}, 0);
}

RenderPlan::Ptr buildRenderPlan (const InstrumentModel& model, const SampleCache& cache, double sampleRate,
                                 const HostedPluginProvider& hostedPlugins)
{
    RenderPlan::Ptr plan (new RenderPlan());
    const auto envelope = toAdsrParameters (model.envelope);

    for (const auto& zone : model.zones)
    {
        ZonePlan zonePlan;
        zonePlan.lowNote = zone.lowNote;
        zonePlan.highNote = zone.highNote;
        zonePlan.lowVelocity = zone.lowVelocity;
        zonePlan.highVelocity = zone.highVelocity;
        zonePlan.rootNote = zone.rootNote;
        zonePlan.followsPitch = model.kind != InstrumentKind::drumKit;
        zonePlan.envelope = envelope;

        const bool anySolo = std::any_of (zone.tracks.begin(), zone.tracks.end(),
                                          [] (const Track& t) { return t.solo; });

        for (int trackIndex = 0; trackIndex < (int) zone.tracks.size(); ++trackIndex)
        {
            const auto& track = zone.tracks[(size_t) trackIndex];

            if (track.mute || (anySolo && ! track.solo))
                continue;

            /* Die Clips der Spur als Strecken: daraus folgt, wo jeder zu hören ist. Clips
               ohne Sample verdecken nichts. */
            std::vector<geometry::Span> spans;

            for (const auto& clip : track.clips)
            {
                if (! clip.hasSample())
                {
                    spans.push_back ({ clip.offset, clip.offset, 0.0, 0.0 });
                    continue;
                }

                const double length = clip.length();
                spans.push_back ({ clip.offset, clip.end(), clip.fadeIn * length, clip.fadeOut * length });
            }

            const bool looping = track.loop != LoopMode::oneShot;
            int busIndex = -1;

            for (size_t clipIndex = 0; clipIndex < track.clips.size(); ++clipIndex)
            {
                const auto& clip = track.clips[clipIndex];

                if (! clip.hasSample())
                    continue;

                auto sample = cache.get (clip.sample);

                if (sample == nullptr || sample->getNumSamples() < 2)
                    continue;

                const auto& span = spans[clipIndex];
                const double until = looping ? geometry::loopUntil (spans, clipIndex) : span.end;
                const auto segments = geometry::audibleSegments (spans, clipIndex, until);

                if (segments.empty())
                    continue;

                // Ein Signalweg je Spur, gemeinsam für alle ihre Clips
                if (busIndex < 0)
                {
                    busIndex = (int) plan->buses.size();
                    plan->buses.push_back (buildEffects (track, sampleRate, hostedPlugins, zone.id, trackIndex));
                }

                const double total = (double) sample->getNumSamples();
                const double start = juce::jlimit (0.0, total - 1.0, clip.trimStart * total);
                const double end = juce::jlimit (start + 1.0, total, clip.trimEnd * total);
                const double region = end - start;

                LayerPlan layer;
                layer.sample = sample;
                layer.busIndex = busIndex;
                layer.delaySeconds = clip.offset * InstrumentModel::timelineSeconds;
                layer.startSample = start;
                layer.endSample = end;
                layer.fadeInSamples = juce::jlimit (0.0, region, clip.fadeIn * region);
                layer.fadeOutSamples = juce::jlimit (0.0, region - layer.fadeInSamples, clip.fadeOut * region);
                layer.reverse = track.reverse;
                layer.loop = track.loop;

                /* Schleife und Überblendung beziehen sich auf den **gespielten** Ausschnitt
                   des Clips, nicht auf das ganze Sample. Die Schleife braucht mindestens
                   zwei Samples, und die Überblendung darf höchstens die halbe Schleife lang
                   sein: sie nimmt von beiden Enden. */
                layer.loopStartSamples = juce::jlimit (0.0, juce::jmax (0.0, region - 2.0), track.loopStart * region);
                const double loopLength = region - layer.loopStartSamples;
                layer.loopCrossfadeSamples = juce::jlimit (0.0, juce::jmax (0.0, loopLength * 0.5 - 1.0),
                                                           track.loopCrossfade * loopLength);
                layer.stretch = juce::jlimit (0.25, 4.0, clip.stretch);
                layer.grainSamples = grainSizeFor (track.algorithm);
                layer.keepTempo = track.keepTempo;
                layer.semitoneOffset = (double) track.pitch + ((double) track.cents - 0.5);

                // Panorama mit 0 dB in der Mitte: der angezeigte Spurpegel ist auch der gehörte,
                // und beim Schwenken wird nichts angehoben.
                const double pan = juce::jlimit (-1.0f, 1.0f, track.pan);
                layer.gainLeft = (float) (track.gain * juce::jmin (1.0, 1.0 - pan));
                layer.gainRight = (float) (track.gain * juce::jmin (1.0, 1.0 + pan));

                /* Je hörbarem Abschnitt eine Schicht. Was darüberliegende Clips verdecken,
                   bleibt still; ein Abschnitt, der am natürlichen Clip-Ende endet, braucht
                   kein eigenes Ende. */
                for (const auto& [from, to] : segments)
                {
                    auto part = layer;
                    part.gateStartSeconds = (from - clip.offset) * InstrumentModel::timelineSeconds;

                    const bool naturalEnd = ! std::isfinite (to) || (! looping && to >= span.end - 1.0e-12);
                    part.gateEndSeconds = naturalEnd ? std::numeric_limits<double>::infinity()
                                                     : (to - clip.offset) * InstrumentModel::timelineSeconds;

                    zonePlan.layers.push_back (std::move (part));
                }
            }
        }

        plan->zones.push_back (std::move (zonePlan));
    }

    return plan;
}
} // namespace sis
