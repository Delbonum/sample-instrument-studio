#pragma once

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"
#include "../Widgets.h"

namespace sis
{
/** Inhalt der rechten Spalte: Zone, Hüllkurve, Makros am Instrument. */
class ZonePanelContent final : public juce::Component, private juce::ChangeListener
{
public:
    explicit ZonePanelContent (StudioContext&);
    ~ZonePanelContent() override;

    int getIdealHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void paintMacroTitle (juce::Graphics&) const;
    void updateFromModel();
    juce::Path createEnvelopePath (juce::Rectangle<float>) const;

    struct Row
    {
        juce::String label;
        juce::Rectangle<int> labelArea, valueArea;
        ValueBar bar;
    };

    void stepRoot (int delta);
    void stepRange (bool lowEnd, int delta);
    void stepVelocity (bool lowEnd, int delta);

    StudioContext& ctx;
    std::array<Row, 4> envelopeRows;
    std::array<Row, 4> macroRows;

    // Tastenbereich, Velocity und Grundton lassen sich hier ändern
    Stepper rootStepper, lowNoteStepper, highNoteStepper, lowVelocityStepper, highVelocityStepper;

    juce::Rectangle<int> headerArea, zoneSection, envelopeSection, macroSection;
    juce::Rectangle<int> titleArea, envelopeArea;
    juce::Rectangle<int> envelopeTitleArea, macroTitleArea;
    juce::Rectangle<int> rootLabel, rangeLabel, velocityLabel, samplesLabel, samplesValue;
};

/** Rechte Spalte (286 px), scrollt bei kleinen Fenstern. */
class ZonePanel final : public juce::Component, private juce::ChangeListener
{
public:
    explicit ZonePanel (StudioContext&);
    ~ZonePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /* Wie beim Spur-Inspektor: eingeklappte Abschnitte ändern nur die Höhe des
       Inhalts, nicht die des Rahmens. */
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    StudioContext& ctx;
    ZonePanelContent content;
    juce::Viewport viewport;
};
} // namespace sis
