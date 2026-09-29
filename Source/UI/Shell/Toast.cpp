#include "Toast.h"
#include "../Assets.h"
#include "../Theme.h"

namespace sis
{
Toast::Toast()
{
    setInterceptsMouseClicks (false, false);
    setAlwaysOnTop (true);
}

void Toast::show (const juce::String& text)
{
    message = text;
    updatePosition();
    setVisible (true);
    toFront (false);
    repaint();
    startTimer (metrics::toastMs);
}

void Toast::updatePosition()
{
    auto* parent = getParentComponent();
    if (parent == nullptr || message.isEmpty())
        return;

    const int boxWidth = juce::roundToInt (textWidth (sansFont (12.0f), message)) + 32;   // padding 10px 16px
    const int boxHeight = 36;
    const int boxBottom = parent->getHeight() - 52;                                        // bottom: 52px

    setBounds (parent->getWidth() / 2 - boxWidth / 2 - shadowMargin,
               boxBottom - boxHeight - shadowMargin,
               boxWidth + 2 * shadowMargin,
               boxHeight + 2 * shadowMargin);
}

void Toast::paint (juce::Graphics& g)
{
    const auto box = getLocalBounds().reduced (shadowMargin);

    // box-shadow: 0 10px 30px rgba(25,24,23,0.22)
    juce::DropShadow (colours::toast.withAlpha (0.22f), 26, { 0, 10 }).drawForRectangle (g, box);

    g.setColour (colours::toast);
    g.fillRect (box);
    g.setColour (colours::toastText);
    g.setFont (sansFont (12.0f));
    g.drawText (message, box, juce::Justification::centred, false);
}

void Toast::timerCallback()
{
    stopTimer();
    setVisible (false);
    message.clear();
}
} // namespace sis
