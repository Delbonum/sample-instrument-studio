#pragma once

#include <map>

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"
#include "../Widgets.h"

namespace sis
{
/** Mapping über ein gezeichnetes Schlagzeug (von oben gesehen).

    Jeder Klang des Kits ist eine Zone mit **einer** Taste. Klick wählt das Teil und
    schlägt es an, Doppelklick öffnet es im Editor. Ein Teil, das noch kein Sample hat,
    wird blass und gestrichelt gezeichnet; angeklickt legt es seine Zone an, damit das
    nächste Sample aus dem Browser ein Ziel hat. Ein Sample lässt sich auch direkt aus dem
    Browser auf ein Teil ziehen – die Ansicht bleibt dabei, wo sie ist, und das Teil zeigt
    mit einer Marke, dass jetzt etwas darin liegt.

    Welche Teile aufgebaut sind, steht im Instrument (`kitPieces`): Becken, Toms und
    Percussion kommen über „Teil hinzufügen“ dazu und per Rechtsklick wieder weg. Teile mit
    mehreren Spielweisen (HiHat, Snare, Ride) stehen nur einmal da, mit einem Umschalter
    darunter.

    Die Anordnung steckt in `layout()` als Anteile der Fläche, nicht in festen Pixeln —
    die Ansicht soll mit dem Fenster wachsen. */
class DrumKitView final : public juce::Component,
                          public juce::DragAndDropTarget,
                          private juce::ChangeListener,
                          private juce::Timer
{
public:
    explicit DrumKitView (StudioContext&);
    ~DrumKitView() override;

    /** Doppelklick auf ein Teil. */
    std::function<void (const juce::String& zoneId)> onOpenPart;

    /** Menü „Teil hinzufügen“, am Knopf in der Kopfzeile des Mappings. */
    void showAddPieceMenu (juce::Component& anchor);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    /** Ein Teil des Kits an seinem Platz. */
    struct Placed
    {
        const DrumPiece* piece = nullptr;
        juce::Rectangle<float> bounds;   // Ellipse
        bool cymbal = false;             // Becken werden flacher und heller gezeichnet

        /** Umschalter der Spielweisen, nur bei mehr als einer. */
        std::vector<std::pair<const DrumPart*, juce::Rectangle<float>>> switches;
    };

    /** Was unter dem Mauszeiger liegt: ein Teil, und welche seiner Spielweisen. */
    struct Hit
    {
        const DrumPiece* piece = nullptr;
        const DrumPart* part = nullptr;
    };

    void layout();
    Hit hitAt (juce::Point<float>) const;

    /** Die Spielweise, die das Teil gerade zeigt. */
    const DrumPart& currentPart (const DrumPiece&) const;

    /** Wählt die Zone des Klangs (legt sie bei Bedarf an) und schlägt ihn an. */
    void choose (const DrumPiece&, const DrumPart&);
    void play (const DrumPart&);
    void releaseNote();

    juce::PopupMenu buildAddMenu();
    void showPieceMenu (const DrumPiece&);

    /** Kurzes Aufleuchten, nachdem ein Sample auf einem Teil gelandet ist. */
    void flash (const DrumPart&);

    void paintPiece (juce::Graphics&, const Placed&) const;

    StudioContext& ctx;
    std::vector<Placed> pieces;
    juce::Rectangle<int> stage;
    int soundingNote = -1;

    std::map<juce::String, juce::String> articulation;   // Teil → gezeigte Spielweise
    const DrumPart* dropTarget = nullptr;
    const DrumPart* flashing = nullptr;
    double flashStart = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumKitView)
};
} // namespace sis
