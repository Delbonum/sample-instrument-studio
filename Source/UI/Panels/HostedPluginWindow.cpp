#include "HostedPluginWindow.h"
#include "../Theme.h"

namespace sis
{
namespace
{
    // Die offenen Fenster; nur vom Message-Thread benutzt.
    juce::Array<HostedPluginWindow*>& openWindows()
    {
        static juce::Array<HostedPluginWindow*> windows;
        return windows;
    }
}

//==============================================================================
void HostedPluginWindow::open (HostedPlugin::Ptr plugin, const juce::String& title)
{
    if (plugin == nullptr || plugin->getInstance() == nullptr)
        return;

    for (auto* window : openWindows())
    {
        if (window->plugin == plugin)
        {
            window->toFront (true);
            return;
        }
    }

    auto* window = new HostedPluginWindow (std::move (plugin), title);
    openWindows().add (window);
    window->setVisible (true);
    window->toFront (true);
}

void HostedPluginWindow::closeAll()
{
    // Rückwärts, weil sich jedes Fenster beim Löschen selbst austrägt
    const auto windows = openWindows();

    for (int i = windows.size(); --i >= 0;)
        delete windows[i];
}

void HostedPluginWindow::closeFor (const HostedPlugin* plugin)
{
    const auto windows = openWindows();

    for (int i = windows.size(); --i >= 0;)
        if (windows[i]->plugin.get() == plugin)
            delete windows[i];
}

//==============================================================================
HostedPluginWindow::HostedPluginWindow (HostedPlugin::Ptr p, const juce::String& title)
    : juce::DocumentWindow (title, colours::panel, juce::DocumentWindow::closeButton, true),
      plugin (std::move (p))
{
    auto* instance = plugin->getInstance();

    // Plugins ohne eigene Oberfläche bekommen JUCEs allgemeine Reglerliste
    auto* editor = instance->hasEditor() ? instance->createEditorAndMakeActive()
                                         : new juce::GenericAudioProcessorEditor (*instance);

    if (editor == nullptr)
        editor = new juce::GenericAudioProcessorEditor (*instance);

    setUsingNativeTitleBar (true);
    setContentOwned (editor, true);
    setResizable (editor->isResizable(), false);
    centreWithSize (getWidth(), getHeight());
}

HostedPluginWindow::~HostedPluginWindow()
{
    openWindows().removeFirstMatchingValue (this);

    // Löscht den Editor; der meldet sich in seinem Destruktor beim Plugin ab.
    clearContentComponent();
}

void HostedPluginWindow::closeButtonPressed()
{
    delete this;
}
} // namespace sis
