#pragma once

#include <JuceHeader.h>

namespace sis
{
/** Der Weg vom Plugin zurück ins Studio.

    Aus dem Plugin heraus lässt sich die Standalone-App nicht einfach finden: die
    eigene Programmdatei ist die der DAW, und das Plugin liegt im VST3-Ordner, die App
    irgendwo anders. Geraten wird deshalb nicht – die **App merkt sich bei jedem Start
    ihren eigenen Pfad** in der gemeinsamen Einstellungsdatei, und das Plugin liest ihn
    dort. Wer die App noch nie gestartet hat, bekommt einen Hinweis statt eines
    Fehlversuchs.

    Übergeben wird der aktuelle Stand als ganz normale `.sisp`-Datei im Temp-Ordner,
    die der App als Befehlszeilenargument mitgegeben wird. Ein Sonderformat lohnt nicht:
    was das Plugin kann, kann die Projektdatei auch. */
namespace handover
{
    /** Einstellungen von App und Plugin – dieselbe Datei wie in `StandaloneApp`. */
    juce::PropertiesFile::Options settingsOptions();

    /** Von der App beim Start aufzurufen. */
    void rememberStudioExecutable();

    /** Die gemerkte Programmdatei, oder eine leere Datei, wenn es sie (noch) nicht gibt. */
    juce::File findStudioExecutable();

    /** Datei, über die Plugin und App den Stand austauschen. */
    juce::File transferFile();
} // namespace handover
} // namespace sis
