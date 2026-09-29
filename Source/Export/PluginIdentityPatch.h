#pragma once

#include <JuceHeader.h>

namespace sis
{
/** Gibt dem kopierten VST3 einen eigenen Namen und eine eigene Kennung.

    Möglich wird das, weil das Plugin beides zur Laufzeit aus einem Datenblock im Binary
    liest (Source/PluginIdentity.*). Hier wird dieser Block ersetzt, dazu passend die
    Klassen-Kennungen in `moduleinfo.json` und der Dateiname des Moduls.

    Damit erscheint jedes exportierte Instrument in der DAW als eigenes Plugin. */
namespace identity
{
    /** Marke, an der der Datenblock im Binary erkannt wird. */
    inline constexpr const char* marker = "SIS-IDENTITY-v1|";

    /** Größe des Blocks im Plugin – muss zu PluginIdentity.cpp passen. */
    inline constexpr int blockSize = 192;

    /** Vierstelliger Plugin-Code, aus dem Namen abgeleitet: gleicher Name → gleiche Kennung. */
    juce::String makePluginCode (const juce::String& instrumentName);

    /** Ersetzt den Datenblock in einer Programmdatei – Plugin-Modul wie Standalone-App.
        Geschrieben wird an Ort und Stelle, nicht über eine Zwischendatei. */
    bool patchBinary (const juce::File& binary, const juce::String& instrumentName,
                      const juce::String& pluginCode, juce::String& error);

    /** Ersetzt Name und Kennung im Bundle. Gibt bei Misserfolg false zurück und füllt `error`. */
    bool patchBundle (const juce::File& bundle, const juce::String& instrumentName,
                      const juce::String& pluginCode, juce::String& error);
} // namespace identity
} // namespace sis
