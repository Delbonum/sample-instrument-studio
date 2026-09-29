#include "TitleBar.h"

namespace sis
{
namespace
{
    constexpr int buttonWidth = 44;

    FlatButton::Style windowButtonStyle (juce::Colour hoverBg, juce::Colour hoverFg)
    {
        FlatButton::Style s;
        s.text = colours::windowText;
        s.hoverBackground = hoverBg;
        s.hoverText = hoverFg;
        return s;
    }
}

TitleBar::TitleBar (InstrumentModel& m)
    : model (m),
      icon (appIcon()),
      minimiseButton ("Minimieren", windowButtonStyle (colours::windowHover, {})),
      maximiseButton ("Maximieren", windowButtonStyle (colours::windowHover, {})),
      closeButton ("Schließen"_u, windowButtonStyle (colours::warning, colours::white))
{
    minimiseButton.setIcon ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c);
        g.fillRect (r.withSizeKeepingCentre (10.0f, 1.0f));
    });

    maximiseButton.setIcon ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c);
        g.drawRect (r.withSizeKeepingCentre (9.0f, 9.0f), 1.0f);
    });

    closeButton.setIcon ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        const auto x = r.withSizeKeepingCentre (9.0f, 9.0f);
        g.setColour (c);
        g.drawLine ({ x.getTopLeft(), x.getBottomRight() }, 1.1f);
        g.drawLine ({ x.getTopRight(), x.getBottomLeft() }, 1.1f);
    });

    minimiseButton.onClick = [this]
    {
        if (auto* w = getWindow())
            w->setMinimised (true);
    };

    maximiseButton.onClick = [this] { toggleMaximised(); };

    closeButton.onClick = []
    {
        if (auto* app = juce::JUCEApplicationBase::getInstance())
            app->systemRequestedQuit();
    };

    for (auto* b : { &minimiseButton, &maximiseButton, &closeButton })
        addAndMakeVisible (b);

    model.addChangeListener (this);
}

TitleBar::~TitleBar()
{
    model.removeChangeListener (this);
}

juce::ResizableWindow* TitleBar::getWindow() const
{
    return dynamic_cast<juce::ResizableWindow*> (getTopLevelComponent());
}

juce::String TitleBar::getTitle() const
{
    const auto file = model.projectFile.existsAsFile() ? model.projectFile.getFileName()
                                                      : model.name + ".sisp";

    // Der Programmname kommt aus dem Kennungsblock, nicht aus einer festen Zeichenkette:
    // eine exportierte App heißt wie ihr Instrument.
    return file + " — "_u + juce::String (JucePlugin_Name);
}

void TitleBar::toggleMaximised()
{
    if (auto* w = getWindow())
        w->setFullScreen (! w->isFullScreen());
}

void TitleBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::windowBar);
    draw::bottomLine (g, getLocalBounds(), colours::windowBarLine);

    auto area = getLocalBounds().withTrimmedBottom (1).withTrimmedRight (3 * buttonWidth);
    area.removeFromLeft (12);

    if (icon.isValid())
        g.drawImage (icon, area.removeFromLeft (17).withSizeKeepingCentre (17, 17).toFloat(),
                     juce::RectanglePlacement::centred);

    area.removeFromLeft (9);
    g.setColour (colours::windowText);
    g.setFont (sansFont (12.0f));
    g.drawText (getTitle(), area, juce::Justification::centredLeft, true);
}

void TitleBar::resized()
{
    auto area = getLocalBounds().withTrimmedBottom (1);
    closeButton.setBounds (area.removeFromRight (buttonWidth));
    maximiseButton.setBounds (area.removeFromRight (buttonWidth));
    minimiseButton.setBounds (area.removeFromRight (buttonWidth));
}

void TitleBar::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;

    if (auto* w = getWindow())
        if (! w->isFullScreen())
        {
            dragger.startDraggingComponent (w, e.getEventRelativeTo (w));
            dragging = true;
        }
}

void TitleBar::mouseDrag (const juce::MouseEvent& e)
{
    if (auto* w = getWindow(); w != nullptr && dragging)
        dragger.dragComponent (w, e.getEventRelativeTo (w), nullptr);
}

void TitleBar::mouseDoubleClick (const juce::MouseEvent&)
{
    toggleMaximised();
}

void TitleBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}
} // namespace sis
