#pragma once

#include <JuceHeader.h>
#include <vector>

#include "PluginLibrary.h"
#include "../DSP/BitCrusher.h"
#include "../DSP/ChannelStrip.h"
#include "../DSP/Chorus.h"
#include "../DSP/Compressor.h"
#include "../DSP/DeEsser.h"
#include "../DSP/Delay.h"
#include "../DSP/Drive.h"
#include "../DSP/ModulatedDelay.h"
#include "../DSP/Phaser.h"
#include "../DSP/Tremolo.h"
#include "../DSP/Equalizer.h"
#include "../DSP/Gate.h"
#include "../DSP/Limiter.h"
#include "../DSP/Saturator.h"
#include "../DSP/SweptFilter.h"
#include "../DSP/TransientShaper.h"
#include "../Model/Instrument.h"
#include "SampleCache.h"

namespace sis
{
/** Eine Spur der Zone, fertig aufbereitet für die Wiedergabe.
    Alle Werte sind bereits in Samples bzw. Sekunden umgerechnet. */
/** Die Effektkette einer Spur.

    Je Art wirkt ein Effekt; die Reihenfolge in `steps` ist die der Kette in der Oberfläche. */
struct TrackEffectsPlan
{
    // Je Art wirkt ein Effekt, also reicht ein Platz pro Art
    static constexpr int maxSteps = 23;

    std::array<EffectType, maxSteps> steps {};
    int numSteps = 0;

    bool hasEqualizer = false;
    std::array<dsp::Equalizer::Coefficients::Ptr, dsp::Equalizer::numBands> equalizerCoefficients;
    std::array<bool, dsp::Equalizer::numBands> bandAudible {};

    bool hasCompressor = false;
    dsp::CompressorSettings compressor;

    bool hasReverb = false;
    juce::Reverb::Parameters reverb;

    // Der Kanalfilter benutzt denselben Baustein wie der Equalizer, nur mit
    // Hoch- und Tiefpass statt der vier Glocken.
    bool hasChannelFilter = false;
    std::array<dsp::Equalizer::Coefficients::Ptr, dsp::Equalizer::numBands> filterCoefficients;
    std::array<bool, dsp::Equalizer::numBands> filterAudible {};

    bool hasSaturation = false;
    dsp::SaturationSettings saturation;

    bool hasTransients = false;
    dsp::TransientSettings transients;

    bool hasChorus = false;
    dsp::ChorusSettings chorus;

    bool hasBitCrusher = false;
    dsp::BitCrusherSettings bitCrusher;

    bool hasOverdrive = false;
    dsp::DriveSettings overdrive;

    bool hasDistortion = false;
    dsp::DriveSettings distortion;

    bool hasFlanger = false;
    dsp::ModulatedDelaySettings flanger;

    bool hasVibrato = false;
    dsp::ModulatedDelaySettings vibrato;

    bool hasPhaser = false;
    dsp::PhaserSettings phaser;

    bool hasTremolo = false;
    dsp::TremoloSettings tremolo;

    bool hasDelay = false;
    dsp::DelaySettings delay;

    // Noise Gate und Expander rechnen dasselbe, nur mit anderen Einstellungen
    bool hasNoiseGate = false;
    dsp::GateSettings noiseGate;

    bool hasExpander = false;
    dsp::GateSettings expander;

    bool hasLimiter = false;
    dsp::LimiterSettings limiter;

    bool hasDeEsser = false;
    dsp::DeEsserSettings deEsser;

    // Envelope Filter und Auto-Wah: dasselbe Filter, verschieden bewegt
    bool hasEnvelopeFilter = false;
    dsp::SweptFilterSettings envelopeFilter;

    bool hasAutoWah = false;
    dsp::SweptFilterSettings autoWah;

    bool hasChannelStrip = false;
    dsp::ChannelStripSettings channelStrip;

    /** Fremdes Plugin; der Plan hält es am Leben, solange er benutzt wird. */
    HostedPlugin::Ptr external;

