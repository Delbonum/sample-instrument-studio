#pragma once

#include <JuceHeader.h>

#include "../Audio/EffectChain.h"
#include "../Model/Instrument.h"
#include "../Model/PresetLibrary.h"

namespace sis
{
/** Ein interner Effekt als eigenständiges VST3.

    **Ein** Quelltext für alle: welcher Effekt daraus wird, sagt `SIS_EFFECT_TYPE` beim
    Übersetzen. Die Regler kommen aus derselben Fabrik (`Effect::make…()`) wie im Studio,
    die Umrechnung aus `buildSingleEffect`, das Rechnen aus `applyEffectChain`. Damit
    klingt „SIS Chorus“ in einer fremden DAW nicht *ähnlich* wie der Chorus in der
    Effektkette, sondern es ist derselbe Code.

    Koeffizienten entstehen wie im Studio **außerhalb des Audio-Threads**: ein Regler
    setzt nur ein Flag, ein Timer baut den Plan neu und reicht ihn unter einem SpinLock
    weiter. */
class EffectPlugin final : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    EffectPlugin();
    ~EffectPlugin() override;

    /** Welcher Effekt dieses Plugin ist. */
    static EffectType effectType() noexcept;

    /** Frisch aus der Fabrik – Name, Regler und deren Beschriftungen. */
    static Effect makePrototype();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    /** Presets – **dieselbe** Ablage wie im Studio: was hier gesichert wird, steht dort
        im „Presets“-Menü der Effektkarte und umgekehrt. */
    PresetLibrary& getPresets() noexcept { return presets; }
    juce::StringArray getPresetNames() const;
    bool loadPreset (const juce::String& name);
    void savePreset (const juce::String& name);
    void deletePreset (const juce::String& name);
    void resetToFactoryDefaults();

    float getOutputLevel() const noexcept { return outputLevel.load (std::memory_order_relaxed); }

    /** Regler-Kennung im Zustandsbaum: `p0`, `p1`, … */
    static juce::String parameterId (int index);

private:
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void rebuildPlan();

    const Effect prototype { makePrototype() };
    juce::AudioProcessorValueTreeState state;

    PresetLibrary presets;

    /* Der Plan wird auf dem Message-Thread gebaut und vom Audio-Thread nur gelesen –
       dieselbe Aufteilung wie beim Renderplan des Studios. */
    juce::SpinLock planLock;
    TrackEffectsPlan plan;
    std::atomic<bool> parametersChanged { true };

    /* Der ganze Satz, obwohl nur einer davon arbeitet. `BusProcessors::prepare` legt ein
       paar Puffer zu viel an (das Delay ist der größte Posten), dafür läuft das Plugin
       durch **genau** denselben Aufbau wie eine Spur im Studio. Ein eigener, schlanker
       Pfad wäre die zweite Stelle, an der dasselbe anders gemacht wird. */
    BusProcessors processors;
    double preparedSampleRate = 44100.0;
    std::atomic<float> outputLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectPlugin)
};
} // namespace sis
