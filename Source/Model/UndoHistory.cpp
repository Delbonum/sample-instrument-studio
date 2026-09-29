#include "UndoHistory.h"
#include "Text.h"

namespace sis
{
UndoHistory::UndoHistory (InstrumentModel& m, juce::AudioFormatManager& f)
    : model (m), formats (f)
{
    reset();
}

UndoHistory::~UndoHistory()
{
    stopTimer();
}

//==============================================================================
void UndoHistory::reset()
{
    stopTimer();
    pendingName.clear();

    steps.clear();
    steps.push_back ({ model.toValueTree(), {} });
    current = 0;

    if (onChanged != nullptr)
        onChanged();
}

void UndoHistory::nameNextStep (const juce::String& name)
{
    if (restoring)
        return;

    // Ein anderer Name beginnt etwas Neues - das Offene vorher abschließen
    if (isTimerRunning() && name != pendingName)
        record();

    pendingName = name;
}

void UndoHistory::modelChanged()
{
    if (restoring)
        return;

    // Neu starten: erst wenn eine Weile Ruhe ist, wird der Schritt festgehalten
    startTimer (coalesceMilliseconds);
}

void UndoHistory::flush()
{
    if (isTimerRunning())
        record();
}

void UndoHistory::timerCallback()
{
    record();
}

//==============================================================================
void UndoHistory::record()
{
    stopTimer();

    auto tree = model.toValueTree();

    if (steps.empty())
    {
        steps.push_back ({ tree, {} });
        current = 0;
        pendingName.clear();
        return;
    }

    auto& present = steps[(size_t) current];

    // Wurde nur etwas anderes ausgewählt, ist das kein Schritt - sonst füllt
    // bloßes Herumklicken den Verlauf.
    if (withoutSelection (present.tree).isEquivalentTo (withoutSelection (tree)))
    {
        present.tree = tree;
        pendingName.clear();
        return;
    }

    steps.erase (steps.begin() + current + 1, steps.end());
    steps.push_back ({ tree, pendingName.isNotEmpty() ? pendingName : juce::String ("Änderung"_u) });
    pendingName.clear();

    while ((int) steps.size() > maxSteps)
        steps.erase (steps.begin());

    current = (int) steps.size() - 1;

    if (onChanged != nullptr)
        onChanged();
}

void UndoHistory::restore (const juce::ValueTree& tree)
{
    const juce::ScopedValueSetter<bool> guard (restoring, true);

    stopTimer();
    pendingName.clear();

    const auto folder = model.projectFile != juce::File() ? model.projectFile.getParentDirectory()
                                                          : juce::File();
    model.fromValueTree (tree, formats, folder);
    model.notifyChanged();
}

//==============================================================================
juce::String UndoHistory::getUndoName() const
{
    return canUndo() ? steps[(size_t) current].name : juce::String();
}

juce::String UndoHistory::getRedoName() const
{
    return canRedo() ? steps[(size_t) current + 1].name : juce::String();
}

bool UndoHistory::undo()
{
    flush();

    if (! canUndo())
        return false;

    --current;
    restore (steps[(size_t) current].tree);

    if (onChanged != nullptr)
        onChanged();

    return true;
}

bool UndoHistory::redo()
{
    flush();

    if (! canRedo())
        return false;

    ++current;
    restore (steps[(size_t) current].tree);

    if (onChanged != nullptr)
        onChanged();

    return true;
}
} // namespace sis
