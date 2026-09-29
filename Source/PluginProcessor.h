#pragma once

#include <JuceHeader.h>

#include "Audio/Bounce.h"
#include "Audio/PluginLibrary.h"
#include "Audio/SampleCache.h"
#include "Audio/SamplerEngine.h"
#include "Model/Instrument.h"
#include "Model/PresetLibrary.h"
#include "Model/UndoHistory.h"

namespace sis
{
/** Gemeinsamer Prozessor für VST3, AU und Standalone. */
class StudioProcessor final : public juce::AudioProcessor,
                              private juce::ChangeListener,
                              private juce::Timer
{
public:
    StudioProcessor();
    ~StudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    InstrumentModel& getModel() noexcept                    { return model; }
    juce::MidiKeyboardState& getKeyboardState() noexcept    { return keyboardState; }
    juce::AudioFormatManager& getFormatManager() noexcept   { return formatManager; }
    juce::AudioParameterFloat& getMasterGain() noexcept     { return *masterGain; }
    double getCpuLoad()                                     { return loadMeasurer.getLoadAsProportion(); }

    int getActiveVoiceCount() const noexcept                { return engine.getActiveVoiceCount(); }

    /** Spitzenpegel des letzten Blocks, mit Nachleuchten – für die Anzeige. */
    float getOutputLevel() const noexcept                   { return outputLevel.load (std::memory_order_relaxed); }
    juce::int64 getSampleMemoryBytes() const                { return sampleCache.getTotalBytes(); }

    /** Ob die Zone überhaupt Audiodaten hat – sonst bleibt sie stumm. */
    bool zoneHasAudio (const Zone&) const;

    /** Verlauf der Änderungen; hängt am Änderungssignal des Modells. */
    UndoHistory& getHistory() noexcept { return history; }

    /** Liste der installierten fremden Effekte. */
    PluginLibrary& getPluginLibrary() noexcept { return pluginLibrary; }

    /** Gespeicherte Reglerstellungen je Effektart. */
    PresetLibrary& getPresetLibrary() noexcept { return presetLibrary; }

    /** Geladene Instanz eines externen Effekts, etwa um seine Oberfläche zu öffnen. */
    HostedPlugin::Ptr findHostedPlugin (const juce::String& zoneId, int trackIndex,
                                        const juce::String& pluginIdentifier) const;

    /** Sichert die Zustände aller geladenen Plugins ins Modell (vor dem Speichern). */
    void captureHostedPluginStates();

    /** Löscht eine Spur. Geht über den Prozessor und nicht direkt ans Modell, weil die
        fremden Plugins über die **Nummer** der Spur gefunden werden: nach dem Löschen
        rücken die Spuren dahinter auf, und ohne Neuaufbau bekäme eine Spur die
        Plugin-Instanz (samt Einstellungen) ihres Vorgängers. */
    void removeTrack (const juce::String& zoneId, int trackIndex);

    /** Der nächste Anschlag von `note` beginnt `seconds` hinten auf der Zeitachse
        (Wiedergabe ab dem Locator). */
    void setPlaybackStart (int note, double seconds) { engine.setStartOffset (note, seconds); }

    /** Ergebnis eines Bounce. */
    struct BounceOutcome
    {
        bool succeeded = false;
        juce::String message;
        juce::File file;
        double lengthSeconds = 0.0;
    };

    /** Rendert eine Zone samt Effekten in eine WAV-Datei und ersetzt ihre Spuren durch
        diese eine. Damit stecken auch fremde Plugins fest im Instrument und überleben
        den Export. Läuft auf dem Message-Thread. */
    BounceOutcome bounceZone (const juce::String& zoneId, const juce::File& targetFolder);

    /** Wohin Bounces standardmäßig geschrieben werden: neben die Projektdatei,
        sonst in die eigenen Dokumente. */
    juce::File getDefaultBounceFolder() const;

    /** Instrument, das im eigenen Plugin-Bundle mitgeliefert wurde (exportiertes Instrument).
        Leer, wenn das Plugin nicht aus einem Export stammt. */
    static juce::File findBundledInstrument();

    /** Wurde beim Start ein mitgeliefertes Instrument geladen? */
    bool isPlayingBundledInstrument() const noexcept { return loadedBundledInstrument; }

    /** Vollständiger Projektzustand (.sisp und Plugin-State). */
    juce::ValueTree createProjectState() const;
    bool loadProjectState (const juce::ValueTree&, const juce::File& baseFolder = juce::File());

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuildRenderPlan();

    juce::AudioFormatManager formatManager;
    InstrumentModel model;
    UndoHistory history { model, formatManager };   // nach Modell und Formaten anlegen
    SampleCache sampleCache;
    SamplerEngine engine;

    juce::MidiKeyboardState keyboardState;
    juce::AudioParameterFloat* masterGain = nullptr;
    juce::AudioProcessLoadMeasurer loadMeasurer;
    float lastGain = 0.0f;
    std::atomic<float> outputLevel { 0.0f };

    bool loadBundledInstrument();

    bool loadedBundledInstrument = false;

    // Fremde Plugins: je Spur und Kennung eine Instanz, die über Planwechsel hinweg bestehen bleibt
    static juce::String makeHostedKey (const juce::String& zoneId, int trackIndex,
                                       const juce::String& pluginIdentifier);
    HostedPlugin::Ptr provideHostedPlugin (const juce::String& zoneId, int trackIndex, const Effect&);

    PluginLibrary pluginLibrary;
    PresetLibrary presetLibrary;
    std::map<juce::String, HostedPlugin::Ptr> hostedPlugins;
    std::vector<HostedPlugin::Ptr> retiredPlugins;   // werden nur auf dem Message-Thread freigegeben

    RenderPlan::Ptr publishedPlan;
    std::vector<RenderPlan::Ptr> retiredPlans;   // werden nur auf dem Message-Thread freigegeben

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioProcessor)
};
} // namespace sis
