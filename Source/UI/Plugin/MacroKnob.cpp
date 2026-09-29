#include "MacroKnob.h"

namespace sis
{
namespace
{
    constexpr float sweepDegrees = 140.0f;   // −140° … +140°, wie in der Vorlage
    constexpr float pixelsForFullRange = 140.0f;
    constexpr int labelHeight = 22;   // zwei Zeilen Mono 9,5 px
}

MacroKnob::MacroKnob()
{
    setWantsKeyboardFocus (false);
}

void MacroKnob::setValue (float newValue, juce::NotificationType notification)
{
    const float limited = juce::jlimit (0.0f, 1.0f, newValue);

    if (juce::approximatelyEqual (limited, value))
        return;

    value = limited;
    repaint();

    if (notification != juce::dontSendNotification && onValueChange != nullptr)
        onValueChange (value);
}

void MacroKnob::setLabel (const juce::String& newLabel)
{
    if (label == newLabel)
        return;

    label = newLabel;
    repaint();
}

void MacroKnob::setAssigned (bool isAssigned)
{
    if (assigned == isAssigned)
        return;

    assigned = isAssigned;
    repaint();
}

void MacroKnob::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    const auto circle = area.removeFromTop (diameter).withSizeKeepingCentre (diameter, diameter).toFloat();

    g.setColour (colours::white);
    g.fillEllipse (circle);
    g.setColour (colours::lineStrongAlt);
    g.drawEllipse (circle, 1.0f);

    // Zeiger: 2 px breit, 17 px lang, von der Mitte nach außen
    const float angle = juce::degreesToRadians (-sweepDegrees + 2.0f * sweepDegrees * value);
    const auto centre = circle.getCentre();

    juce::Path pointer;
    pointer.addRectangle (-1.0f, -17.0f, 2.0f, 17.0f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

    g.setColour (assigned ? colours::accent : colours::lineStrong);
    g.fillPath (pointer);

    area.removeFromTop (5);

    /* Zweizeilig: „Luft / Obertöne“ passt unter einen 44-px-Regler sonst nicht, und ein
       abgeschnittener Name sagt nichts mehr über das, was der Regler tut. */
    g.setColour (assigned ? colours::text : colours::textTertiary);
    g.setFont (monoFont (9.5f));
    g.drawFittedText (label, area.removeFromTop (labelHeight), juce::Justification::centredTop, 2);

    g.setColour (colours::textTertiary);
    g.drawText (juce::String (juce::roundToInt (value * 100.0f)), area.removeFromTop (12),
                juce::Justification::centred, false);
}

void MacroKnob::mouseDown (const juce::MouseEvent&)
{
    valueAtDragStart = value;

    if (onDragStart != nullptr)
        onDragStart();
}

void MacroKnob::mouseDrag (const juce::MouseEvent& e)
{
    // Nach oben ziehen heißt mehr
    setValue (valueAtDragStart - (float) e.getDistanceFromDragStartY() / pixelsForFullRange,
              juce::sendNotification);
}

void MacroKnob::mouseUp (const juce::MouseEvent&)
{
    if (onDragEnd != nullptr)
        onDragEnd();
}
} // namespace sis
