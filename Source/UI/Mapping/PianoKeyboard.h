#pragma once

#include <JuceHeader.h>

#include "../../Model/Instrument.h"

namespace sis
{
/** Klaviatur C1–B6 (42 weiße, 30 schwarze Tasten). Tasten der gewählten Zone sind eingefärbt,
    die angeschlagene Taste leuchtet 340 ms. Mausklick spielt die Note über den MidiKeyboardState;
    die Anschlagshöhe bestimmt die Velocity (unten = laut). */
class PianoKeyboard final : public juce::Component,
                            public juce::DragAndDropTarget,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    PianoKeyboard (InstrumentModel&, juce::MidiKeyboardState&);
    ~PianoKeyboard() override;

    /** Nach dem Anschlag (Note, Velocity 1–127) bzw. wenn die Hervorhebung endet (-1, 0). */
    std::function<void (int note, int velocity)> onHighlightChanged;

    /** Ein Sample aus dem Browser wurde auf eine Taste gezogen. */
    std::function<void (int note, int sampleIndex)> onSampleDropped;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct Key
    {
        int note = 0;
        bool black = false;
        juce::Rectangle<float> bounds;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    const Key* keyAt (juce::Point<float>) const;

    InstrumentModel& model;
    juce::MidiKeyboardState& keyboardState;
    std::vector<Key> keys;      // erst weiße, dann schwarze Tasten
    int highlightedNote = -1;
    int soundingNote = -1;
    int dropNote = -1;          // Taste unter einem gezogenen Sample
};
} // namespace sis
