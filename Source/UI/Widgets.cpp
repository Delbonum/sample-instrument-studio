#include "Widgets.h"

namespace sis
{
FlatButton::FlatButton (const juce::String& text, Style s)
    : juce::Button (text), style (s)
{
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}

void FlatButton::setStyle (const Style& s)
{
    style = s;
    repaint();
}

void FlatButton::setIcon (IconPainter painter)
{
    icon = std::move (painter);
    repaint();
}

juce::Font FlatButton::getFont() const
{
    return style.mono ? monoFont (style.fontSize, style.weight) : sansFont (style.fontSize, style.weight);
}

int FlatButton::getTextWidth() const
{
    return juce::roundToInt (textWidth (getFont(), getButtonText()));
}

void FlatButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    auto bg = style.background;
    auto fg = style.text;

    if (isMouseOver || isButtonDown)
    {
        if (! style.hoverBackground.isTransparent()) bg = style.hoverBackground;
        if (! style.hoverText.isTransparent())       fg = style.hoverText;
    }

    if (! isEnabled())
        fg = fg.withMultipliedAlpha (0.45f);

    if (! bg.isTransparent())
    {
        g.setColour (bg);
        g.fillRect (bounds);
    }

    if (! style.border.isTransparent())
    {
        g.setColour (style.border);
        g.drawRect (bounds, 1.0f);
    }

    if (! style.underline.isTransparent())
    {
        g.setColour (style.underline);
        g.fillRect (bounds.removeFromBottom (2.0f));
    }

    if (icon != nullptr)
    {
        icon (g, bounds, fg);
        return;
    }

    g.setColour (fg);
    g.setFont (getFont());
    g.drawText (getButtonText(), bounds.reduced (style.justification.testFlags (juce::Justification::left) ? 8.0f : 0.0f, 0.0f),
                style.justification, true);
}

//==============================================================================
ValueBar::ValueBar()
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void ValueBar::setValue (float newValue, juce::NotificationType notification)
{
    newValue = juce::jlimit (0.0f, 1.0f, newValue);

    if (juce::approximatelyEqual (newValue, value))
        return;

    value = newValue;
    repaint();

    if (notification != juce::dontSendNotification && onValueChange != nullptr)
        onValueChange (value);
}

void ValueBar::setFillColour (juce::Colour c)   { fill = c; repaint(); }
void ValueBar::setTrackHeight (float h)         { trackHeight = h; repaint(); }
void ValueBar::setMode (Mode m)                 { mode = m; repaint(); }

void ValueBar::paint (juce::Graphics& g)
{
    const auto track = getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), trackHeight);

    g.setColour (colours::sliderTrack);
    g.fillRect (track);

    g.setColour (isEnabled() ? fill : colours::lineStrong);

    if (mode == Mode::bipolar)
    {
        // Pan: Füllung von der Mitte aus
        const float centre = track.getCentreX();
        const float pos = track.getX() + value * track.getWidth();
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (centre, pos), track.getY(),
                                                               juce::jmax (centre, pos), track.getBottom()));
    }
    else
    {
        g.fillRect (track.withWidth (value * track.getWidth()));
    }
}

void ValueBar::setFromPosition (float x)
{
    if (getWidth() > 0)
        setValue (x / (float) getWidth(), juce::sendNotificationSync);
}

void ValueBar::mouseDown (const juce::MouseEvent& e)
{
    // Die zweite Maustaste verstellt nichts, sie fragt
    if (e.mods.isPopupMenu())
    {
        if (onSecondaryClick != nullptr)
            onSecondaryClick();

        return;
    }

    if (onDragStart != nullptr)
        onDragStart();

    setFromPosition (e.position.x);
}

void ValueBar::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    setFromPosition (e.position.x);
}

void ValueBar::mouseUp (const juce::MouseEvent&)
{
    if (onDragEnd != nullptr)
        onDragEnd();
}

