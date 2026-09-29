#include "ZonePanel.h"

namespace sis
{
namespace
{
    constexpr int pad = 12;
    constexpr int fieldHeight = 46;      // 7 + Label 13 + 3 + Wert 16 + 7
    constexpr int sectionTitle = 13;
    constexpr int rowHeight = 13;
    constexpr int rowGap = 7;
    constexpr int macroRowHeight = 16;

    constexpr int stepperHeight = 24;
    constexpr int stepperRowGap = 8;
    constexpr int zoneLabelWidth = 62;
    constexpr int zoneSectionHeight = pad + 18 + 11 + 4 * stepperHeight + 3 * stepperRowGap + pad;
    constexpr int envelopeSectionHeight = pad + sectionTitle + 10 + 56 + 10 + 4 * (rowHeight + rowGap) - rowGap + pad;
    constexpr int macroSectionHeight = pad + sectionTitle + 10 + 4 * (macroRowHeight + rowGap) - rowGap + pad;

}

ZonePanelContent::ZonePanelContent (StudioContext& c) : ctx (c)
{
    const char* const envLabels[] = { "A", "D", "S", "R" };

    for (int i = 0; i < 4; ++i)
    {
        auto& env = envelopeRows[(size_t) i];
        env.label = envLabels[i];
        env.bar.onValueChange = [this, i] (float v)
        {
            ctx.step ("Hüllkurve geändert"_u);
            auto& e = ctx.model.envelope;
            (i == 0 ? e.attack : i == 1 ? e.decay : i == 2 ? e.sustain : e.release) = v;
            ctx.model.notifyChanged();
        };
        addAndMakeVisible (env.bar);

        auto& macro = macroRows[(size_t) i];
        macro.label = macroName (i);
        macro.bar.onValueChange = [this, i] (float v)
        {
            ctx.step ("Makro geändert"_u);
            ctx.model.macros[(size_t) i] = v;
            ctx.model.applyMacro (i);
            ctx.model.notifyChanged();
        };
        addAndMakeVisible (macro.bar);
    }

    rootStepper.onStep = [this] (int delta) { stepRoot (delta); };
    lowNoteStepper.onStep = [this] (int delta) { stepRange (true, delta); };
    highNoteStepper.onStep = [this] (int delta) { stepRange (false, delta); };
    lowVelocityStepper.onStep = [this] (int delta) { stepVelocity (true, delta); };
    highVelocityStepper.onStep = [this] (int delta) { stepVelocity (false, delta); };

    for (auto* stepper : { &rootStepper, &lowNoteStepper, &highNoteStepper,
                           &lowVelocityStepper, &highVelocityStepper })
        addAndMakeVisible (stepper);

    ctx.model.addChangeListener (this);
    updateFromModel();
}

void ZonePanelContent::stepRoot (int delta)
{
    if (auto* zone = ctx.model.getSelectedZone())
    {
        ctx.step ("Grundton geändert"_u);
        zone->rootNote = juce::jlimit (0, 127, zone->rootNote + delta);
        ctx.model.notifyChanged();
    }
}

void ZonePanelContent::stepRange (bool lowEnd, int delta)
{
    auto* zone = ctx.model.getSelectedZone();

    if (zone == nullptr)
        return;

    ctx.step ("Tastenbereich geändert"_u);

    if (lowEnd)
        zone->lowNote = juce::jlimit (0, zone->highNote, zone->lowNote + delta);
    else
        zone->highNote = juce::jlimit (zone->lowNote, 127, zone->highNote + delta);

    ctx.model.notifyChanged();
}

void ZonePanelContent::stepVelocity (bool lowEnd, int delta)
{
    auto* zone = ctx.model.getSelectedZone();

    if (zone == nullptr)
        return;

    ctx.step ("Velocity-Bereich geändert"_u);

    if (lowEnd)
        zone->lowVelocity = juce::jlimit (0, zone->highVelocity, zone->lowVelocity + delta);
    else
        zone->highVelocity = juce::jlimit (zone->lowVelocity, 127, zone->highVelocity + delta);

    ctx.model.notifyChanged();
}

ZonePanelContent::~ZonePanelContent()
{
    ctx.model.removeChangeListener (this);
}

int ZonePanelContent::getIdealHeight() const
{
    // Eingeklappt bleibt nur die Überschrift stehen
    const int envelope = ctx.ui.showEnvelopeSection ? envelopeSectionHeight : pad + sectionTitle + pad;
    const int macros = ctx.ui.showMacroSection ? macroSectionHeight : pad + sectionTitle + pad;

    return metrics::panelHeader + zoneSectionHeight + envelope + macros;
}

void ZonePanelContent::updateFromModel()
{
    const auto& e = ctx.model.envelope;
    const float env[] = { e.attack, e.decay, e.sustain, e.release };

    for (size_t i = 0; i < 4; ++i)
    {
        envelopeRows[i].bar.setValue (env[i]);
        macroRows[i].bar.setValue (ctx.model.macros[i]);
    }

    const auto* zone = ctx.model.getSelectedZone();

    for (auto* stepper : { &rootStepper, &lowNoteStepper, &highNoteStepper,
                           &lowVelocityStepper, &highVelocityStepper })
        stepper->setEnabledState (zone != nullptr);

    if (zone != nullptr)
    {
        rootStepper.setText (noteName (zone->rootNote));
        lowNoteStepper.setText (noteName (zone->lowNote));
        highNoteStepper.setText (noteName (zone->highNote));
        lowVelocityStepper.setText (juce::String (zone->lowVelocity));
        highVelocityStepper.setText (juce::String (zone->highVelocity));
    }

    repaint();
}

void ZonePanelContent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateFromModel();
}

