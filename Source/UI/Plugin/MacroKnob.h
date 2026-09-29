#pragma once

#include "../Theme.h"
#include "../Widgets.h"

namespace sis
{
/** Drehregler der Plugin-Ansicht: Kreis mit Zeiger, senkrecht ziehbar.

    Der Bereich geht von −140° bis +140°, wie in der Vorlage. Gezogen wird senkrecht –
    im Kreis herumzufahren ist auf kleinen Reglern unbrauchbar, weil der Mauszeiger die
    Mitte streift und der Wert dann springt. */
class MacroKnob final : public juce::Component
{
public:
    static constexpr int diameter = 44;
    static constexpr int fullHeight = 44 + 5 + 22 + 12;   // Regler, Name (2 Zeilen), Wert

    MacroKnob();

    void setValue (float newValue, juce::NotificationType = juce::dontSendNotification);
    float getValue() const noexcept { return value; }

    void setLabel (const juce::String&);

    /** Ob der Regler etwas steuert – sonst wird er blass gezeichnet. */
    void setAssigned (bool);

    std::function<void (float)> onValueChange;
    std::function<void()> onDragStart, onDragEnd;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    float value = 0.5f;
    float valueAtDragStart = 0.5f;
    bool assigned = false;
    juce::String label;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroKnob)
};
} // namespace sis
