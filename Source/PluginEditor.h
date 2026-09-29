#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "UI/Plugin/PluginShell.h"
#include "UI/Shell/StudioShell.h"
#include "UI/StudioLookAndFeel.h"

namespace sis
{
/** Wählt je nach Laufform die Oberfläche: volles Studio (Standalone) oder kompaktes Plugin-Fenster. */
class StudioEditor final : public juce::AudioProcessorEditor
{
public:
    explicit StudioEditor (StudioProcessor&);
    ~StudioEditor() override;

    /** Nur Standalone: Callback für "Datei → Audio-Einstellungen …". */
    void setAudioSettingsCallback (std::function<void()>);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    StudioLookAndFeel lookAndFeel;   // muss die Kind-Komponenten überleben
    std::unique_ptr<StudioShell> studio;
    std::unique_ptr<PluginShell> plugin;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioEditor)
};
} // namespace sis