void ZonePanelContent::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (metrics::panelHeader);
    zoneSection = area.removeFromTop (zoneSectionHeight);
    const bool showEnvelope = ctx.ui.showEnvelopeSection;
    const bool showMacros = ctx.ui.showMacroSection;

    envelopeSection = area.removeFromTop (showEnvelope ? envelopeSectionHeight : pad + sectionTitle + pad);
    macroSection = area.removeFromTop (showMacros ? macroSectionHeight : pad + sectionTitle + pad);

    for (auto& row : envelopeRows)
        row.bar.setVisible (showEnvelope);

    for (auto& row : macroRows)
        row.bar.setVisible (showMacros);

    // Zone: Name, darunter Grundton, Tastenbereich, Velocity und die Sample-Zahl
    {
        auto r = zoneSection.reduced (pad);
        titleArea = r.removeFromTop (18);
        r.removeFromTop (11);

        auto row = r.removeFromTop (stepperHeight);
        rootLabel = row.removeFromLeft (zoneLabelWidth);
        rootStepper.setBounds (row.removeFromLeft (juce::jmin (96, row.getWidth())));
        r.removeFromTop (stepperRowGap);

        row = r.removeFromTop (stepperHeight);
        rangeLabel = row.removeFromLeft (zoneLabelWidth);
        const int halfWidth = (row.getWidth() - 8) / 2;
        lowNoteStepper.setBounds (row.removeFromLeft (halfWidth));
        row.removeFromLeft (8);
        highNoteStepper.setBounds (row.removeFromLeft (halfWidth));
        r.removeFromTop (stepperRowGap);

        row = r.removeFromTop (stepperHeight);
        velocityLabel = row.removeFromLeft (zoneLabelWidth);
        lowVelocityStepper.setBounds (row.removeFromLeft (halfWidth));
        row.removeFromLeft (8);
        highVelocityStepper.setBounds (row.removeFromLeft (halfWidth));
        r.removeFromTop (stepperRowGap);

        row = r.removeFromTop (stepperHeight);
        samplesLabel = row.removeFromLeft (zoneLabelWidth);
        samplesValue = row;
    }

    // Hüllkurve
    {
        auto r = envelopeSection.reduced (pad);
        envelopeTitleArea = r.removeFromTop (sectionTitle);
        r.removeFromTop (10);
        envelopeArea = {};

        if (showEnvelope)
        {
            envelopeArea = r.removeFromTop (56);
            r.removeFromTop (10);

            for (auto& row : envelopeRows)
            {
                auto line = r.removeFromTop (rowHeight);
                row.labelArea = line.removeFromLeft (26);
                line.removeFromLeft (8);
                row.valueArea = line.removeFromRight (52);
                line.removeFromRight (8);
                row.bar.setBounds (line.withSizeKeepingCentre (line.getWidth(), 11));
                r.removeFromTop (rowGap);
            }
        }
    }

    // Makros
    {
        auto r = macroSection.reduced (pad);
        macroTitleArea = r.removeFromTop (sectionTitle);
        r.removeFromTop (10);

        if (showMacros)
        {
            for (auto& row : macroRows)
            {
                auto line = r.removeFromTop (macroRowHeight);
                row.valueArea = line.removeFromRight (30);
                line.removeFromRight (8);
                row.bar.setBounds (line.removeFromRight (100).withSizeKeepingCentre (100, 11));
                line.removeFromRight (8);
                row.labelArea = line;
                r.removeFromTop (rowGap);
            }
        }
    }
}

