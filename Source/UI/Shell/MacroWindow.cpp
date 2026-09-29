#include "MacroWindow.h"
#include "../../PluginProcessor.h"

namespace sis
{
namespace
{
    constexpr int pad = 20;
    constexpr int rowHeight = 40;
}

void MacroWindow::show (StudioContext& ctx, juce::Component* parent)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Makros zuweisen"_u;
    options.dialogBackgroundColour = colours::panel;
    options.componentToCentreAround = parent;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.content.setOwned (new MacroWindow (ctx));
    options.content->setSize (460, 2 * pad + 46 + InstrumentModel::numMacros * rowHeight + 40);
    options.launchAsync();
}

MacroWindow::MacroWindow (StudioContext& c) : ctx (c)
{
    FlatButton::Style style;
    style.background = colours::white;
    style.border = colours::lineStrongAlt;
    style.text = colours::text;
    style.hoverBackground = colours::rowHover;
    style.fontSize = 12.0f;

    closeButton.setStyle (style);
    closeButton.onClick = [this]
    {
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState (0);
    };

    addAndMakeVisible (closeButton);
    ctx.model.addChangeListener (this);
}

MacroWindow::~MacroWindow()
{
    ctx.model.removeChangeListener (this);
}

void MacroWindow::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

juce::String MacroWindow::describe (const MacroTarget& target) const
{
    const auto* zone = const_cast<InstrumentModel&> (ctx.model).findZone (target.zoneId);

    if (zone == nullptr || ! juce::isPositiveAndBelow (target.trackIndex, (int) zone->tracks.size()))
        return "(verwaist)"_u;

    const auto& track = zone->tracks[(size_t) target.trackIndex];

    if (! juce::isPositiveAndBelow (target.effectIndex, (int) track.effects.size()))
        return "(verwaist)"_u;

    const auto& effect = track.effects[(size_t) target.effectIndex];

    if (! juce::isPositiveAndBelow (target.parameterIndex, (int) effect.parameters.size()))
        return "(verwaist)"_u;

    return track.name + " · "_u + effect.name + " · "_u
           + effect.parameters[(size_t) target.parameterIndex].label;
}

void MacroWindow::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    auto area = getLocalBounds().reduced (pad);

    g.setColour (colours::textSecondary);
    g.setFont (sansFont (11.5f));
    g.drawFittedText ("Zuweisen mit der rechten Maustaste auf einem Regler in der Effektkette. "
                      "Hier klicken löst eine Zuweisung wieder."_u,
                      area.removeFromTop (38), juce::Justification::topLeft, 2);

    area.removeFromTop (8);

    for (int macro = 0; macro < InstrumentModel::numMacros; ++macro)
    {
        const auto row = rowAreas[(size_t) macro];
        const auto& targets = ctx.model.macroTargets[(size_t) macro];

        g.setColour (targets.empty() ? colours::surface : colours::accentSoft);
        g.fillRect (row);
        g.setColour (targets.empty() ? colours::line : colours::accent);
        g.drawRect (row, 1);

        auto inner = row.reduced (10, 5);

        g.setColour (colours::text);
        g.setFont (sansFont (12.0f, Weight::medium));
        g.drawText (macroName (macro), inner.removeFromTop (15), juce::Justification::centredLeft, true);

        g.setColour (colours::textTertiary);
        g.setFont (monoFont (10.0f));

        juce::String text = "nicht zugewiesen"_u;

        if (! targets.empty())
        {
            juce::StringArray parts;

            for (const auto& target : targets)
                parts.add (describe (target));

            text = parts.joinIntoString (" · "_u);
        }

        g.drawText (text, inner, juce::Justification::centredLeft, true);
    }
}

void MacroWindow::resized()
{
    auto area = getLocalBounds().reduced (pad);
    area.removeFromTop (38 + 8);

    for (auto& row : rowAreas)
        row = area.removeFromTop (rowHeight).reduced (0, 2);

    closeButton.setBounds (getLocalBounds().reduced (pad).removeFromBottom (26).removeFromRight (100));
}

void MacroWindow::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;

    for (int macro = 0; macro < InstrumentModel::numMacros; ++macro)
    {
        if (! rowAreas[(size_t) macro].contains (e.getPosition()))
            continue;

        if (ctx.model.macroTargets[(size_t) macro].empty())
            return;

        ctx.step (macroName (macro) + " gelöst"_u);
        ctx.model.macroTargets[(size_t) macro].clear();
        ctx.model.notifyChanged();
        ctx.toast (macroName (macro) + " folgt jetzt nichts mehr"_u);
        return;
    }
}
} // namespace sis