    bool isEmpty() const noexcept { return numSteps == 0; }
};

struct LayerPlan
{
    SampleData::Ptr sample;
    int busIndex = 0;              // Signalweg der Spur (für die Effektkette)

    double delaySeconds = 0.0;      // Versatz auf der Zeitachse (Spur-Offset)
    double startSample = 0.0;       // Ausschnitt im Sample (trimStart)
    double endSample = 0.0;         // trimEnd
    double fadeInSamples = 0.0;
    double fadeOutSamples = 0.0;

    float gainLeft = 1.0f;
    float gainRight = 1.0f;
    double semitoneOffset = 0.0;    // Spur-Tonhöhe in Halbtönen inkl. Cent
    double stretch = 1.0;           // Zeitdehnung; die Tonhöhe bleibt davon unberührt
    double grainSamples = 2048.0;   // Körnung des Überlappungsverfahrens (je nach Algorithmus)
    bool reverse = false;
    LoopMode loop = LoopMode::oneShot;
    double loopStartSamples = 0.0;      // Beginn der Schleife, gezählt ab Beginn des Ausschnitts
    double loopCrossfadeSamples = 0.0;  // Überblendung an der Naht; höchstens die halbe Schleife

    double getRegionLength() const noexcept { return endSample - startSample; }
};

/** Eine Zone mit ihren spielbaren Spuren. */
struct ZonePlan
{
    int lowNote = 0, highNote = 127;
    int lowVelocity = 0, highVelocity = 127;
    int rootNote = 60;

    /** Ob die Taste die Tonhöhe bestimmt. Beim Drumset nicht: dort klingt jedes Sample
        in seiner eigenen Tonhöhe, sonst wäre eine Snare zwei Oktaven höher eine andere
        Snare. Siehe `InstrumentKind`. */
    bool followsPitch = true;

    juce::ADSR::Parameters envelope;
    std::vector<LayerPlan> layers;

    bool matches (int note, int velocity) const noexcept
    {
        return note >= lowNote && note <= highNote && velocity >= lowVelocity && velocity <= highVelocity;
    }
};

/** Unveränderliche Momentaufnahme des Instruments für den Audio-Thread.
    Wird auf dem Message-Thread gebaut und danach nur noch gelesen. */
struct RenderPlan : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<RenderPlan>;

    std::vector<ZonePlan> zones;
    std::vector<TrackEffectsPlan> buses;   // ein Eintrag je Spur mit eigenem Signalweg

    const ZonePlan* zoneFor (int note, int velocity) const noexcept
    {
        for (const auto& zone : zones)
            if (zone.matches (note, velocity))
                return &zone;
        return nullptr;
    }

    int getNumLayers() const noexcept
    {
        int total = 0;
        for (const auto& zone : zones)
            total += (int) zone.layers.size();
        return total;
    }
};

/** Baut den Plan aus dem Modell: Mute/Solo, Trim, Fades, Pan und Tonhöhe sind darin
    schon aufgelöst. Spuren ohne geladenes Sample fallen weg. */
/** Liefert für einen externen Effekt die passende Instanz (oder nullptr). */
using HostedPluginProvider = std::function<HostedPlugin::Ptr (const juce::String& zoneId, int trackIndex,
                                                              const Effect&)>;

RenderPlan::Ptr buildRenderPlan (const InstrumentModel&, const SampleCache&, double sampleRate = 44100.0,
                                 const HostedPluginProvider& = {});

/** Einen einzelnen Effekt übersetzen – für die eigenständigen Effekt-Plugins.
    Dieselbe Umrechnung wie in der Effektkette des Studios. */
TrackEffectsPlan buildSingleEffect (const Effect&, double sampleRate);

/** Hüllkurve des Instruments in Sekunden bzw. als Pegel (wie in der rechten Spalte angezeigt). */
juce::ADSR::Parameters toAdsrParameters (const Envelope&);

/** Körnung des Time-Stretch je Algorithmus. */
double grainSizeFor (StretchAlgorithm);
} // namespace sis
