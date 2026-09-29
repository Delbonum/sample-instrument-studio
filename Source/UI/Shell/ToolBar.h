#pragma once

#include <JuceHeader.h>

#include "../Widgets.h"
#include "StudioContext.h"

namespace sis
{
/** Werkzeugleiste (46 px): Reiter Mapping | Editor | Export, Transport, Master, Exportieren. */
class ToolBar final : public juce::Component,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    explicit ToolBar (StudioContext&);
    ~ToolBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void updateStyles();

    StudioContext& ctx;
    FlatButton mappingTab { "Mapping" }, editorTab { "Editor" }, exportTab { "Export" };
    FlatButton playButton { "Wiedergabe" }, loopButton { "LOOP" }, exportButton { "Exportieren" };
    ValueBar masterBar;
    juce::Rectangle<int> masterLabelArea, masterValueArea, meterArea;
    float lastLevel = 0.0f;
};
} // namespace sis
