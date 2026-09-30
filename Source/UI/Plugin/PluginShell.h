#pragma once

#include <JuceHeader.h>

#include "../Mapping/PianoKeyboard.h"
#include "../Mapping/ZoneGrid.h"
#include "../Shell/StudioContext.h"
#include "../Shell/Toast.h"
#include "../Widgets.h"
#include "MacroKnob.h"
#include "ZoneSheet.h"

namespace sis
{
class StudioProcessor;

/** Kompaktes Plugin-Fenster in der DAW.

    Host-Leiste, Kopf mit Preset-Auswahl, Zonenstreifen, vier Makro-Regler, Klaviatur,
    Fußzeile – und per Doppelklick auf eine Zone der Zonen-Dialog.

    Die Ansicht führt einen eigenen `StudioContext`, obwohl es hier weder Menüs noch
    Befehle gibt: der Sample-Editor im Zonen-Dialog ist derselbe wie im Standalone und
    arbeitet gegen den Kontext. Der Verlauf ist der des Prozessors, also zählt ein
    Schnitt im Plugin genauso wie einer im Studio. */
class PluginShell final : public juce::Component, private juce::ChangeListener
{
public:
    explicit PluginShell (StudioProcessor&);
    ~PluginShell() override;

    /** Als exportierte App: keine Host-Leiste und kein Weg ins Studio (der Empfänger hat es
        meist nicht), dafür ein Knopf für die Audio-Einstellungen. */
    void setStandaloneApp (bool);

    /** Nur als App: öffnet den Audio-/MIDI-Dialog. */
    std::function<void()> onShowAudioSettings;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateMacroKnobs();
    void togglePlayback();
    void showPresetMenu();
    void loadPreset (const juce::File&);
    void savePreset();
    void showToast (const juce::String&);

    /** Ordner der Instrument-Presets: Eigene Dokumente / Sample Instrument Studio / Presets. */
    static juce::File presetFolder();

    StudioProcessor& processor;
    UiState ui;
    juce::ApplicationCommandManager commands;   // vor ctx anlegen – ctx hält eine Referenz
    StudioContext ctx;

    juce::Image icon;
    ZoneGrid zones;
    PianoKeyboard keyboard;
    std::array<MacroKnob, InstrumentModel::numMacros> macroKnobs;
    FlatButton studioButton { "IM STUDIO ÖFFNEN ↗"_u }, presetButton, playButton, audioButton { "Audio …"_u };
    bool standaloneApp = false;
    ZoneSheet sheet;
    Toast toast;

    juce::Rectangle<int> hostBar, header, footer, macroArea, zoneLabelArea, macroLabelArea;
    juce::String footerText;
    int auditionNote = -1;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::AlertWindow> nameWindow;
};
} // namespace sis