void ZonePanelContent::paintMacroTitle (juce::Graphics& g) const
{
    auto title = macroTitleArea;
    draw::disclosure (g, title.removeFromLeft (10), ctx.ui.showMacroSection, colours::textTertiary);
    title.removeFromLeft (4);
    draw::capsLabel (g, "Makros am Instrument", title);
}

void ZonePanelContent::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;

    // Ein Klick auf die Überschrift klappt den Abschnitt zu oder auf
    if (envelopeTitleArea.expanded (0, 4).contains (e.getPosition()))
    {
        ctx.ui.showEnvelopeSection = ! ctx.ui.showEnvelopeSection;
        ctx.ui.changed();
    }
    else if (macroTitleArea.expanded (0, 4).contains (e.getPosition()))
    {
        ctx.ui.showMacroSection = ! ctx.ui.showMacroSection;
        ctx.ui.changed();
    }
}

juce::Path ZonePanelContent::createEnvelopePath (juce::Rectangle<float> r) const
{
    // Wie clip-path im Prototyp: Punkte in Prozent der Fläche
    const auto& e = ctx.model.envelope;
    const float eA = e.attack * 24.0f;
    const float eD = e.decay * 26.0f;
    const float eS = (1.0f - e.sustain) * 88.0f;
    const float eSus = 72.0f;
    const float eR = juce::jmin (100.0f, eSus + e.release * 28.0f);

    auto pt = [r] (float xPct, float yPct)
    {
        return juce::Point<float> (r.getX() + r.getWidth() * xPct / 100.0f, r.getY() + r.getHeight() * yPct / 100.0f);
    };

    juce::Path p;
    p.startNewSubPath (pt (0.0f, 100.0f));
    p.lineTo (pt (eA, 4.0f));
    p.lineTo (pt (eA + eD, eS));
    p.lineTo (pt (eSus, eS));
    p.lineTo (pt (eR, 100.0f));
    p.closeSubPath();
    return p;
}

