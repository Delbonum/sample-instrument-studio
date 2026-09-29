#include "CreditsWindow.h"
#include "../../Version.h"

namespace sis
{
namespace
{
    constexpr int pad = 22;
    constexpr int rowHeight = 22;
    constexpr int labelWidth = 92;
}

void CreditsWindow::show (juce::Component* parent)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Credits";
    options.dialogBackgroundColour = colours::panel;
    options.componentToCentreAround = parent;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.content.setOwned (new CreditsWindow());
    options.content->setSize (360, 250);
    options.launchAsync();
}

CreditsWindow::CreditsWindow()
{
    FlatButton::Style style;
    style.background = colours::white;
    style.border = colours::lineStrongAlt;
    style.text = colours::text;
    style.hoverBackground = colours::rowHover;
    style.fontSize = 12.0f;

    closeButton.setStyle (style);
    closeButton.onClick = [this] { close(); };
    addAndMakeVisible (closeButton);
}

void CreditsWindow::close()
{
    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        dialog->exitModalState (0);
}

void CreditsWindow::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    auto area = getLocalBounds().reduced (pad);

    g.setColour (colours::text);
    g.setFont (sansFont (15.0f, Weight::semibold));
    g.drawText (juce::String (JucePlugin_Name), area.removeFromTop (24),
                juce::Justification::centredLeft, true);

    area.removeFromTop (14);

    g.setColour (colours::textSecondary);
    g.setFont (sansFont (11.5f));
    g.drawFittedText ("Eigene Audiodateien zu einem spielbaren Instrument bauen "
                      "und als VST3 oder App weitergeben."_u,
                      area.removeFromTop (34), juce::Justification::topLeft, 2);

    area.removeFromTop (10);

    const std::pair<juce::String, juce::String> rows[] = {
        { "Entwickler", developerName() },
        { "Version", versionString() },
        { "Schriften", "IBM Plex (SIL Open Font License)" }
    };

    for (const auto& [label, value] : rows)
    {
        auto row = area.removeFromTop (rowHeight);

        g.setColour (colours::textTertiary);
        g.setFont (monoFont (10.5f));
        g.drawText (label, row.removeFromLeft (labelWidth), juce::Justification::centredLeft, true);

        g.setColour (colours::text);
        g.setFont (sansFont (12.5f));
        g.drawText (value, row, juce::Justification::centredLeft, true);
    }
}

void CreditsWindow::resized()
{
    auto area = getLocalBounds().reduced (pad);
    closeButton.setBounds (area.removeFromBottom (26).removeFromRight (100));
}
} // namespace sis
