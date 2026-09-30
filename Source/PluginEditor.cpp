#include "PluginEditor.h"
#include "UI/Panels/HostedPluginWindow.h"
#include "UI/Theme.h"

namespace sis
{
StudioEditor::StudioEditor (StudioProcessor& p)
    : AudioProcessorEditor (p)
{
    setLookAndFeel (&lookAndFeel);

    /* Die Plugin-Ansicht bekommt man sonst nur in einer DAW zu sehen, und damit wäre sie
       beim Entwickeln praktisch ungeprüft. Mit gesetzter Umgebungsvariable zeigt auch die
       App das Plugin-Fenster – nur zum Ansehen, die Übergabe ans Studio bleibt außen vor. */
    const bool forcePluginView = juce::SystemStats::getEnvironmentVariable ("SIS_PLUGIN_VIEW", {}).isNotEmpty();
    const bool standalone = p.wrapperType == juce::AudioProcessor::wrapperType_Standalone;

    /* Eine exportierte App trägt ihr Instrument mit – wer sie bekommt, will es spielen, nicht
       bearbeiten. Sie zeigt deshalb die Spiel-Oberfläche des Plugins statt des Studios. */
    const bool exportedApp = standalone && p.isPlayingBundledInstrument();

    if (standalone && ! forcePluginView && ! exportedApp)
    {
       #if JUCE_MAC
        constexpr bool ownTitleBar = false;   // macOS behält die native Fensterleiste
       #else
        constexpr bool ownTitleBar = true;
       #endif

        studio = std::make_unique<StudioShell> (p, ownTitleBar);
        addAndMakeVisible (*studio);
        setSize (1440, 900);
    }
    else
    {
        plugin = std::make_unique<PluginShell> (p);

        if (exportedApp)
            plugin->setStandaloneApp (true);

        addAndMakeVisible (*plugin);
        setResizable (true, true);
        setResizeLimits (720, 420, 1600, 1100);
        setSize (960, 520);
    }
}

StudioEditor::~StudioEditor()
{
    // Fenster fremder Plugins gehören zu dieser Oberfläche und dürfen sie nicht überleben
    HostedPluginWindow::closeAll();

    studio.reset();
    plugin.reset();
    setLookAndFeel (nullptr);
}

void StudioEditor::setAudioSettingsCallback (std::function<void()> callback)
{
    if (studio != nullptr)
        studio->onShowAudioSettings = std::move (callback);
    else if (plugin != nullptr)
        plugin->onShowAudioSettings = std::move (callback);
}

void StudioEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void StudioEditor::resized()
{
    if (studio != nullptr)
        studio->setBounds (getLocalBounds());

    if (plugin != nullptr)
        plugin->setBounds (getLocalBounds());
}
} // namespace sis
