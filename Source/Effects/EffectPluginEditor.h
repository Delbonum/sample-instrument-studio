#pragma once

#include <JuceHeader.h>

#include "../UI/StudioLookAndFeel.h"
#include "../UI/Widgets.h"
#include "EffectPlugin.h"

namespace sis
{
/** Fenster eines eigenständigen Effekt-Plugins.

    Absichtlich dieselbe Gestalt wie die Effektkarte im Studio: Kopf mit Name und
    Umgehen-Schalter, eine Zeile je Regler, darüber die Preset-Leiste. Wer den Effekt im
    Studio kennt, muss hier nichts Neues lernen.

    Die Oberfläche ist für **alle** Effekte dieselbe; sie liest die Beschriftungen aus dem
    Prototyp. Ein eigener Entwurf je Effekt wäre einundzwanzigmal dieselbe Arbeit — und
    einundzwanzig Gelegenheiten, dass einer davon veraltet. */
class EffectPluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EffectPluginEditor (EffectPlugin&);
    ~EffectPluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        juce::String label;
        ValueBar bar;
        juce::Rectangle<int> labelArea, valueArea;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        juce::RangedAudioParameter* parameter = nullptr;
    };

    void timerCallback() override;
    void refreshPresetList();
    void showPresetMenu();
    void askForPresetName();

    EffectPlugin& processor;
    StudioLookAndFeel lookAndFeel;

    FlatButton presetButton { "Presets" }, bypassButton { "UMGEHEN"_u };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    std::vector<std::unique_ptr<Row>> rows;
    juce::StringArray presetNames;

    juce::Rectangle<int> headerArea, presetArea, meterArea;
    std::unique_ptr<juce::AlertWindow> nameWindow;
    float lastLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectPluginEditor)
};
} // namespace sis