void ZonePanelContent::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    draw::bottomLine (g, headerArea, colours::lineFine);
    draw::capsLabel (g, "Zone", headerArea.reduced (pad, 0), 10.5f);

    draw::bottomLine (g, zoneSection, colours::lineFine);
    draw::bottomLine (g, envelopeSection, colours::lineFine);

    // Zone
    if (const auto* zone = ctx.model.getSelectedZone())
    {
        auto t = titleArea;
        g.setColour (zone->colour);
        g.fillRect (t.removeFromLeft (10).withSizeKeepingCentre (10, 10));
        t.removeFromLeft (8);
        g.setColour (colours::text);
        g.setFont (sansFont (13.0f, Weight::semibold));
        g.drawText (zone->name, t, juce::Justification::centredLeft, true);

        draw::capsLabel (g, "Grundton", rootLabel, 10.0f, 0.08f);
        draw::capsLabel (g, "Bereich", rangeLabel, 10.0f, 0.08f);
        draw::capsLabel (g, "Velocity", velocityLabel, 10.0f, 0.08f);
        draw::capsLabel (g, "Samples", samplesLabel, 10.0f, 0.08f);

        g.setColour (colours::text);
        g.setFont (monoFont (12.0f));
        g.drawText (juce::String (zone->numSamples()), samplesValue, juce::Justification::centredLeft, false);
    }
    else
    {
        g.setColour (colours::textTertiary);
        g.setFont (sansFont (12.0f));
        g.drawText ("Keine Zone gewählt"_u, zoneSection.reduced (pad), juce::Justification::topLeft, true);
    }

    // Hüllkurve
    {
        auto title = envelopeTitleArea;
        draw::disclosure (g, title.removeFromLeft (10), ctx.ui.showEnvelopeSection, colours::textTertiary);
        title.removeFromLeft (4);
        draw::capsLabel (g, "Hüllkurve"_u, title);
    }

    if (! ctx.ui.showEnvelopeSection)
    {
        paintMacroTitle (g);
        return;
    }
    g.setColour (colours::surface);
    g.fillRect (envelopeArea);
    g.setColour (colours::lineFine);
    g.drawRect (envelopeArea, 1);
    g.setColour (colours::accent.withAlpha (0.22f));
    g.fillPath (createEnvelopePath (envelopeArea.reduced (1).toFloat()));

    const auto& e = ctx.model.envelope;
    const juce::String envTexts[] = { juce::String (juce::roundToInt (e.attack * 2000.0f)) + " ms",
                                      juce::String (juce::roundToInt (e.decay * 3000.0f)) + " ms",
                                      gainToText (e.sustain),
                                      juce::String (juce::roundToInt (e.release * 5000.0f)) + " ms" };

    g.setFont (monoFont (10.0f));
    for (size_t i = 0; i < envelopeRows.size(); ++i)
    {
        const auto& row = envelopeRows[i];
        g.setColour (colours::textSecondary);
        g.drawText (row.label, row.labelArea, juce::Justification::centredLeft, false);
        g.drawText (envTexts[i], row.valueArea, juce::Justification::centredRight, false);
    }

    // Makros
    paintMacroTitle (g);

    if (! ctx.ui.showMacroSection)
        return;

    for (size_t i = 0; i < macroRows.size(); ++i)
    {
        const auto& row = macroRows[i];
        g.setColour (colours::text);
        g.setFont (sansFont (11.5f));
        g.drawText (row.label, row.labelArea, juce::Justification::centredLeft, true);

        g.setColour (colours::textSecondary);
        g.setFont (monoFont (10.0f));
        g.drawText (juce::String (juce::roundToInt (ctx.model.macros[i] * 100.0f)), row.valueArea,
                    juce::Justification::centredRight, false);
    }
}

//==============================================================================
ZonePanel::ZonePanel (StudioContext& c) : ctx (c), content (c)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    ctx.ui.addChangeListener (this);
}

ZonePanel::~ZonePanel()
{
    ctx.ui.removeChangeListener (this);
}

void ZonePanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    resized();
    repaint();
}

void ZonePanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::line);
    g.fillRect (0, 0, 1, getHeight());   // border-left
}

void ZonePanel::resized()
{
    viewport.setBounds (getLocalBounds().withTrimmedLeft (1));
    const int h = content.getIdealHeight();
    const bool needsScroll = h > viewport.getHeight();
    content.setSize (viewport.getWidth() - (needsScroll ? viewport.getScrollBarThickness() : 0),
                     juce::jmax (h, viewport.getHeight()));
}
} // namespace sis
