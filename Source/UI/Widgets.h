#pragma once

#include <JuceHeader.h>

#include "../Model/Text.h"
#include "Assets.h"
#include "Theme.h"

namespace sis
{
struct FlatButtonStyle
{
    juce::Colour background = juce::Colours::transparentBlack;
    juce::Colour text = colours::textSecondary;
    juce::Colour border = juce::Colours::transparentBlack;
    juce::Colour hoverBackground = juce::Colours::transparentBlack;   // transparent = unverändert
    juce::Colour hoverText = juce::Colours::transparentBlack;
    juce::Colour underline = juce::Colours::transparentBlack;
    float fontSize = 12.0f;
    bool mono = false;
    Weight weight = Weight::regular;
    juce::Justification justification = juce::Justification::centred;
};

/** Flacher Knopf der Referenz: Fläche, 1-px-Rand, optional Unterstrich (Reiter)
    und Hover-Farben. Nimmt keinen Tastaturfokus, damit die Leertaste die Wiedergabe steuert. */
class FlatButton : public juce::Button
{
public:
    using Style = FlatButtonStyle;

    /** Zeichnet statt Text ein Symbol (Play, Fensterknöpfe …) in der aktuellen Textfarbe. */
    using IconPainter = std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)>;

    explicit FlatButton (const juce::String& text = {}, Style = {});

    void setStyle (const Style&);
    const Style& getStyle() const noexcept { return style; }
    void setIcon (IconPainter);

    juce::Font getFont() const;
    int getTextWidth() const;

    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

private:
    Style style;
    IconPainter icon;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlatButton)
};

//==============================================================================
/** Waagerechter Schieber: 5–6 px Schiene #DCD9D3, Füllung in Akzent- oder Spurfarbe.
    Die Komponente darf höher sein als die Schiene (größere Klickfläche). */
class ValueBar : public juce::Component
{
public:
    enum class Mode { fromLeft, bipolar };

    ValueBar();

    void setValue (float newValue, juce::NotificationType = juce::dontSendNotification);
    float getValue() const noexcept { return value; }

    void setFillColour (juce::Colour);
    void setTrackHeight (float);
    void setMode (Mode);

    std::function<void (float)> onValueChange;

    /** Rechtsklick – hier hängt die Makro-Zuweisung dran. */
    std::function<void()> onSecondaryClick;
    std::function<void()> onDragStart, onDragEnd;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void setFromPosition (float x);

    float value = 0.0f;
    float trackHeight = 5.0f;
    juce::Colour fill = colours::accent;
    Mode mode = Mode::fromLeft;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBar)
};

//==============================================================================
/** Wert mit − / + und waagerechtem Ziehen, für Tastenbereich, Velocity und Grundton. */
class Stepper : public juce::Component
{
public:
    Stepper();

    /** Schrittweite: +1 / −1 je Klick, beim Ziehen entsprechend mehr. */
    std::function<void (int delta)> onStep;

    void setText (const juce::String&);
    void setEnabledState (bool);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    FlatButton downButton { "–"_u }, upButton { "+" };
    juce::String text;
    int dragSteps = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Stepper)
};

//==============================================================================
namespace draw
{
    /** Versaler Abschnittstitel (Mono 10 px, 0.09em, #56534E). */
    void capsLabel (juce::Graphics&, const juce::String& text, juce::Rectangle<int> area,
                    float size = 10.0f, float tracking = 0.09f,
                    juce::Justification = juce::Justification::centredLeft);

    /** Kleines Dreieck als Aufklapp-Anzeige: nach unten offen, nach rechts zu. */
    void disclosure (juce::Graphics&, juce::Rectangle<int> area, bool open, juce::Colour);

    /** Waagerechte 1-px-Linie am unteren Rand. */
    void bottomLine (juce::Graphics&, juce::Rectangle<int> area, juce::Colour);

    /** Mini-Wellenform als Balken, vertikal zentriert. */
    void waveBars (juce::Graphics&, juce::Rectangle<float> area, const std::vector<float>& peaks,
                   int numBars, float gap, float minHeight, juce::Colour);

    /** Aussteuerungsanzeige: Balken mit Warnfarbe kurz unter Vollaussteuerung. */
    void levelMeter (juce::Graphics&, juce::Rectangle<float>, float level);

    void playIcon (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void stopIcon (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
} // namespace draw
} // namespace sis
