#include "MappingView.h"
#include "../../PluginProcessor.h"
#include "../SampleDrop.h"

namespace sis
{
namespace
{
    constexpr int padX = 16;
    constexpr int padY = 14;
    constexpr int headerHeight = 28;
    constexpr int axisWidth = 30;
    constexpr int axisGap = 8;
}

MappingView::MappingView (StudioContext& c)
    : ctx (c),
      grid (c),
      drums (c),
      keyboard (c.model, c.processor.getKeyboardState())
{
    FlatButton::Style style;
    style.background = colours::surface;
    style.hoverBackground = colours::white;
    style.border = colours::lineStrongAlt;
    style.text = colours::text;
    style.fontSize = 11.5f;
    addZoneButton.setStyle (style);
    addZoneButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::addZone, false); };
    addPieceButton.setStyle (style);
    addPieceButton.onClick = [this] { drums.showAddPieceMenu (addPieceButton); };

    // Ein Sample auf eine Taste: in ihre Zone, sonst eine neue darum herum
    keyboard.onSampleDropped = [this] (int note, int sampleIndex)
    {
        sampleDrop::ontoNote (ctx, note, -1, sampleIndex);
    };

    grid.onOpenZone = [this] (const juce::String&) { ctx.commands.invokeDirectly (cmd::showEditor, false); };
    drums.onOpenPart = [this] (const juce::String&) { ctx.commands.invokeDirectly (cmd::showEditor, false); };

    /* Der Schalter setzt die Art des Instruments um, nicht bloß die Ansicht – deshalb
       ein Schritt im Verlauf. */
    melodicButton.onClick = [this]
    {
        if (isDrumKit())
        {
            ctx.step ("Tonhöhen-Instrument"_u);
            ctx.model.setKind (InstrumentKind::melodic);
            ctx.toast ("Die Taste bestimmt wieder die Tonhöhe"_u);
        }
    };

    drumButton.onClick = [this]
    {
        if (! isDrumKit())
        {
            ctx.step ("Drumset");
            ctx.model.setKind (InstrumentKind::drumKit);
            ctx.toast ("Jedes Sample klingt jetzt in seiner eigenen Tonhöhe"_u);
        }
    };

    keyboard.onHighlightChanged = [this] (int note, int velocity)
    {
        ctx.ui.heldNote = note;
        ctx.ui.heldVelocity = velocity;
        ctx.ui.changed();
    };

    addAndMakeVisible (addZoneButton);
    addChildComponent (addPieceButton);
    addAndMakeVisible (melodicButton);
    addAndMakeVisible (drumButton);
    addAndMakeVisible (grid);
    addChildComponent (drums);
    addAndMakeVisible (keyboard);

    updateStyles();
    ctx.ui.addChangeListener (this);
    ctx.model.addChangeListener (this);
}

bool MappingView::isDrumKit() const
{
    return ctx.model.kind == InstrumentKind::drumKit;
}

void MappingView::updateStyles()
{
    auto segment = [] (bool active)
    {
        FlatButton::Style s;
        s.fontSize = 11.0f;
        s.background = active ? colours::surface : juce::Colours::transparentBlack;
        s.text = active ? colours::text : colours::textSecondary;
        return s;
    };

    melodicButton.setStyle (segment (! isDrumKit()));
    drumButton.setStyle (segment (isDrumKit()));

    grid.setVisible (! isDrumKit());
    drums.setVisible (isDrumKit());

    // Beim Drumset gibt es keine Velocity-Achse: ein Kit-Teil sitzt auf einer Taste
    addZoneButton.setVisible (! isDrumKit());
    addPieceButton.setVisible (isDrumKit());
}

MappingView::~MappingView()
{
    ctx.model.removeChangeListener (this);
    ctx.ui.removeChangeListener (this);
}

void MappingView::paint (juce::Graphics& g)
{
    g.fillAll (colours::workspace);

    // Kopfzeile: Titel, Hinweis
    auto header = headerArea;
    const auto titleFont = sansFont (13.0f, Weight::semibold);
    const juce::String title (isDrumKit() ? "Schlagzeug" : "Tastatur-Zonen");
    const int titleWidth = juce::roundToInt (textWidth (titleFont, title)) + 2;

    g.setColour (colours::text);
    g.setFont (titleFont);
    g.drawText (title, header.removeFromLeft (titleWidth), juce::Justification::centredLeft, false);
    header.removeFromLeft (12);
    header.setRight (juce::jmax (header.getX(), segmentArea.getX() - 12));

    g.setColour (colours::textTertiary);
    g.setFont (monoFont (10.0f));
    g.drawText (isDrumKit() ? "Klick schlägt an · Doppelklick öffnet den Editor · Rechtsklick entfernt · Samples daraufziehen"_u
                            : "Klick wählt · Doppelklick öffnet den Editor · Samples auf Zone oder Taste ziehen"_u,
                header, juce::Justification::centredLeft, true);

    // Rahmen des Segmentschalters
    g.setColour (colours::lineFine);
    g.fillRect (segmentArea);
    g.setColour (colours::line);
    g.drawRect (segmentArea, 1);

    // Velocity-Achse
    if (ctx.ui.velocityLayers && ! isDrumKit() && ! axisArea.isEmpty())
    {
        const auto axis = axisArea.reduced (0, 2);
        g.setColour (colours::textTertiary);
        g.setFont (monoFont (10.0f));
        g.drawText ("127", axis.withHeight (14), juce::Justification::topRight, false);
        g.drawText ("64", axis.withSizeKeepingCentre (axis.getWidth(), 14), juce::Justification::centredRight, false);
        g.drawText ("0", axis.withTop (axis.getBottom() - 14), juce::Justification::bottomRight, false);
    }
}

void MappingView::resized()
{
    auto area = getLocalBounds().reduced (padX, padY);

    headerArea = area.removeFromTop (headerHeight);

    auto header = headerArea;

    {
        auto& button = isDrumKit() ? addPieceButton : addZoneButton;
        const int buttonWidth = button.getTextWidth() + 22;   // padding 6px 11px
        button.setBounds (header.removeFromRight (buttonWidth));
        header.removeFromRight (10);
    }

    const int segmentWidth = melodicButton.getTextWidth() + drumButton.getTextWidth() + 44;
    segmentArea = header.removeFromRight (segmentWidth).withSizeKeepingCentre (segmentWidth, headerHeight - 2);
    auto inner = segmentArea.reduced (3);
    melodicButton.setBounds (inner.removeFromLeft (melodicButton.getTextWidth() + 20));
    drumButton.setBounds (inner);

    area.removeFromTop (10);

    auto keyboardRow = area.removeFromBottom (12 + metrics::keyboard);
    keyboardRow.removeFromTop (12);

    const bool showAxis = ctx.ui.velocityLayers && ! isDrumKit();
    if (showAxis)
    {
        axisArea = area.removeFromLeft (axisWidth);
        area.removeFromLeft (axisGap);
        keyboardRow.removeFromLeft (axisWidth + axisGap);
    }
    else
    {
        axisArea = {};
    }

    grid.setBounds (area);
    drums.setBounds (area);
    keyboard.setBounds (keyboardRow);
}

void MappingView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateStyles();
    resized();
    repaint();
}
} // namespace sis
