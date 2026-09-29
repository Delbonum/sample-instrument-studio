/*
    Name und VST3-Kennung des Plugins - zur Laufzeit gelesen, nicht einkompiliert.

    Beide Werte stehen in einem Datenblock im Binary (siehe PluginIdentity.cpp), den der
    Export beim Kopieren ersetzt. So bekommt jedes exportierte Instrument eine eigene
    Kennung und einen eigenen Namen in der DAW, ohne dass etwas neu gebaut werden muss.

    Diese Datei wird allen Übersetzungseinheiten vorangestellt (/FI bzw. -include), weil
    JucePlugin_Name und JucePlugin_PluginCode auch in JUCEs eigenen Quellen stehen.
    Deshalb: reines C, keine Includes.
*/

#ifndef SIS_PLUGIN_IDENTITY_H
#define SIS_PLUGIN_IDENTITY_H

#ifdef __cplusplus
extern "C" {
#endif

/** Anzeigename des Plugins, z. B. "Nocturne Hybrid Bass". */
const char* sisExportedPluginName (void);

/** Vierstelliger Plugin-Code als 32-Bit-Wert, z. B. 'Sist'. Bildet die VST3-Kennung. */
unsigned int sisExportedPluginCode (void);

#ifdef __cplusplus
}
#endif

/*  Diese beiden Makros setzt CMake als JucePlugin_Name bzw. JucePlugin_PluginCode ein.
    Auf der Kommandozeile stehen Rückfallwerte (Zeichenkette und Zahl) - der Manifest-Helfer
    von JUCE sieht nur die; alles, was unsere Quellen mitlinkt, bekommt die Laufzeitwerte. */
#undef SIS_PLUGIN_NAME
#undef SIS_PLUGIN_CODE
#define SIS_PLUGIN_NAME sisExportedPluginName()
#define SIS_PLUGIN_CODE sisExportedPluginCode()

#endif
