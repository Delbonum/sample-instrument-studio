#include "ZoneSheet.h"

namespace sis
{
namespace
{
    constexpr int pad = 13;
    constexpr int nameWidth = 104;
    constexpr int gainWidth = 52;
    constexpr int muteWidth = 21;
}

ZoneSheet::ZoneSheet (StudioContext& c) : ctx (c), editor (c)
{
    FlatButton::Style plain;
    plain.background = juce::Colours::transparentBlack;
    plain.border = juce::Colours::transparentBlack;
    plain.text = colours::textSecondary;
    plain.hoverBackground = colours::rowHover;
    plain.fontSize = 15.0f;

    closeButton.setStyle (plain);
    closeButton.onClick = [this] { close(); };

    FlatButton::Style accent;
    accent.background = colours::accent;
    accent.border = colours::accent;
    accent.text = colours::white;
    accent.hoverBackground = colours::accentHover;
    accent.fontSize = 11.5f;

    studioButton.setStyle (accent);
    studioButton.onClick = [this]
    {
        if (onOpenStudio != nullptr)
            onOpenStudio();
    };

    addAndMakeVisible (editor);
    addAndMakeVisible (closeButton);
    addAndMakeVisible (studioButton);

    ctx.model.addChangeListener (this);
}

ZoneSheet::~ZoneSheet()
{
    ctx.model.removeChangeListener (this);
}

void ZoneSheet::changeListenerCallback (juce::ChangeBroadcaster*)
{
    /* Die Spurenliste wächst und schrumpft mit dem Instrument, also neu aufteilen und
       nicht nur neu zeichnen. */
    resized();
    repaint();
}

void ZoneSheet::close()
{
    setVisible (false);

    if (auto* parent = getParentComponent())
        parent->grabKeyboardFocus();
}

void ZoneSheet::resized()
{
    const auto* zone = ctx.model.getSelectedZone();
    const int numTracks = zone != nullptr ? (int) zone->tracks.size() : 0;

    const int wanted = 44                                  // Kopf
                     + pad + numTracks * rowHeight         // Spuren
                     + pad + editorHeight                  // Sample-Editor
                     + pad + 32 + pad;                     // Fußzeile

    const int width = juce::jmin (cardMaxWidth, getWidth() - 2 * 14);
    const int height = juce::jmin (wanted, getHeight() - 2 * 14);

    card = getLocalBounds().withSizeKeepingCentre (juce::jmax (width, 0), juce::jmax (height, 0));

    auto inner = card;
    headerArea = inner.removeFromTop (44);
    closeButton.setBounds (headerArea.reduced (pad, 0).removeFromRight (22).withSizeKeepingCentre (22, 22));

    footerArea = inner.removeFromBottom (pad + 32 + pad);
    studioButton.setBounds (footerArea.reduced (pad, 0).removeFromRight (150).withSizeKeepingCentre (150, 32));

    editor.setBounds (inner.removeFromBottom (editorHeight).reduced (pad, 0));
    inner.removeFromBottom (pad);

    inner.removeFromTop (pad);
    rowAreas.clear();
    muteAreas.clear();

    for (int i = 0; i < numTracks; ++i)
    {
        auto row = inner.removeFromTop (rowHeight).reduced (pad, 2);
        rowAreas.push_back (row);
        muteAreas.push_back (row.reduced (9, 0).removeFromRight (muteWidth).withSizeKeepingCentre (muteWidth, 18));
    }
}

void ZoneSheet::paintTrackRow (juce::Graphics& g, const Track& track, int index,
                               juce::Rectangle<int> row) const
{
    const auto* zone = ctx.model.getSelectedZone();
    const bool selected = zone != nullptr && zone->selectedTrack == index;

    g.setColour (selected ? colours::accentSoft : colours::surface);
    g.fillRect (row);
    g.setColour (selected ? colours::accent : colours::line);
    g.drawRect (row, 1);

    auto inner = row.reduced (9, 0);

    g.setColour (track.colour);
    g.fillRect (inner.removeFromLeft (8).withSizeKeepingCentre (8, 8));
    inner.removeFromLeft (9);

    g.setColour (colours::text);
    g.setFont (sansFont (11.5f));
    g.drawText (track.name, inner.removeFromLeft (nameWidth), juce::Justification::centredLeft, true);
    inner.removeFromLeft (9);

    // M-Knopf ganz rechts, davor der Pegel
    auto mute = inner.removeFromRight (muteWidth).withSizeKeepingCentre (muteWidth, 18);
    g.setColour (track.mute ? colours::warning : colours::surface);
    g.fillRect (mute);
    g.setColour (colours::line);
    g.drawRect (mute, 1);
    g.setColour (track.mute ? colours::white : colours::textSecondary);
    g.setFont (monoFont (10.0f));
    g.drawText ("M", mute, juce::Justification::centred, false);

    inner.removeFromRight (9);
    g.setColour (colours::textSecondary);
    g.setFont (monoFont (10.0f));
    g.drawText (juce::String (juce::roundToInt (track.gain * 100.0f)) + " %",
                inner.removeFromRight (gainWidth), juce::Justification::centredRight, false);
    inner.removeFromRight (9);

    if (const auto* sample = ctx.model.findSample (track.clip))
        draw::waveBars (g, inner.withSizeKeepingCentre (inner.getWidth(), 20).toFloat(),
                        sample->peaks, 30, 1.0f, 0.08f,
                        track.mute ? colours::waveInactive : track.colour);
}

void ZoneSheet::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0x47191817));   // rgba(25,24,23,0.28)

    g.setColour (colours::panel);
    g.fillRect (card);
    g.setColour (juce::Colour (0xff8a867f));
    g.drawRect (card, 1);

    const auto* zone = ctx.model.getSelectedZone();

    // Kopf
    draw::bottomLine (g, headerArea, colours::line);

    auto h = headerArea.reduced (pad, 0);

    if (zone != nullptr)
    {
        g.setColour (zone->colour);
        g.fillRect (h.removeFromLeft (10).withSizeKeepingCentre (10, 10));
        h.removeFromLeft (10);

        g.setColour (colours::text);
        g.setFont (sansFont (12.5f, Weight::semibold));
        const int nameSpace = juce::jmin (260, h.getWidth());
        g.drawText ("Zone "_u + zone->name, h.removeFromLeft (nameSpace),
                    juce::Justification::centredLeft, true);
        h.removeFromLeft (10);

        g.setColour (colours::textTertiary);
        g.setFont (monoFont (10.0f));
        g.drawText (noteName (zone->lowNote) + "–"_u + noteName (zone->highNote),
                    h.withTrimmedRight (30), juce::Justification::centredLeft, false);
    }

    // Spuren
    if (zone != nullptr)
        for (size_t i = 0; i < rowAreas.size() && i < zone->tracks.size(); ++i)
            paintTrackRow (g, zone->tracks[i], (int) i, rowAreas[i]);

    // Fußzeile
    g.setColour (colours::textTertiary);
    g.setFont (monoFont (10.0f));
    g.drawFittedText ("Spuren anlegen, Fades ziehen und Effekte: im Standalone-Studio"_u,
                      footerArea.reduced (pad, 0).withTrimmedRight (160),
                      juce::Justification::centredLeft, 2);
}

void ZoneSheet::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;

    // Klick neben die Karte schließt
    if (! card.contains (e.getPosition()))
    {
        close();
        return;
    }

    auto* zone = ctx.model.getSelectedZone();

    if (zone == nullptr)
        return;

    for (size_t i = 0; i < rowAreas.size() && i < zone->tracks.size(); ++i)
    {
        if (! rowAreas[i].contains (e.getPosition()))
            continue;

        if (muteAreas[i].contains (e.getPosition()))
        {
            ctx.step (zone->tracks[i].mute ? "Spur hörbar"_u : "Spur stumm"_u);
            zone->tracks[i].mute = ! zone->tracks[i].mute;
        }
        else
        {
            zone->selectedTrack = (int) i;
        }

        ctx.model.notifyChanged();
        return;
    }
}
} // namespace sis
