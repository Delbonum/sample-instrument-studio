#include "EffectPluginEditor.h"

namespace sis
{
namespace
{
    constexpr int pad = 16;
    constexpr int headerHeight = 40;
    constexpr int presetHeight = 30;
    constexpr int rowHeight = 30;
    constexpr int labelWidth = 118;
    constexpr int valueWidth = 54;
    constexpr int meterHeight = 22;
}

EffectPluginEditor::EffectPluginEditor (EffectPlugin& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    FlatButton::Style field;
    field.background = colours::surface;
    field.border = colours::lineStrongAlt;
    field.text = colours::text;
    field.hoverBackground = colours::rowHover;
    field.fontSize = 11.5f;

    presetButton.setStyle (field);
    presetButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetButton);

    FlatButton::Style toggle = field;
    toggle.fontSize = 10.0f;
    toggle.mono = true;
    bypassButton.setStyle (toggle);
    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.getState(), "bypass", bypassButton);

    const auto prototype = EffectPlugin::makePrototype();

    for (int i = 0; i < (int) prototype.parameters.size(); ++i)
    {
        auto row = std::make_unique<Row>();
        row->label = prototype.parameters[(size_t) i].label;
        row->parameter = processor.getState().getParameter (EffectPlugin::parameterId (i));

        if (row->parameter == nullptr)
            continue;

        row->bar.setFillColour (colours::accent);
        addAndMakeVisible (row->bar);

        auto* raw = row.get();
        raw->attachment = std::make_unique<juce::ParameterAttachment> (
            *raw->parameter,
            [this, raw] (float newValue)
            {
                raw->bar.setValue (raw->parameter->convertTo0to1 (newValue));
                repaint (raw->valueArea);
            },
            nullptr);

        raw->bar.onDragStart = [raw] { raw->attachment->beginGesture(); };
        raw->bar.onDragEnd = [raw] { raw->attachment->endGesture(); };
        raw->bar.onValueChange = [raw] (float v)
        {
            raw->attachment->setValueAsPartOfGesture (raw->parameter->convertFrom0to1 (v));
        };

        raw->attachment->sendInitialUpdate();
        rows.push_back (std::move (row));
    }

    refreshPresetList();

    setSize (360, pad + headerHeight + presetHeight + 8
                      + (int) rows.size() * rowHeight + 10 + meterHeight + pad);

    startTimerHz (20);
}

EffectPluginEditor::~EffectPluginEditor()
{
    setLookAndFeel (nullptr);
}

void EffectPluginEditor::refreshPresetList()
{
    presetNames = processor.getPresetNames();
}

void EffectPluginEditor::showPresetMenu()
{
    refreshPresetList();

    juce::PopupMenu menu;

    for (int i = 0; i < presetNames.size(); ++i)
        menu.addItem (i + 1, presetNames[i]);

    if (presetNames.isEmpty())
        menu.addItem (-1, "Noch keine Presets"_u, false, false);

    menu.addSeparator();
    menu.addItem (2000, "Sichern unter …"_u);

    juce::PopupMenu remove;
    for (int i = 0; i < presetNames.size(); ++i)
        remove.addItem (3000 + i, presetNames[i]);

    menu.addSubMenu ("Löschen"_u, remove, ! presetNames.isEmpty());
    menu.addSeparator();
    menu.addItem (2001, "Grundstellung");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton),
                        [this] (int result)
    {
        if (result == 0)
            return;

        if (result == 2000)
            askForPresetName();
        else if (result == 2001)
            processor.resetToFactoryDefaults();
        else if (result >= 3000 && result - 3000 < presetNames.size())
            processor.deletePreset (presetNames[result - 3000]);
        else if (result > 0 && result <= presetNames.size())
            processor.loadPreset (presetNames[result - 1]);

        refreshPresetList();
    });
}

void EffectPluginEditor::askForPresetName()
{
    nameWindow = std::make_unique<juce::AlertWindow> ("Preset sichern", "Name:",
                                                      juce::MessageBoxIconType::NoIcon, this);
    nameWindow->addTextEditor ("name", {});
    nameWindow->addButton ("Sichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameWindow->addButton ("Abbrechen"_u, 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<EffectPluginEditor> safe (this);
    nameWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr)
            return;

        const auto name = safe->nameWindow->getTextEditorContents ("name").trim();
        safe->nameWindow = nullptr;

        if (result == 0 || name.isEmpty())
            return;

        safe->processor.savePreset (name);
        safe->refreshPresetList();
    }), false);
}

void EffectPluginEditor::timerCallback()
{
    const float level = processor.getOutputLevel();

    if (std::abs (level - lastLevel) > 0.002f)
    {
        lastLevel = level;
        repaint (meterArea);
    }
}

void EffectPluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    // Kopf: Name des Effekts
    g.setColour (colours::bars);
    g.fillRect (headerArea);
    draw::bottomLine (g, headerArea, colours::line);

    auto header = headerArea.reduced (pad, 0).withTrimmedRight (bypassButton.getWidth() + 10);
    g.setColour (colours::text);
    g.setFont (sansFont (13.5f, Weight::semibold));
    g.drawText (EffectPlugin::makePrototype().name, header, juce::Justification::centredLeft, true);

    // Regler
    for (const auto& row : rows)
    {
        g.setColour (colours::textSecondary);
        g.setFont (monoFont (10.0f));
        g.drawText (row->label, row->labelArea, juce::Justification::centredLeft, true);

        g.setColour (colours::text);
        g.drawText (juce::String (juce::roundToInt (row->bar.getValue() * 100.0f)) + " %",
                    row->valueArea, juce::Justification::centredRight, false);
    }

    // Aussteuerung
    draw::levelMeter (g, meterArea.toFloat(), lastLevel);
}

void EffectPluginEditor::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (headerHeight);

    {
        auto h = headerArea.reduced (pad, 0);
        const int width = bypassButton.getTextWidth() + 18;
        bypassButton.setBounds (h.removeFromRight (width).withSizeKeepingCentre (width, 22));
    }

    area.reduce (pad, 0);
    area.removeFromTop (10);

    presetArea = area.removeFromTop (presetHeight);
    presetButton.setBounds (presetArea.withSizeKeepingCentre (presetArea.getWidth(), 26));
    area.removeFromTop (8);

    for (auto& row : rows)
    {
        auto line = area.removeFromTop (rowHeight);
        row->labelArea = line.removeFromLeft (labelWidth);
        row->valueArea = line.removeFromRight (valueWidth);
        line.removeFromLeft (6);
        line.removeFromRight (6);
        row->bar.setBounds (line.withSizeKeepingCentre (line.getWidth(), 18));
    }

    area.removeFromTop (10);
    meterArea = area.removeFromTop (meterHeight).withSizeKeepingCentre (area.getWidth(), 8);
}
} // namespace sis