//==============================================================================
Stepper::Stepper()
{
    downButton.onClick = [this] { if (onStep != nullptr) onStep (-1); };
    upButton.onClick = [this] { if (onStep != nullptr) onStep (1); };

    FlatButton::Style style;
    style.background = colours::white;
    style.border = colours::lineStrongAlt;
    style.text = colours::text;
    style.fontSize = 11.0f;
    downButton.setStyle (style);
    upButton.setStyle (style);

    addAndMakeVisible (downButton);
    addAndMakeVisible (upButton);
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void Stepper::setText (const juce::String& newText)
{
    if (text != newText)
    {
        text = newText;
        repaint();
    }
}

void Stepper::setEnabledState (bool shouldBeEnabled)
{
    setEnabled (shouldBeEnabled);
    downButton.setEnabled (shouldBeEnabled);
    upButton.setEnabled (shouldBeEnabled);
    repaint();
}

void Stepper::resized()
{
    auto area = getLocalBounds();
    downButton.setBounds (area.removeFromLeft (20));
    upButton.setBounds (area.removeFromRight (20));
}

void Stepper::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().withTrimmedLeft (20).withTrimmedRight (20);

    g.setColour (colours::surface);
    g.fillRect (area);
    g.setColour (colours::lineFine);
    g.drawRect (area, 1);

    g.setColour (isEnabled() ? colours::text : colours::textTertiary);
    g.setFont (monoFont (12.0f));
    g.drawText (text, area, juce::Justification::centred, false);
}

void Stepper::mouseDown (const juce::MouseEvent&)
{
    dragSteps = 0;
}

void Stepper::mouseDrag (const juce::MouseEvent& e)
{
    // Ziehen ändert den Wert: 6 px je Schritt
    const int steps = e.getDistanceFromDragStartX() / 6;

    if (steps != dragSteps && onStep != nullptr)
    {
        onStep (steps - dragSteps);
        dragSteps = steps;
    }
}

//==============================================================================
namespace draw
{
    void capsLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                    float size, float tracking, juce::Justification justification)
    {
        g.setColour (colours::textSecondary);
        g.setFont (capsFont (size, tracking));
        g.drawText (text.toUpperCase(), area, justification, true);
    }

    void disclosure (juce::Graphics& g, juce::Rectangle<int> area, bool open, juce::Colour colour)
    {
        const auto box = area.toFloat().withSizeKeepingCentre (7.0f, 7.0f);

        juce::Path triangle;

        if (open)
        {
            triangle.addTriangle (box.getX(), box.getY(),
                                  box.getRight(), box.getY(),
                                  box.getCentreX(), box.getBottom());
        }
        else
        {
            triangle.addTriangle (box.getX(), box.getY(),
                                  box.getRight(), box.getCentreY(),
                                  box.getX(), box.getBottom());
        }

        g.setColour (colour);
        g.fillPath (triangle);
    }

    void bottomLine (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour)
    {
        g.setColour (colour);
        g.fillRect (area.getX(), area.getBottom() - 1, area.getWidth(), 1);
    }

    void waveBars (juce::Graphics& g, juce::Rectangle<float> area, const std::vector<float>& peaks,
                   int numBars, float gap, float minHeight, juce::Colour colour)
    {
        if (peaks.empty() || numBars <= 0)
            return;

        const float barWidth = (area.getWidth() - gap * (float) (numBars - 1)) / (float) numBars;
        g.setColour (colour);

        for (int i = 0; i < numBars; ++i)
        {
            // Maximum der Spitzen im Abschnitt des Balkens
            const auto from = (size_t) (i * (int) peaks.size() / numBars);
            const auto to = juce::jmax (from + 1, (size_t) ((i + 1) * (int) peaks.size() / numBars));
            float peak = 0.0f;
            for (auto p = from; p < to && p < peaks.size(); ++p)
                peak = juce::jmax (peak, peaks[p]);

            const float h = juce::jmax (minHeight, peak) * area.getHeight();
            g.fillRect (area.getX() + (float) i * (barWidth + gap), area.getCentreY() - h * 0.5f, barWidth, h);
        }
    }

    void levelMeter (juce::Graphics& g, juce::Rectangle<float> area, float level)
    {
        g.setColour (colours::sliderTrack);
        g.fillRect (area);

        if (level <= 0.0f)
            return;

        const float width = juce::jlimit (0.0f, 1.0f, level) * area.getWidth();
        g.setColour (level > 0.95f ? colours::warning : colours::accent);
        g.fillRect (area.withWidth (width));
    }

    void playIcon (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    {
        const auto r = area.withSizeKeepingCentre (8.0f, 9.0f);
        juce::Path p;
        p.addTriangle (r.getTopLeft(), r.getBottomLeft(), { r.getRight(), r.getCentreY() });
        g.setColour (colour);
        g.fillPath (p);
    }

    void stopIcon (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    {
        g.setColour (colour);
        g.fillRect (area.withSizeKeepingCentre (8.0f, 8.0f));
    }
} // namespace draw
} // namespace sis
