#pragma once

#include <JuceHeader.h>

namespace sis
{
/** Schreibt einen XML-Baum **unmittelbar** in die Datei.

    `juce::XmlElement::writeTo (File)` legt dafür eine Zwischendatei an und benennt sie
    um. Unter Windows scheitert genau das immer wieder am Virenscanner, der die frisch
    geschriebene Datei noch einen Moment festhält – und zwar sporadisch, was es besonders
    unangenehm macht: der Aufrufer sieht keinen Fehler, die Datei behält nur still ihren
    alten Inhalt.

    Dieselbe Falle hat in diesem Projekt schon den Kennungs-Patch der Plugin-Datei, die
    `moduleinfo.json` und das Verschieben eines Ordners im Test erwischt. Deshalb hier an
    einer Stelle benannt statt viermal einzeln umgangen.

    Gibt false zurück, wenn die Datei nicht geschrieben werden konnte. */
inline bool writeXmlDirectly (const juce::XmlElement& element, const juce::File& file)
{
    file.getParentDirectory().createDirectory();

    juce::FileOutputStream stream (file);

    if (! stream.openedOk()
        || ! stream.setPosition (0)
        || ! stream.truncate().wasOk()
        || ! stream.writeText (element.toString(), false, false, nullptr))
        return false;

    stream.flush();
    return ! stream.getStatus().failed();
}
} // namespace sis
