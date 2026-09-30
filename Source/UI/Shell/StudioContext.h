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

/** Werkzeuge der Spuren im Editor – gewählt über die Werkzeugleiste, die ein gehaltener
    Rechtsklick in der Zeitleiste öffnet. */
enum class EditTool { select, erase, split };

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
    double gridSeconds = 0.5;    // Rasterweite im Editor

    /** Wie viel der Achse der Editor zeigt (1 = 8 Sekunden). Kleiner heißt hineingezoomt. */
    double visibleLength = 1.0;
    static constexpr double minVisibleLength = 1.0 / 32.0;   // 0,25 s
    static constexpr double maxVisibleLength = 4.0;          // 32 s
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

    /* Auswahl im Editor. Clips über ihre Laufzeit-Kennung (die Nummern verschieben sich beim
       Umstapeln), Spuren über ihre Nummer in der gewählten Zone. Beides unabhängig
       voneinander; Shift erweitert. Die Hauptspur bleibt `Zone::selectedTrack`. */
    std::set<juce::uint32> selectedClips;
    std::set<int> selectedTracks;
    juce::uint32 focusClip = 0;  // zuletzt angeklickt: den zeigen Sample-Editor und Inspektor
    EditTool editTool = EditTool::select;
    std::vector<ClipboardClip> clipboard;

    bool isClipSelected (juce::uint32 uid) const { return selectedClips.count (uid) > 0; }

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

        togglePlayback,

        keepTempoAll,      // „Tempo halten“ für alle Spuren des Instruments ein …
        keepTempoNone      // … bzw. aus
    };
} // namespace cmd

/** Der Clip, den Sample-Editor und Inspektor zeigen: der zuletzt angeklickte, sofern er auf
    der gewählten Spur liegt, sonst deren erster. */
inline Clip* currentClip (InstrumentModel& model, const UiState& ui)
{
    auto* zone = model.getSelectedZone();
    auto* track = zone != nullptr ? zone->getSelectedTrack() : nullptr;

    if (track == nullptr)
        return nullptr;

    if (auto* focused = track->findClip (ui.focusClip))
        return focused;

    for (auto& clip : track->clips)
        if (clip.hasSample())
            return &clip;

    return nullptr;
}

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
/** „Umkehren“: gilt den gewählten Clips der Zone, wenn der gezeigte Clip dazugehört, sonst
    nur dem gezeigten. Alle bekommen dieselbe Richtung – die umgekehrte des gezeigten.
    Gibt die Zahl der umgeschalteten Clips zurück (0, wenn es keinen gab). */
inline int toggleReverse (StudioContext& ctx, bool& nowReversed)
{
    auto* shown = currentClip (ctx.model, ctx.ui);
    auto* zone = ctx.model.getSelectedZone();

    if (shown == nullptr || zone == nullptr)
        return 0;

    nowReversed = ! shown->reverse;
    const bool wholeSelection = ctx.ui.isClipSelected (shown->uid);
    int changed = 0;

    ctx.step (nowReversed ? "Umgekehrt"_u : "Wieder vorwärts"_u);

    for (auto& track : zone->tracks)
        for (auto& clip : track.clips)
            if (&clip == shown || (wholeSelection && ctx.ui.isClipSelected (clip.uid)))
            {
                clip.reverse = nowReversed;
                ++changed;
            }

    ctx.model.notifyChanged();
    return changed;
}
} // namespace sis
