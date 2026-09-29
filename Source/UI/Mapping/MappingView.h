#pragma once

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"
#include "../Widgets.h"
#include "DrumKitView.h"
#include "PianoKeyboard.h"
#include "ZoneGrid.h"

namespace sis
{
/** Startansicht: Zonen über die Tastatur verteilen.

    Zwei Gestalten, je nach `InstrumentKind`: das Zonenraster über der Klaviatur für ein
    Tonhöhen-Instrument, das gezeichnete Schlagzeug für ein Drumset. Umgeschaltet wird
    oben rechts — und zwar die **Art des Instruments**, nicht bloß die Ansicht, denn davon
    hängt ab, ob die Taste die Tonhöhe bestimmt. */
class MappingView final : public juce::Component, private juce::ChangeListener
{
public:
    explicit MappingView (StudioContext&);
    ~MappingView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    StudioContext& ctx;
    void updateStyles();
    bool isDrumKit() const;

    FlatButton addZoneButton { "Zone hinzufügen"_u };
    FlatButton addPieceButton { "Teil hinzufügen"_u };
    FlatButton melodicButton { "Tonhöhen"_u }, drumButton { "Drumset" };
    ZoneGrid grid;
    DrumKitView drums;
    PianoKeyboard keyboard;
    juce::Rectangle<int> headerArea, axisArea, segmentArea;
};
} // namespace sis
