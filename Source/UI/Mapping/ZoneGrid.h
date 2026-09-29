#pragma once

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"

namespace sis
{
/** Zonenraster: X = Tonhöhe C1–B6, Y = Velocity 127 (oben) bis 0.
    Klick wählt eine Zone, Doppelklick öffnet sie.

    Ein Sample aus dem Browser lässt sich hineinziehen: auf eine Zone gelegt, wird es
    dort eine Spur; auf freie Fläche gelegt, entsteht darum eine neue Zone. Eine Zone mit
    Samples trägt eine Marke mit ihrer Anzahl und eine kleine Wellenform, damit man ohne
    Editor sieht, dass die Zuordnung geklappt hat. */
class ZoneGrid final : public juce::Component,
                       public juce::DragAndDropTarget,
                       private juce::ChangeListener,
                       private juce::Timer
{
public:
    explicit ZoneGrid (StudioContext&);
    ~ZoneGrid() override;

    std::function<void (const juce::String& zoneId)> onOpenZone;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    juce::Rectangle<float> getInnerArea() const;
    juce::Rectangle<float> getZoneBounds (const Zone&) const;
    const Zone* zoneAt (juce::Point<float>) const;
    int noteAt (float x) const;
    int velocityAt (float y) const;

    StudioContext& ctx;
    InstrumentModel& model;
    UiState& ui;

    // Während ein Sample darübergezogen wird
    bool dragging = false;
    juce::String dropZoneId;
    juce::Point<float> dropPosition;

    juce::String flashingZoneId;
    double flashStart = 0.0;
};
} // namespace sis
