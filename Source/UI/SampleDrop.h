#pragma once

#include <JuceHeader.h>

#include "Shell/StudioContext.h"

namespace sis
{
/** Ein Sample aus dem Browser in die Mitte ziehen – was dabei für alle Ziele gleich ist.

    Gezogen wird nur die Nummer in `model.samples`, nicht die Datei: das Ziel soll dieselbe
    Spur anlegen wie der Doppelklick im Browser, samt Länge und Wellenform. */
namespace sampleDrop
{
    /** Beschreibung für `DragAndDropContainer::startDragging`. */
    juce::var describe (int sampleIndex);

    /** Nummer des gezogenen Samples, -1 wenn etwas anderes gezogen wird. */
    int indexOf (const juce::DragAndDropTarget::SourceDetails&);

    /** Das Sample, falls es sich zuordnen lässt; sonst Hinweis und nullptr. Beispiel-Einträge
        ohne Datei gehen nicht – sie würden stumm bleiben. */
    const SampleFile* resolve (StudioContext&, int sampleIndex);

    /** Legt das Sample als Spur in die Zone (eine leere Spur wird gefüllt) und meldet es.
        Die Ansicht bleibt, wo sie ist. */
    void intoZone (StudioContext&, Zone&, int sampleIndex);

    /** Taste im Tonhöhen-Instrument: in die Zone auf Taste und Velocity, sonst in eine neue
        Zone um die Taste herum (`velocity < 0`: gleich welche Velocity – so von der
        Klaviatur). Im Drumset: in das Kit-Teil auf dieser Taste. */
    void ontoNote (StudioContext&, int note, int velocity, int sampleIndex);

    /** Ins Kit-Teil – dessen Zone entsteht dabei, falls es sie noch nicht gibt. */
    void intoDrumPart (StudioContext&, const DrumPart&, int sampleIndex);
} // namespace sampleDrop
} // namespace sis
