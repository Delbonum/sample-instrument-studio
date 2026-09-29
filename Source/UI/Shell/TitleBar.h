#pragma once

#include <JuceHeader.h>

#include "../../Model/Instrument.h"
#include "../Widgets.h"

namespace sis
{
/** Selbst gezeichnete Fensterleiste (34 px), nur im Standalone unter Windows/Linux.
    Ziehen verschiebt das Fenster, Doppelklick maximiert. */
class TitleBar final : public juce::Component, private juce::ChangeListener
{
public:
    explicit TitleBar (InstrumentModel&);
    ~TitleBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    juce::ResizableWindow* getWindow() const;
    juce::String getTitle() const;
    void toggleMaximised();

    InstrumentModel& model;
    juce::Image icon;
    FlatButton minimiseButton, maximiseButton, closeButton;
    juce::ComponentDragger dragger;
    bool dragging = false;
};
} // namespace sis
