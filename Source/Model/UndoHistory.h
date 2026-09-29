#pragma once

#include <JuceHeader.h>
#include <vector>

#include "Instrument.h"

namespace sis
{
/** Verlauf der Änderungen am Instrument.

    Arbeitet mit Schnappschüssen des ganzen Modells. Das passt hier, weil sich das
    Instrument ohnehin vollständig als Baum schreiben lässt und nur wenige Kilobyte
    groß ist — die Audiodaten stehen als Pfad darin, nicht als Inhalt.

    Der Verlauf hängt am Änderungssignal des Modells, nicht an einzelnen Aufrufern.
    Damit ist **jede** Änderung erfasst, auch eine, die niemand angemeldet hat; wer
    vorher `nameNextStep` aufruft, bekommt nur einen besseren Namen im Menü.
    Änderungen, die schnell aufeinander folgen, werden zu einem Schritt zusammengefasst,
    damit ein Reglerzug nicht hundert Schritte hinterlässt.

    Was ein fremdes Plugin intern an seinen Reglern ändert, steht nicht im Modell und
    löst kein Änderungssignal aus — das lässt sich folglich auch nicht zurücknehmen. */
class UndoHistory : private juce::Timer
{
public:
    static constexpr int maxSteps = 60;
    static constexpr int coalesceMilliseconds = 500;

    UndoHistory (InstrumentModel&, juce::AudioFormatManager&);
    ~UndoHistory() override;

    /** Verwirft den Verlauf und nimmt den aktuellen Stand als Ausgangspunkt.
        Beim Anlegen, Laden und Öffnen eines Instruments aufrufen. */
    void reset();

    /** Benennt den nächsten Schritt; **vor** der Änderung aufrufen.
        Ein anderer Name schließt einen noch offenen Schritt ab. */
    void nameNextStep (const juce::String&);

    /** Vom Änderungssignal des Modells. */
    void modelChanged();

    /** Hält eine noch offene Änderung sofort fest (vor dem Speichern oder Exportieren). */
    void flush();

    bool canUndo() const noexcept   { return current > 0; }
    bool canRedo() const noexcept   { return current + 1 < (int) steps.size(); }

    /** Name des Schritts, den `undo` bzw. `redo` ausführen würde; sonst leer. */
    juce::String getUndoName() const;
    juce::String getRedoName() const;

    bool undo();
    bool redo();

    int getNumSteps() const noexcept { return (int) steps.size(); }

    /** Meldet sich, wenn sich der Verlauf geändert hat (Menübeschriftungen). */
    std::function<void()> onChanged;

private:
    void timerCallback() override;
    void record();
    void restore (const juce::ValueTree&);

    struct Step
    {
        juce::ValueTree tree;
        juce::String name;   // was zu diesem Stand geführt hat
    };

    InstrumentModel& model;
    juce::AudioFormatManager& formats;

    std::vector<Step> steps;
    int current = 0;
    juce::String pendingName;
    bool restoring = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoHistory)
};
} // namespace sis
