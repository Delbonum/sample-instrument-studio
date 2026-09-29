#include "EffectChain.h"

namespace sis
{
void applyEffectChain (const TrackEffectsPlan& effects, BusProcessors& processors,
                       float* const* channels, int numSamples)
{
    if (numSamples <= 0 || effects.isEmpty())
        return;

    // In der Reihenfolge, in der die Effekte in der Kette stehen
    for (int step = 0; step < effects.numSteps; ++step)
    {
        switch (effects.steps[(size_t) step])
        {
            case EffectType::equalizer:
            {
                for (int band = 0; band < dsp::Equalizer::numBands; ++band)
                    processors.equalizer.setPreparedBand (band, effects.equalizerCoefficients[(size_t) band],
                                                          effects.bandAudible[(size_t) band]);

                processors.equalizer.process (channels, 2, numSamples);
                break;
            }

            case EffectType::channelFilter:
            {
                for (int band = 0; band < dsp::Equalizer::numBands; ++band)
                    processors.channelFilter.setPreparedBand (band, effects.filterCoefficients[(size_t) band],
                                                              effects.filterAudible[(size_t) band]);

                processors.channelFilter.process (channels, 2, numSamples);
                break;
            }

            case EffectType::compressor:
                processors.compressor.setSettings (effects.compressor);
                processors.compressor.process (channels, 2, numSamples);
                break;

            case EffectType::reverb:
                processors.reverb.setParameters (effects.reverb);
                processors.reverb.processStereo (channels[0], channels[1], numSamples);
                break;

            case EffectType::saturation:
                processors.saturator.setSettings (effects.saturation);
                processors.saturator.process (channels, 2, numSamples);
                break;

            case EffectType::transients:
                processors.transients.setSettings (effects.transients);
                processors.transients.process (channels, 2, numSamples);
                break;

            case EffectType::chorus:
                processors.chorus.setSettings (effects.chorus);
                processors.chorus.process (channels, 2, numSamples);
                break;

            case EffectType::bitCrusher:
                processors.bitCrusher.setSettings (effects.bitCrusher);
                processors.bitCrusher.process (channels, 2, numSamples);
                break;

            case EffectType::overdrive:
                processors.overdrive.setSettings (effects.overdrive);
                processors.overdrive.process (channels, 2, numSamples);
                break;

            case EffectType::distortion:
                processors.distortion.setSettings (effects.distortion);
                processors.distortion.process (channels, 2, numSamples);
                break;

            case EffectType::flanger:
                processors.flanger.setSettings (effects.flanger);
                processors.flanger.process (channels, 2, numSamples);
                break;

            case EffectType::vibrato:
                processors.vibrato.setSettings (effects.vibrato);
                processors.vibrato.process (channels, 2, numSamples);
                break;

            case EffectType::phaser:
                processors.phaser.setSettings (effects.phaser);
                processors.phaser.process (channels, 2, numSamples);
                break;

            case EffectType::tremolo:
                processors.tremolo.setSettings (effects.tremolo);
                processors.tremolo.process (channels, 2, numSamples);
                break;

            case EffectType::delay:
                processors.delay.setSettings (effects.delay);
                processors.delay.process (channels, 2, numSamples);
                break;

            case EffectType::noiseGate:
                processors.noiseGate.setSettings (effects.noiseGate);
                processors.noiseGate.process (channels, 2, numSamples);
                break;

            case EffectType::expander:
                processors.expander.setSettings (effects.expander);
                processors.expander.process (channels, 2, numSamples);
                break;

            case EffectType::limiter:
                processors.limiter.setSettings (effects.limiter);
                processors.limiter.process (channels, 2, numSamples);
                break;

            case EffectType::deEsser:
                processors.deEsser.setSettings (effects.deEsser);
                processors.deEsser.process (channels, 2, numSamples);
                break;

            case EffectType::envelopeFilter:
                processors.envelopeFilter.setSettings (effects.envelopeFilter);
                processors.envelopeFilter.process (channels, 2, numSamples);
                break;

            case EffectType::autoWah:
                processors.autoWah.setSettings (effects.autoWah);
                processors.autoWah.process (channels, 2, numSamples);
                break;

            case EffectType::channelStrip:
                processors.channelStrip.setSettings (effects.channelStrip);
                processors.channelStrip.process (channels, 2, numSamples);
                break;

            case EffectType::external:
                if (effects.external != nullptr)
                    effects.external->process (channels, numSamples);
                break;

            case EffectType::generic:
                break;
        }
    }
}
} // namespace sis
