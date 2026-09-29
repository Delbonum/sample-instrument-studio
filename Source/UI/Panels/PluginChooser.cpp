#include "PluginChooser.h"

namespace sis
{
namespace
{
    constexpr int pad = 16;
    constexpr int buttonHeight = 26;
    constexpr int rowHeight = 34;
    constexpr int statusHeight = 20;

    FlatButton::Style plainStyle()
    {
        FlatButton::Style style;
        style.background = colours::white;
        style.border = colours::lineStrongAlt;
        style.text = colours::text;
        style.hoverBackground = colours::rowHover;
        style.fontSize = 12.0f;
        return style;
    }

    FlatButton::Style accentStyle()
    {
        FlatButton::Style style;
        style.background = colours::accent;
        style.border = colours::accentDark;
        style.text = colours::white;
        style.hoverBackground = colours::accentHover;
        style.fontSize = 12.0f;
        style.weight = Weight::medium;
        return style;
    }
}

//==============================================================================
void PluginChooser::show (PluginLibrary& library, juce::Component* parent, ChosenCallback onChosen)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Externes Plugin wählen"_u;
    options.dialogBackgroundColour = colours::panel;
    options.componentToCentreAround = parent;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.content.setOwned (new PluginChooser (library, std::move (onChosen)));
    options.content->setSize (460, 400);
    options.launchAsync();
}

PluginChooser::PluginChooser (PluginLibrary& l, ChosenCallback callback)
    : library (l), onChosen (std::move (callback))
{
    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, colours::white);
    list.setColour (juce::ListBox::outlineColourId, colours::lineStrongAlt);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    scanButton.setStyle (plainStyle());
    fileButton.setStyle (plainStyle());
    cancelButton.setStyle (plainStyle());
    chooseButton.setStyle (accentStyle());

    scanButton.onClick   = [this] { startScan(); };
    fileButton.onClick   = [this] { addFileByHand(); };
    cancelButton.onClick = [this] { closeWindow(); };
    chooseButton.onClick = [this] { chooseSelected(); };

    for (auto* button : { &scanButton, &fileButton, &chooseButton, &cancelButton })
        addAndMakeVisible (button);

    library.addChangeListener (this);
    refreshList();

    // Beim ersten Mal ist die Liste leer - dann gleich lossuchen
    if (entries.isEmpty())
        startScan();
}

PluginChooser::~PluginChooser()
{
    library.removeChangeListener (this);
}

//==============================================================================
void PluginChooser::refreshList()
{
    const auto selected = juce::isPositiveAndBelow (list.getSelectedRow(), entries.size())
                              ? entries[list.getSelectedRow()].createIdentifierString()
                              : juce::String();

    entries = library.getEffects();
    list.updateContent();

    // Die vorherige Auswahl nach einem Suchlauf nicht verlieren
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].createIdentifierString() == selected)
            list.selectRow (i);

    if (! library.isScanning())
        statusText = entries.isEmpty() ? "Noch keine Effekte gefunden."_u
                                       : juce::String (entries.size()) + " Effekte gefunden."_u;

    chooseButton.setEnabled (list.getSelectedRow() >= 0);
    repaint();
}

void PluginChooser::startScan()
{
    if (library.isScanning())
        return;

    statusText = "Suche läuft …"_u;
    scanButton.setEnabled (false);
    startTimerHz (8);
    repaint();

    library.startScan ([this] (int found)
    {
        statusText = found > 0 ? juce::String (found) + " Plugins geprüft."_u
                               : "Nichts gefunden."_u;
        scanButton.setEnabled (true);
        stopTimer();
        refreshList();
    });
}

void PluginChooser::addFileByHand()
{
    fileChooser = std::make_unique<juce::FileChooser> ("VST3-Plugin wählen"_u,
                                                       juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory),
                                                       "*.vst3");

    // Ein .vst3 ist unter Windows ein Ordner - beides zulassen
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (file == juce::File())
            return;

        juce::String error;
        const auto added = library.addPluginFile (file, error);

        statusText = added.isEmpty() ? error
                                     : file.getFileNameWithoutExtension() + " aufgenommen."_u;
        refreshList();
    });
}

void PluginChooser::chooseSelected()
{
    const int row = list.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, entries.size()))
        return;

    const auto description = entries[row];

    if (onChosen != nullptr)
        onChosen (description.name, description.createIdentifierString());

    closeWindow();
}

void PluginChooser::closeWindow()
{
    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        dialog->exitModalState (0);
}

//==============================================================================
void PluginChooser::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshList();
}

void PluginChooser::timerCallback()
{
    // Während der Suche zeigen, woran gerade gearbeitet wird
    const auto current = library.getCurrentScanName();

    if (current.isNotEmpty())
    {
        statusText = "Prüfe "_u + juce::File (current).getFileNameWithoutExtension() + " …"_u;
        repaint();
    }
}

//==============================================================================
int PluginChooser::getNumRows()
{
    return entries.size();
}

void PluginChooser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool isSelected)
{
    if (! juce::isPositiveAndBelow (row, entries.size()))
        return;

    const auto& description = entries[row];

    g.setColour (isSelected ? colours::accentSoft : colours::white);
    g.fillRect (0, 0, width, height);

    g.setColour (colours::lineFine);
    g.fillRect (0, height - 1, width, 1);

    g.setColour (isSelected ? colours::accentDark : colours::text);
    g.setFont (sansFont (12.5f, Weight::medium));
    g.drawText (description.name, juce::Rectangle<int> (10, 4, width - 20, 15),
                juce::Justification::centredLeft, true);

    g.setColour (colours::textTertiary);
    g.setFont (sansFont (10.5f));
    g.drawText (description.manufacturerName + " · "_u + description.pluginFormatName,
                juce::Rectangle<int> (10, height - 18, width - 20, 13),
                juce::Justification::centredLeft, true);
}

void PluginChooser::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    chooseSelected();
}

void PluginChooser::selectedRowsChanged (int lastRowSelected)
{
    chooseButton.setEnabled (lastRowSelected >= 0);
}

//==============================================================================
void PluginChooser::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    g.setColour (colours::textSecondary);
    g.setFont (sansFont (11.0f));
    g.drawText (statusText,
                juce::Rectangle<int> (pad, getHeight() - pad - buttonHeight - 6 - statusHeight,
                                      getWidth() - 2 * pad, statusHeight),
                juce::Justification::centredLeft, true);
}

void PluginChooser::resized()
{
    auto area = getLocalBounds().reduced (pad);

    auto top = area.removeFromTop (buttonHeight);
    scanButton.setBounds (top.removeFromLeft (150));
    top.removeFromLeft (8);
    fileButton.setBounds (top.removeFromLeft (120));

    area.removeFromTop (12);

    auto bottom = area.removeFromBottom (buttonHeight);
    chooseButton.setBounds (bottom.removeFromRight (100));
    bottom.removeFromRight (8);
    cancelButton.setBounds (bottom.removeFromRight (100));

    area.removeFromBottom (6 + statusHeight);
    list.setBounds (area);
}
} // namespace sis
