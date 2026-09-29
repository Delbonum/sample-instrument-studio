#pragma once

#include <set>

#include <JuceHeader.h>

#include "../../Model/Instrument.h"

namespace sis
{
class StudioProcessor;

enum class View { mapping, editor, exportView };

/** Was das Ziehen an einer Clip-Kante ohne Zusatztaste bewirkt (Shift kehrt es um). */
enum class EdgeMode { trim, stretch };

/** Werkzeuge im Sample-Editor. */
enum class SampleTool { select, trim, fadeIn, fadeOut, normalise, reverse };

/** Zustand der Oberfläche (nicht Teil der .sisp-Datei). */
class UiState : public juce::ChangeBroadcaster
{
public:
    View view = View::mapping;

    bool showMenuBar = true;
    bool showLeftPanel = true;
    bool showRightPanel = true;

    /* Eingeklappte Abschnitte und Effektkarten. Das gehört in die Oberfläche und
         nicht ins Instrument: sonst löste jedes Einklappen einen Neuaufbau des
         Renderplans und einen Schritt im Verlauf aus.

         Effekte sind über ihren Namen gemerkt, nicht über ihre Nummer - so überlebt
         der Zustand das Umsortieren der Kette. */
    bool showPitchSection = true;
    bool showEffectSection = true;
    bool showEnvelopeSection = true;
    bool showMacroSection = true;
    std::set<juce::String> collapsedEffects;

    bool isEffectCollapsed (const juce::String& name) const { return collapsedEffects.count (name) > 0; }

    void toggleEffectCollapsed (const juce::String& name)
    {
        if (! collapsedEffects.insert (name).second)
            collapsedEffects.erase (name);

        changed();
    }

    bool velocityLayers = true;
    bool snapToGrid = true;
    EdgeMode edgeMode = EdgeMode::trim;

    /** Auswahl im Sample-Editor, als Anteil des ganzen Samples. */
    double selectionStart = 0.08;
    double selectionEnd = 0.62;
    SampleTool tool = SampleTool::select;

    bool playing = false;
    bool looping = true;
    double playhead = 0.0;       // Anteile der 8-Sekunden-Achse; mit langen Clips auch über 1

    /** Ab wo die Wiedergabe beginnt – im Zeitlineal des Editors gesetzt (Achsen-Anteile). */
    double locator = 0.0;

    /** Linker Rand des sichtbaren Ausschnitts im Editor (Achsen-Anteile). Sichtbar sind
        immer 8 Sekunden; was darüber hinausgeht, erreicht man durch Scrollen. */
    double timelineStart = 0.0;

    int heldNote = -1;           // angeschlagene Taste (340 ms), sonst -1
    int heldVelocity = 0;

    void changed() { sendChangeMessage(); }
};

namespace cmd
{
    enum : juce::CommandID
    {
        newInstrument = 0x5100,
        openProject,
        saveProject,
        saveProjectAs,
        importSamples,
        exportInstrument,
        audioSettings,
        quit,

        undo,
        redo,
        cut,
        copy,
        paste,
        trimToSelection,

        showMapping,
        showEditor,
        showExport,
        toggleMenuBar,
        toggleLeftPanel,
        toggleRightPanel,
        toggleVelocityLayers,
        toggleSnap,

        addZone,
        addTrack,
        bounceZone,
        assignMacros,

        openManual,
        showShortcuts,
        credits,

        togglePlayback
    };
} // namespace cmd

/** Was die Teile des Studio-Rahmens gemeinsam brauchen. */
struct StudioContext
{
    StudioProcessor& processor;
    InstrumentModel& model;
    UiState& ui;
    juce::ApplicationCommandManager& commands;
    std::function<void (const juce::String&)> toast;

    /** Benennt den nächsten Schritt im Verlauf; **vor** der Änderung aufrufen.
        Ohne Namen heißt der Schritt einfach „Änderung“ – erfasst wird er so oder so. */
    std::function<void (const juce::String&)> step;
};
} // namespace sis
