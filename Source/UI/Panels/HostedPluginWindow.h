#pragma once

#include "../../Audio/PluginLibrary.h"

namespace sis
{
/** Fenster um die eigene Oberfläche eines fremden Plugins.

    Jedes Plugin bekommt höchstens ein Fenster; ein zweiter Aufruf holt das
    vorhandene nach vorn. Die Fenster schließen sich selbst, wenn das Plugin
    verschwindet, und werden beim Beenden über `closeAll` aufgeräumt. */
class HostedPluginWindow final : public juce::DocumentWindow
{
public:
    /** Öffnet die Oberfläche oder holt ein offenes Fenster nach vorn.
        Plugins ohne eigene Oberfläche bekommen die allgemeine Reglerliste. */
    static void open (HostedPlugin::Ptr, const juce::String& title);

    /** Schließt alle offenen Plugin-Fenster (beim Beenden des Programms). */
    static void closeAll();

    /** Schließt das Fenster zu genau diesem Plugin, etwa wenn der Effekt gelöscht wird. */
    static void closeFor (const HostedPlugin*);

    ~HostedPluginWindow() override;

    void closeButtonPressed() override;

private:
    HostedPluginWindow (HostedPlugin::Ptr, const juce::String& title);

    HostedPlugin::Ptr plugin;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedPluginWindow)
};
} // namespace sis
