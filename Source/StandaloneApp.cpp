// Eigene Standalone-Anwendung (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1):
// Das Fenster hat unter Windows/Linux keine native Titelleiste; die App zeichnet
// ihre Fensterleiste selbst (UI/Shell/TitleBar). Audio/MIDI laufen über den
// StandalonePluginHolder von JUCE.

#include <JuceHeader.h>
#include "StudioHandover.h"

#if JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "PluginEditor.h"
#include "UI/StudioLookAndFeel.h"
#include "UI/Theme.h"

namespace sis
{
class StudioWindow final : public juce::DocumentWindow
{
public:
    StudioWindow (const juce::String& title, juce::StandalonePluginHolder& h)
        : DocumentWindow (title, colours::windowBar, juce::DocumentWindow::allButtons, true),
          holder (h)
    {
       #if JUCE_MAC
        setUsingNativeTitleBar (true);
       #else
        setUsingNativeTitleBar (false);
        setTitleBarHeight (0);
       #endif

        bool studio = true;

        if (auto* editor = holder.processor->createEditorAndMakeActive())
        {
            if (auto* studioEditor = dynamic_cast<StudioEditor*> (editor))
            {
                studioEditor->setAudioSettingsCallback ([this] { holder.showAudioSettingsDialog(); });
                studio = studioEditor->showsStudio();
            }

            setContentOwned (editor, true);
        }

        const auto screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto userArea = screen != nullptr ? screen->userBounds.toNearestInt() : juce::Rectangle<int> (0, 0, 1440, 900);
        setResizable (true, false);

        // Das Studio braucht Platz; die Spiel-Oberfläche einer exportierten App ist kompakt
        if (studio)
        {
            setResizeLimits (1100, 700, 10000, 10000);
            centreWithSize (juce::jmin (1440, userArea.getWidth()), juce::jmin (900, userArea.getHeight()));
        }
        else
        {
            setResizeLimits (720, 420, 10000, 10000);
            centreWithSize (juce::jmin (960, userArea.getWidth()), juce::jmin (560, userArea.getHeight()));
        }

        setVisible (true);
    }

    ~StudioWindow() override
    {
        // Editor vor dem Prozessor abbauen
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplicationBase::getInstance()->systemRequestedQuit();
    }

private:
    juce::StandalonePluginHolder& holder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioWindow)
};

//==============================================================================
class StudioApplication final : public juce::JUCEApplication
{
public:
    StudioApplication()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = JucePlugin_Name;
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName = JucePlugin_Name;
        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override    { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        // Auch die Dialoge von JUCE (Audio-Einstellungen, Dateiauswahl) im Look der App
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        holder = std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(), false);
        window = std::make_unique<StudioWindow> (getApplicationName(), *holder);

        /* Damit das Plugin die App überhaupt finden kann - es kennt nur den Pfad der DAW.
           Siehe StudioHandover.h. */
        handover::rememberStudioExecutable();

        openFileFromCommandLine (commandLine);
    }

    /** Beim Start übergebene .sisp-Datei laden - so kommt der Stand aus dem Plugin herüber. */
    void openFileFromCommandLine (const juce::String& commandLine)
    {
        const auto arguments = juce::StringArray::fromTokens (commandLine, true);

        for (const auto& argument : arguments)
        {
            const juce::File file (argument.unquoted());

            if (! file.existsAsFile() || ! file.hasFileExtension ("sisp"))
                continue;

            auto xml = juce::XmlDocument::parse (file);

            if (xml == nullptr)
                return;

            if (auto* processor = dynamic_cast<StudioProcessor*> (holder->processor.get()))
            {
                if (processor->loadProjectState (juce::ValueTree::fromXml (*xml), file.getParentDirectory()))
                {
                    /* Die Übergabedatei liegt im Temp-Ordner. Sie als Projektdatei zu
                       merken hieße, dass „Speichern“ dorthin schriebe - also nicht. */
                    processor->getModel().projectFile = juce::File();
                    processor->getModel().notifyChanged();
                }
            }

            return;
        }
    }

    void shutdown() override
    {
        if (holder != nullptr)
            holder->savePluginState();

        window = nullptr;
        holder = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    StudioLookAndFeel lookAndFeel;
    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandalonePluginHolder> holder;
    std::unique_ptr<StudioWindow> window;
};
} // namespace sis

juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new sis::StudioApplication();
}

#endif
