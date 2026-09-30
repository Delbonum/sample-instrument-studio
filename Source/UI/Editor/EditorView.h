#pragma once

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"
#include "../Widgets.h"
#include "SampleEditor.h"
#include "TrackArea.h"

namespace sis
{
/** Spuransicht einer Zone: Kopfzeile, Zeitlineal, Spuren und Sample-Editor.

    Ein Klick ins Zeitlineal setzt den Locator – dort beginnt die Wiedergabe; ein Klick
    auf seine Anzeige links im Lineal setzt ihn auf 0 zurück. Unter den Spuren liegt ein
    waagerechter Rollbalken, sobald ein Clip über die sichtbaren 8 Sekunden hinausreicht. */
class EditorView final : public juce::Component,
                         private juce::ChangeListener,
                         private juce::ScrollBar::Listener
{
public:
    explicit EditorView (StudioContext&);
    ~EditorView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    // Zwischenablage für die Clips (Strg+C / X / V aus der Shell)
    void copyClips()  { trackArea.copyClips(); }
    void cutClips()   { trackArea.cutClips(); }
    void pasteClips() { trackArea.pasteClips(); }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void setLocatorFrom (int x);
    void updateScrollBar();
    juce::Rectangle<int> getRulerLane() const;
    void updateStyles();

    StudioContext& ctx;
    FlatButton backButton { "‹ Mapping"_u };
    FlatButton trimModeButton { "Zuschneiden" }, stretchModeButton { "Stretchen" };
    FlatButton snapButton { "Raster 0.5 s" }, addTrackButton { "Spur hinzufügen"_u }, bounceButton { "Zone bouncen" };
    FlatButton zoomOutButton { "−"_u }, zoomInButton { "+" };

    /** Menü am Rasterknopf: aus oder eine der Rasterweiten. */
    void showGridMenu();

    juce::Viewport viewport;
    TrackArea trackArea;
    juce::ScrollBar timeScroll { false };
    juce::String shownZoneId;
    SampleEditor sampleEditor;

    juce::Rectangle<int> headerArea, rulerArea, segmentArea;
};
} // namespace sis
