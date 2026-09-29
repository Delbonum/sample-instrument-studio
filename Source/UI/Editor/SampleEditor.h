#pragma once

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"
#include "../Widgets.h"

namespace sis
{
/** Untere Zone (186 px): volle Wellenform des Samples mit Auswahl und Werkzeugen. */
class SampleEditor final : public juce::Component, private juce::ChangeListener
{
public:
    explicit SampleEditor (StudioContext&);
    ~SampleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateToolStyles();
    void applyTool (SampleTool);
    Track* getTrack() const;
    double sampleSeconds() const;
    double positionToFraction (int x) const;

    StudioContext& ctx;
    std::array<FlatButton, 6> tools;
    juce::Rectangle<int> headerArea, waveArea;
    int toolsLeft = 0;            // linke Kante des ersten Werkzeugknopfs
    int draggingHandle = 0;   // 0 = keiner, 1 = Anfang, 2 = Ende
};
} // namespace sis
