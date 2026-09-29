#pragma once

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"

namespace sis
{
/** Spuren der gewählten Zone mit Clips auf der Zeitachse.

    Gesten (siehe README): Körper ziehen verschiebt, Kante ziehen schneidet zu
    (Shift bzw. der Segmentschalter streckt), obere Ecken ziehen die Fades. Entf entfernt
    den Clip der gewählten Spur (eine leere Spur löscht sie ganz), Rechtsklick bietet
    „Spur löschen“ an. Ein Sample aus dem Browser lässt sich auf eine Spur ziehen – oder
    unter die letzte, dann entsteht eine neue.

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

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    enum class Drag { none, move, leftEdge, rightEdge, fadeIn, fadeOut };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    Zone* getZone() const;
    juce::Rectangle<int> getRowBounds (int index) const;
    juce::Rectangle<int> getLaneBounds (int index) const;
    juce::Rectangle<float> getClipBounds (int index) const;
    juce::Rectangle<int> getMuteBounds (int index) const;
    juce::Rectangle<int> getSoloBounds (int index) const;
    float laneWidth() const;

    int rowAt (juce::Point<int>) const;
    Drag dragModeAt (int trackIndex, juce::Point<int>) const;
    double positionToTime (int x) const;      // Zeit unter dem Zeiger, nie vor 0
    bool isStretching (const juce::MouseEvent&) const;

    void applyDrag (const juce::MouseEvent&);

    /** Entfernt den Clip der Spur; eine schon leere Spur wird gelöscht. */
    void deleteSelected();
    void removeClip (int trackIndex);
    void removeTrack (int trackIndex);
    void showTrackMenu (int trackIndex);

    /** Rollt um `delta` Achsen-Anteile, begrenzt auf die Länge der Achse. */
    void scrollBy (double delta);

    StudioContext& ctx;
    Drag drag = Drag::none;
    int dragTrack = -1;
    double grabOffset = 0.0;
    int hoverRow = -1;

    // Vorschau eines hereingezogenen Samples: Zeile (oder neue Spur darunter) und Zeitpunkt
    bool dropActive = false;
    int dropRow = -1;
    double dropTime = 0.0;
    double dropLength = 0.0;
};
} // namespace sis
