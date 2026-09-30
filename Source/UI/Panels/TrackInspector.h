#pragma once

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"
#include "../Widgets.h"

namespace sis
{
/** Eine Zeile der Effektkette: Kippschalter, Name, Parameter. */
class EffectCard final : public juce::Component
{
public:
    EffectCard (StudioContext&, int effectIndex);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    int getIdealHeight() const;
    void updateValues();

private:
    Effect* getEffect() const;

    StudioContext& ctx;
    int index;
    /** Öffnet die Presets der Effektart. */
    void showPresetMenu();
    void askForPresetName();

    bool isCollapsed() const;
    void moveBy (int delta);

    /** Rechtsklick auf einen Regler: welches Makro ihn steuern soll. */
    void showMacroMenu (int parameterIndex);
    MacroTarget makeTarget (int parameterIndex) const;

    FlatButton toggleButton, openButton { "Öffnen"_u }, presetButton { "Presets" },
               removeButton { "×"_u }, moveUpButton, moveDownButton;
    std::vector<std::unique_ptr<ValueBar>> paramBars;
    std::vector<juce::Rectangle<int>> paramLabels, paramValues;
};

/** Linke Spalte in der Editor-Ansicht: Parameter der gewählten Spur. */
class TrackInspectorContent final : public juce::Component, private juce::ChangeListener
{
public:
    explicit TrackInspectorContent (StudioContext&);
    ~TrackInspectorContent() override;

    int getIdealHeight() const;

    /** Werte neu anzeigen – etwa wenn ein anderer Clip gewählt wurde (das ist keine
        Änderung am Modell, sondern an der Auswahl). */
    void refresh() { updateValues(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuildIfNeeded();
    void updateValues();
    void paintEffectTitle (juce::Graphics&) const;
    void showEffectMenu();
    Track* getTrack() const;
    juce::String makeSignature() const;

    /** Ob die gewählte Spur loopt – nur dann gibt es Loop-Beginn und Überblendung. */
    bool isLooping() const;

    StudioContext& ctx;

    ValueBar gainBar, panBar, centsBar, loopStartBar, crossfadeBar;
    FlatButton pitchDownButton { "–"_u }, pitchUpButton { "+" };
    FlatButton resetStretchButton { "1:1" }, reverseButton { "Umkehren" }, addEffectButton { "+" };
    FlatButton keepTempoButton { "Tempo beim Transponieren halten" };
    juce::ComboBox algorithmBox, loopBox;
    std::vector<std::unique_ptr<EffectCard>> effectCards;

    juce::String signature;
    juce::Rectangle<int> headerArea, levelSection, pitchSection, effectSection;
    juce::Rectangle<int> gainLabel, gainValue, panLabel, panValue, fadeLabel, fadeValue;
    juce::Rectangle<int> pitchValueArea, centsLabel, centsValue, stretchLabel, stretchValue;
    juce::Rectangle<int> pitchTitleArea, effectTitleArea, effectNote;
    juce::Rectangle<int> loopStartLabel, loopStartValue, crossfadeLabel, crossfadeValue;
};

/** Rahmen mit Bildlauf. */
class TrackInspector final : public juce::Component, private juce::ChangeListener
{
public:
    explicit TrackInspector (StudioContext&);
    ~TrackInspector() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /* Klappt ein Abschnitt zu, ändert sich die Höhe des Inhalts, nicht die des Rahmens.
       JUCE ruft `resized` aber nur bei geänderten Maßen - also selbst zuhören. */
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    StudioContext& ctx;
    TrackInspectorContent content;
    juce::Viewport viewport;
};
} // namespace sis
