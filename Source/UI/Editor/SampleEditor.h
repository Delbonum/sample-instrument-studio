#pragma once

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"
#include "../Widgets.h"

namespace sis
{
/** Untere Zone (186 px): volle Wellenform des gewählten Clips mit Auswahl und Werkzeugen.

    Der Locator der Zeitleiste läuft hier mit: er steht an der Stelle des Samples, die zur
    Locator-Zeit im Clip erklingt, und lässt sich auch hier ziehen – es ist derselbe
    Locator. Oben im Wellenfeld (Streifen mit dem Dreieck) setzt ein Klick ihn. */
class SampleEditor final : public juce::Component,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    explicit SampleEditor (StudioContext&);
    ~SampleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void updateToolStyles();
    void applyTool (SampleTool);
    Track* getTrack() const;
    Clip* getClip() const;
    double sampleSeconds() const;
    double positionToFraction (int x) const;
    float fractionToX (double fraction) const;

    /** Zeit auf der Achse ↔ Stelle im Sample (Anteil 0 … 1), über den gewählten Clip. */
    bool timeToFraction (double time, double& fraction) const;
    double fractionToTime (double fraction) const;

    bool isOnLocator (juce::Point<int>) const;

    StudioContext& ctx;
    std::array<FlatButton, 6> tools;
    juce::Rectangle<int> headerArea, waveArea;
    int toolsLeft = 0;            // linke Kante des ersten Werkzeugknopfs
    int draggingHandle = 0;       // 0 = keiner, 1 = Anfang, 2 = Ende, 3 = Locator
};
} // namespace sis
