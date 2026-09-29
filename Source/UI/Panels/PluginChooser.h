#pragma once

#include "../../Audio/PluginLibrary.h"
#include "../Theme.h"
#include "../Widgets.h"

namespace sis
{
/** Liste der installierten fremden Effekte, zum Aussuchen.

    Zeigt, was die Plugin-Liste kennt, und bietet zwei Wege, sie zu füllen: die
    Standard-Ordner durchsuchen oder eine einzelne .vst3-Datei von Hand wählen.
    Wird über `show` als eigenes Fenster geöffnet. */
class PluginChooser final : public juce::Component,
                            private juce::ListBoxModel,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    /** `onChosen` bekommt Anzeigename und Kennung des gewählten Plugins. */
    using ChosenCallback = std::function<void (const juce::String& name, const juce::String& identifier)>;

    /** Öffnet den Dialog. Wird abgebrochen, kommt kein Rückruf. */
    static void show (PluginLibrary&, juce::Component* parent, ChosenCallback);

    PluginChooser (PluginLibrary&, ChosenCallback);
    ~PluginChooser() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool isSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void refreshList();
    void chooseSelected();
    void startScan();
    void addFileByHand();
    void closeWindow();

    PluginLibrary& library;
    ChosenCallback onChosen;

    juce::Array<juce::PluginDescription> entries;
    juce::ListBox list { "Plugins", this };

    FlatButton scanButton { "Ordner durchsuchen"_u };
    FlatButton fileButton { "Datei wählen …"_u };
    FlatButton chooseButton { "Einfügen"_u };
    FlatButton cancelButton { "Abbrechen"_u };

    juce::String statusText;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginChooser)
};
} // namespace sis
