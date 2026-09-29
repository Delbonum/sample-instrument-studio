#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

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
#include "RenderPlan.h"

namespace sis
{
/** Eine klingende Note: spielt alle Spuren ihrer Zone übereinander,
    mit gemeinsamer Hüllkurve, je Spur mit Versatz, Ausschnitt, Fades, Tonhöhe und Panorama. */
class SamplerVoice
{
public:
    static constexpr int maxLayers = 16;

    void prepare (double sampleRate);
    /** `skipSeconds`: so weit hinten auf der Zeitachse beginnen (Wiedergabe ab dem Locator). */
    void start (RenderPlan::Ptr, const ZonePlan&, int midiNote, float velocity, double skipSeconds = 0.0);
    void stop (bool allowTailOff);

    bool isActive() const noexcept   { return active; }
    int getNote() const noexcept     { return note; }
    juce::uint32 getAge() const noexcept { return startOrder; }

    /** Schreibt in die Spur-Signalwege: je Spur zwei Kanäle im übergebenen Puffer. */
    void render (juce::AudioBuffer<float>& buses, int startSample, int numSamples);

private:
    struct LayerState
    {
        const LayerPlan* plan = nullptr;
        double position = 0.0;        // Zeitzeiger im Sample (Samples seit Beginn des Ausschnitts)
        double outputPosition = 0.0;  // ausgegebene Samples seit Beginn der Spur
        double ratio = 1.0;           // Abspielgeschwindigkeit für die Tonhöhe
        double timeRatio = 1.0;       // Fortschritt im Sample je ausgegebenem Sample (ratio / stretch)
        double delay = 0.0;           // verbleibender Versatz in Samples
        bool finished = false;
    };

    /** Ein Frame der Spur, bei Bedarf zeitgedehnt. */
    void readLayer (LayerState&, float& left, float& right) const;

    RenderPlan::Ptr planReference;   // hält den Plan, solange die Stimme klingt
    std::array<LayerState, maxLayers> layers;
    int numLayers = 0;

    juce::ADSR adsr;
    double sampleRate = 44100.0;
    int note = -1;
    float velocityGain = 1.0f;
    bool active = false;
    juce::uint32 startOrder = 0;

    friend class SamplerEngine;
};

/** Die Bausteine eines Spur-Signalwegs: je Art einer, gemeinsam vorbereitet.

    Ein Satz je Signalweg statt acht Arrays nebeneinander - was zusammengehört,
    steht so auch beieinander. */
struct BusProcessors
{
    dsp::Equalizer equalizer;
    dsp::Equalizer channelFilter;   // derselbe Baustein, nur mit Hoch- und Tiefpass
    dsp::Compressor compressor;
    juce::Reverb reverb;
    dsp::Saturator saturator;
    dsp::TransientShaper transients;
    dsp::Chorus chorus;
    dsp::BitCrusher bitCrusher;
    dsp::Drive overdrive;
    dsp::Drive distortion;
    dsp::ModulatedDelay flanger;
    dsp::ModulatedDelay vibrato;
    dsp::Phaser phaser;
    dsp::Tremolo tremolo;
    dsp::Delay delay;
    dsp::Gate noiseGate;
    dsp::Gate expander;
    dsp::Limiter limiter;
    dsp::DeEsser deEsser;
    dsp::SweptFilter envelopeFilter;
    dsp::SweptFilter autoWah;
    dsp::ChannelStrip channelStrip;

    void prepare (double sampleRate, int blockSize);
    void reset();
};

/** Polyphone Wiedergabe des Instruments.
    Der Plan wird vom Message-Thread gesetzt und vom Audio-Thread nur gelesen. */
class SamplerEngine
{
public:
    static constexpr int maxVoices = 24;
    static constexpr int maxBuses = 32;

    void prepare (double sampleRate, int blockSize);
    void reset();

    /** Message-Thread: neuen Plan übergeben. Der Audio-Thread übernimmt ihn beim nächsten Block. */
    void setPlan (RenderPlan::Ptr);

    /** Audio-Thread: MIDI verarbeiten und Stimmen mischen. */
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&);

    /** Message-Thread: der nächste Anschlag dieser Note beginnt `seconds` hinten auf der
        Zeitachse. Gilt genau einmal. */
    void setStartOffset (int note, double seconds);

    int getActiveVoiceCount() const noexcept { return activeVoices.load (std::memory_order_relaxed); }

private:
    void updatePlan();
    void processChunk (juce::AudioBuffer<float>&, const juce::MidiBuffer&, int offset, int numSamples);
    void renderVoices (int startSample, int numSamples);
    void applyEffects (int startSample, int numSamples);
    void mixBusesInto (juce::AudioBuffer<float>&, int destinationOffset, int numSamples);
    void handleMessage (const juce::MidiMessage&);
    void noteOn (int note, float velocity);
    void noteOff (int note, bool allowTailOff);
    SamplerVoice* findFreeVoice();

    juce::SpinLock planLock;
    RenderPlan::Ptr pendingPlan;                  // durch planLock geschützt
    RenderPlan::Ptr activePlan;                   // nur Audio-Thread
    std::array<SamplerVoice, maxVoices> voices;

    // Je Spur ein Stereo-Signalweg; darauf arbeitet die Effektkette
    juce::AudioBuffer<float> busBuffer;
    std::array<BusProcessors, maxBuses> busProcessors;
    int numActiveBuses = 0;
    std::atomic<int> activeVoices { 0 };
    std::atomic<int> startOffsetNote { -1 };
    std::atomic<double> startOffsetSeconds { 0.0 };
    juce::uint32 voiceCounter = 0;
    double sampleRate = 44100.0;
};
} // namespace sis
