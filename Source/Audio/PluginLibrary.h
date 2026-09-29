#pragma once

#include <JuceHeader.h>

namespace sis
{
/** Eine geladene Instanz eines fremden Plugins.

    Wird auf dem Message-Thread erzeugt und dort auch wieder freigegeben; der Audio-Thread
    hält sie über den Renderplan nur als Zeiger. Deshalb ist sie referenzgezählt: Ein Plan,
    der noch klingt, hält sein Plugin am Leben. */
class HostedPlugin final : public juce::ReferenceCountedObject
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<HostedPlugin>;

    HostedPlugin (std::unique_ptr<juce::AudioPluginInstance>, juce::String identifier);
    ~HostedPlugin() override;

    /** Message-Thread: Kanäle und Blockgröße festlegen. */
    void prepare (double sampleRate, int blockSize);

    /** Audio-Thread: bearbeitet zwei Kanäle an Ort und Stelle. */
    void process (float* const* channels, int numSamples) noexcept;

    juce::AudioPluginInstance* getInstance() const noexcept { return instance.get(); }
    const juce::String& getIdentifier() const noexcept      { return identifier; }
    juce::String getName() const;

    /** Zustand des Plugins als Text, für die Projektdatei. */
    juce::String getStateAsString() const;
    void setStateFromString (const juce::String&);

private:
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::String identifier;
    juce::MidiBuffer emptyMidi;
    std::atomic<bool> readyToProcess { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedPlugin)
};

/** Kennt die installierten VST3-Effekte und legt Instanzen an.

    Ausschließlich vom Message-Thread benutzen. Die Liste wird in einer Datei neben den
    Programmeinstellungen abgelegt, damit nicht bei jedem Start neu gesucht werden muss. */
class PluginLibrary : private juce::ChangeBroadcaster
{
public:
    PluginLibrary();
    ~PluginLibrary();

    using juce::ChangeBroadcaster::addChangeListener;
    using juce::ChangeBroadcaster::removeChangeListener;

    /** Bekannte Effekte, alphabetisch. */
    juce::Array<juce::PluginDescription> getEffects() const;

    /** Nimmt eine einzelne Plugin-Datei in die Liste auf, etwa aus einem Dateidialog.
        Gibt die Kennungen der gefundenen Plugins zurück; bei Misserfolg leer und `error` gefüllt. */
    juce::StringArray addPluginFile (const juce::File&, juce::String& error);

    /** Sucht im Hintergrund in den Standard-Ordnern nach VST3-Plugins.
        `onFinished` kommt auf dem Message-Thread. */
    void startScan (std::function<void (int numFound)> onFinished);
    bool isScanning() const;
    juce::String getCurrentScanName() const;

    /** Legt eine Instanz an. Gibt bei Misserfolg nullptr zurück und füllt `error`. */
    HostedPlugin::Ptr createInstance (const juce::String& identifier, double sampleRate, int blockSize,
                                      juce::String& error);

    /** Wo die Liste gespeichert wird. */
    static juce::File getListFile();

    /** Die Ordner, in denen gesucht wird. */
    juce::FileSearchPath getSearchPaths() const;

private:
    class ScanJob;

    void loadList();
    void saveList() const;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    std::unique_ptr<ScanJob> scanJob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginLibrary)
};
} // namespace sis
