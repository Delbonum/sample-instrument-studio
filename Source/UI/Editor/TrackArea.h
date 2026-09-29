#pragma once

#include <map>

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"
#include "ToolPalette.h"

namespace sis
{
/** Spuren der gewählten Zone mit ihren Clips auf der Zeitachse.

    Eine Spur trägt beliebig viele Clips. Überlappen sie, klingt der obere (zuletzt
    gesetzte); die Überschneidung ist schraffiert. X über zwei gewählten, sich
    überschneidenden Clips legt einen Crossfade über die ganze Überschneidung.

    Auswahl: Klick auf einen Clip wählt ihn, Klick auf einen Spurkopf die Spur – beides
    unabhängig, Shift erweitert. Entf löscht die gewählten Clips (nie eine Spur); Spuren
    löscht der Rechtsklick auf den Spurkopf. Strg+C / X / V (über die Befehle der Shell)
    kopieren, schneiden aus und fügen am Locator ein: der früheste Clip der obersten
    kopierten Spur landet in der gewählten Spur, die übrigen in derselben Lage dazu –
    fehlende Spuren darunter entstehen dabei. Ein Clip lässt sich auch auf eine andere
    Spur ziehen, unter die letzte gezogen entsteht eine neue.

    Werkzeuge: rechte Maustaste in der Zeitleiste halten öffnet die Werkzeugleiste –
    Auswahl, Löschen (Radiergummi), Schere. Im Auswahl-Werkzeug gelten die Gesten aus dem
    README: Körper ziehen verschiebt, Kante ziehen schneidet zu (Shift bzw. der
    Segmentschalter streckt), obere Ecken ziehen die Fades.

    Sichtbar sind immer 8 Sekunden ab `UiState::timelineStart`; längere Clips erreicht man
    über den Rollbalken darunter, Shift+Mausrad oder waagerechtes Wischen. */
class TrackArea final : public juce::Component,
                        public juce::DragAndDropTarget,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    static constexpr int rowHeight = 78;
    static constexpr int headerWidth = 140;

    /** Weiter nach rechts lässt sich ein Clip nicht ziehen: 16 Achsen = 128 Sekunden. */
    static constexpr double maxAxis = 16.0;

    explicit TrackArea (StudioContext&);
    ~TrackArea() override;

    int getIdealHeight() const;

    /** Länge der Achse in Anteilen der 8 Sekunden: mindestens 1, sonst bis hinter den
        letzten Clip (auf volle Sekunden, mit einer Sekunde Luft). */
    static double axisLength (const Zone*);

    /** x in dieser Komponente ↔ Zeit auf der Achse, unter Berücksichtigung des Bildlaufs. */
    float timeToX (double time) const;
    double xToTime (float x) const;

    // Zwischenablage (Strg+C / X / V)
    void copyClips();
    void cutClips();
    void pasteClips();

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    enum class Drag { none, move, leftEdge, rightEdge, fadeIn, fadeOut, erase, palette };

    /** Was unter dem Zeiger liegt: welcher Clip (Nummer in `clips`) und welcher Griff. */
    struct Hit
    {
        int clip = -1;
        Drag mode = Drag::none;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    Zone* getZone() const;
    juce::Rectangle<int> getRowBounds (int index) const;
    juce::Rectangle<int> getLaneBounds (int index) const;
    juce::Rectangle<float> getClipBounds (int trackIndex, int clipIndex) const;
    juce::Rectangle<int> getMuteBounds (int index) const;
    juce::Rectangle<int> getSoloBounds (int index) const;
    float laneWidth() const;

    int rowAt (juce::Point<int>) const;
    Hit hitAt (int trackIndex, juce::Point<int>) const;
    double positionToTime (int x) const;      // Zeit unter dem Zeiger, nie vor 0
    bool isStretching (const juce::MouseEvent&) const;

    void paintClip (juce::Graphics&, const Track&, int trackIndex, int clipIndex, bool audible) const;
    void paintOverlaps (juce::Graphics&, const Track&, int trackIndex) const;

    void applyDrag (const juce::MouseEvent&);
    void finishMove();

    // Auswahl
    void selectTrack (int trackIndex, bool extend);
    void selectClip (const Clip&, int trackIndex, bool extend);
    void clearClipSelection();

    // Bearbeiten
    void deleteSelectedClips();
    void eraseClipAt (int trackIndex, juce::Point<int>);
    void splitClipAt (int trackIndex, juce::Point<int>);
    void crossfadeSelected();
    void removeTracks (int clickedTrack);
    void showTrackMenu (int trackIndex);

    // Umbenennen: ein Textfeld über dem Namen im Spurkopf
    juce::Rectangle<int> getNameBounds (int trackIndex) const;
    void startRename (int trackIndex);
    void finishRename (bool keep);

    // Werkzeugleiste
    void openPalette (juce::Point<int>);
    void closePalette();
    void setTool (EditTool);

    /** Rollt um `delta` Achsen-Anteile, begrenzt auf die Länge der Achse. */
    void scrollBy (double delta);

    StudioContext& ctx;
    Drag drag = Drag::none;
    int dragTrack = -1;
    juce::uint32 dragClip = 0;
    double grabOffset = 0.0;
    bool dragMoved = false;
    /** Ausgangslage eines mitgezogenen Clips: Zeit und Spur. */
    struct Origin
    {
        double offset = 0.0;
        int track = 0;
    };

    std::map<juce::uint32, Origin> moveOrigins;
    int dragStartRow = 0;
    int createdTracks = 0;   // beim Ziehen nach unten neu angelegte Spuren

    // Schere: wo der Schnitt landen würde
    int hoverRow = -1;
    double hoverTime = -1.0;

    std::unique_ptr<juce::TextEditor> nameEditor;
    int renamingTrack = -1;

    std::unique_ptr<ToolPalette> palette;
    juce::Point<int> paletteOrigin;

    // Vorschau eines hereingezogenen Samples: Zeile (oder neue Spur darunter) und Zeitpunkt
    bool dropActive = false;
    int dropRow = -1;
    double dropTime = 0.0;
    double dropLength = 0.0;

    juce::String shownZoneId;
};
} // namespace sis
