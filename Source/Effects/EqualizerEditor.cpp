#include "EqualizerEditor.h"

namespace sis
{
namespace
{
    constexpr int pad = 14;
    constexpr int stripHeight = 150;
    constexpr int rowHeight = 16;
    constexpr int rowGap = 8;

    // Dieselben vier Farben wie die Spuren im Studio
    const std::array<juce::Colour, 4> bandColours {
        juce::Colour (0xff7a6cf0), juce::Colour (0xff2e9e96),
        juce::Colour (0xffc0862e), juce::Colour (0xffb4553f)
    };

    /** Frequenz auf die Kurvenbreite abbilden (logarithmisch, 20 Hz … 20 kHz). */
    double frequencyToProportion (double frequency)
    {
        return std::log10 (juce::jlimit (20.0, 20000.0, frequency) / 20.0) / std::log10 (1000.0);
    }

    double proportionToFrequency (double proportion)
    {
        return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, proportion));
    }
} // namespace

//==============================================================================
BandStrip::BandStrip (EqualizerProcessor& p, int bandIndex, const juce::String& bandTitle, juce::Colour bandColour)
    : processor (p), index (bandIndex), title (bandTitle), colour (bandColour)
{
    auto& state = processor.getState();

    FlatButton::Style power;
    power.mono = true;
    power.fontSize = 10.0f;
    power.background = colours::surface;
    power.border = colours::line;
    power.text = colours::textSecondary;
    powerButton.setStyle (power);
    powerButton.setClickingTogglesState (true);
    addAndMakeVisible (powerButton);

    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, EqualizerProcessor::parameterId (index, "on"), powerButton);

    powerButton.onStateChange = [this]
    {
        FlatButton::Style style = powerButton.getStyle();
        const bool on = powerButton.getToggleState();
        style.background = on ? colours::accentSoft : colours::surface;
        style.text = on ? colours::accentDark : colours::textTertiary;
        style.border = on ? colours::accent : colours::line;
        powerButton.setStyle (style);
    };

    typeBox.addItemList ({ "Kuhschwanz tief", "Glocke", "Kuhschwanz hoch", "Hochpass", "Tiefpass" }, 1);
    addAndMakeVisible (typeBox);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, EqualizerProcessor::parameterId (index, "type"), typeBox);

    setUpRow (rows[0], "freq", "FREQ", " Hz");
    setUpRow (rows[1], "gain", "GAIN", " dB");
    setUpRow (rows[2], "q", "GÜTE"_u, {});

    powerButton.onStateChange();
}

void BandStrip::setUpRow (Row& row, const juce::String& parameterName, const juce::String& label,
                          const juce::String& suffix)
{
    row.label = label;
    row.suffix = suffix;
    row.parameter = processor.getState().getParameter (EqualizerProcessor::parameterId (index, parameterName));

    if (row.parameter == nullptr)
        return;

    row.bar.setFillColour (colour);
    addAndMakeVisible (row.bar);

    row.attachment = std::make_unique<juce::ParameterAttachment> (
        *row.parameter,
        [this, &row] (float newValue)
        {
            row.bar.setValue (row.parameter->convertTo0to1 (newValue));
            repaint (row.valueArea);
        },
        nullptr);

    row.bar.onDragStart = [&row] { row.attachment->beginGesture(); };
    row.bar.onDragEnd = [&row] { row.attachment->endGesture(); };
    row.bar.onValueChange = [&row] (float proportion)
    {
        row.attachment->setValueAsPartOfGesture (row.parameter->convertFrom0to1 (proportion));
    };

    row.attachment->sendInitialUpdate();
}

juce::String BandStrip::formatValue (const Row& row) const
{
    if (row.parameter == nullptr)
        return {};

    const float value = row.parameter->convertFrom0to1 (row.bar.getValue());

    if (row.suffix == " Hz")
        return value >= 1000.0f ? juce::String (value / 1000.0f, 2) + " kHz"
                                : juce::String (juce::roundToInt (value)) + " Hz";

    if (row.suffix == " dB")
        return (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";

    return juce::String (value, 2);
}

void BandStrip::refresh()
{
    repaint();
}

void BandStrip::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto head = area.removeFromTop (20);
    powerButton.setBounds (head.removeFromRight (34).withSizeKeepingCentre (34, 18));
    head.removeFromRight (8);
    titleArea = head;

    area.removeFromTop (8);
    typeBox.setBounds (area.removeFromTop (26));
    area.removeFromTop (rowGap);

    for (auto& row : rows)
    {
        auto line = area.removeFromTop (rowHeight);
        row.labelArea = line.removeFromLeft (38);
        line.removeFromLeft (6);
        row.valueArea = line.removeFromRight (62);
        line.removeFromRight (6);
        row.bar.setBounds (line);
        area.removeFromTop (rowGap);
    }
}

void BandStrip::paint (juce::Graphics& g)
{
    g.setColour (colours::surface);
    g.fillRect (getLocalBounds());
    g.setColour (colours::line);
    g.drawRect (getLocalBounds(), 1);

    auto swatch = titleArea.removeFromLeft (9).withSizeKeepingCentre (9, 9);
    g.setColour (colour);
    g.fillRect (swatch);
    titleArea.removeFromLeft (7);

    g.setColour (colours::text);
    g.setFont (sansFont (12.0f, Weight::medium));
    g.drawText (title, titleArea, juce::Justification::centredLeft, true);

    g.setFont (monoFont (10.0f));

    for (const auto& row : rows)
    {
        g.setColour (colours::textSecondary);
        g.drawText (row.label, row.labelArea, juce::Justification::centredLeft, false);
        g.setColour (colours::text);
        g.drawText (formatValue (row), row.valueArea, juce::Justification::centredRight, false);
    }
}

//==============================================================================
EqualizerEditor::EqualizerEditor (EqualizerProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    const juce::String titles[] = { "Tiefen", "Tiefmitten", "Hochmitten", "Höhen"_u };

    for (int i = 0; i < dsp::Equalizer::numBands; ++i)
    {
        strips[(size_t) i] = std::make_unique<BandStrip> (processor, i, titles[i], bandColours[(size_t) i]);
        addAndMakeVisible (*strips[(size_t) i]);
    }

    if (auto* output = processor.getState().getParameter ("output"))
    {
        outputBar.setFillColour (colours::accent);
        outputBar.setTrackHeight (6.0f);
        addAndMakeVisible (outputBar);

        outputAttachment = std::make_unique<juce::ParameterAttachment> (
            *output,
            [this, output] (float newValue)
            {
                outputBar.setValue (output->convertTo0to1 (newValue));
                repaint (outputValueArea);
            },
            nullptr);

        outputBar.onDragStart = [this] { outputAttachment->beginGesture(); };
        outputBar.onDragEnd = [this] { outputAttachment->endGesture(); };
        outputBar.onValueChange = [this, output] (float proportion)
        {
            outputAttachment->setValueAsPartOfGesture (output->convertFrom0to1 (proportion));
        };

        outputAttachment->sendInitialUpdate();
    }

    setSize (760, 470);
    startTimerHz (20);
}

EqualizerEditor::~EqualizerEditor()
{
    setLookAndFeel (nullptr);
}

void EqualizerEditor::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (34);
    footerArea = area.removeFromBottom (40);

    area = area.reduced (pad, 0);
    curveArea = area.removeFromTop (area.getHeight() - stripHeight - pad).reduced (0, pad / 2);
    area.removeFromTop (pad / 2);

    auto strip = area.removeFromTop (stripHeight);
    const int width = (strip.getWidth() - 3 * 8) / 4;

    for (int i = 0; i < dsp::Equalizer::numBands; ++i)
    {
        strips[(size_t) i]->setBounds (strip.removeFromLeft (width));
        strip.removeFromLeft (8);
    }

    auto footer = footerArea.reduced (pad, 8);
    meterArea = footer.removeFromRight (120).withSizeKeepingCentre (120, 6);
    footer.removeFromRight (12);
    outputValueArea = footer.removeFromRight (72);
    footer.removeFromRight (8);
    outputBar.setBounds (footer.removeFromRight (140).withSizeKeepingCentre (140, 14));
    footer.removeFromRight (8);
    outputLabelArea = footer;
}

juce::Path EqualizerEditor::createResponsePath (juce::Rectangle<float> area) const
{
    juce::Path path;
    const float maxDb = 24.0f;
    const int steps = juce::jmax (2, (int) area.getWidth());

    for (int i = 0; i < steps; ++i)
    {
        const double proportion = (double) i / (double) (steps - 1);
        const double frequency = proportionToFrequency (proportion);
        const double magnitude = processor.getMagnitudeAt (frequency);
        const auto decibels = (float) juce::Decibels::gainToDecibels (magnitude, -60.0);
        const float y = area.getCentreY() - juce::jlimit (-maxDb, maxDb, decibels) / maxDb * (area.getHeight() * 0.5f);
        const float x = area.getX() + (float) proportion * area.getWidth();

        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    return path;
}

void EqualizerEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // Kopfzeile
    g.setColour (colours::bars);
    g.fillRect (headerArea);
    draw::bottomLine (g, headerArea, colours::line);
    draw::capsLabel (g, "SIS Equalizer", headerArea.reduced (pad, 0), 10.5f);
    g.setColour (colours::textTertiary);
    g.setFont (monoFont (10.0f));
    g.drawText ("4 Bänder · Teil von Sample Instrument Studio"_u, headerArea.reduced (pad, 0),
                juce::Justification::centredRight, true);

    // Kurvenfläche
    const auto curve = curveArea.toFloat();
    g.setColour (colours::surface);
    g.fillRect (curve);

    g.setFont (monoFont (9.0f));

    for (const double frequency : { 50.0, 100.0, 500.0, 1000.0, 5000.0, 10000.0 })
    {
        const float x = curve.getX() + (float) frequencyToProportion (frequency) * curve.getWidth();
        g.setColour (colours::lineFine);
        g.fillRect (juce::Rectangle<float> (x, curve.getY(), 1.0f, curve.getHeight()));
        g.setColour (colours::textTertiary);
        g.drawText (frequency >= 1000.0 ? juce::String (frequency / 1000.0, 0) + "k"
                                        : juce::String (frequency, 0),
                    juce::Rectangle<float> (x + 3.0f, curve.getBottom() - 14.0f, 40.0f, 12.0f),
                    juce::Justification::centredLeft, false);
    }

    for (const float decibels : { -12.0f, 0.0f, 12.0f })
    {
        const float y = curve.getCentreY() - decibels / 24.0f * (curve.getHeight() * 0.5f);
        g.setColour (juce::approximatelyEqual (decibels, 0.0f) ? colours::line : colours::lineFine);
        g.fillRect (juce::Rectangle<float> (curve.getX(), y, curve.getWidth(), 1.0f));

        if (! juce::approximatelyEqual (decibels, 0.0f))
        {
            g.setColour (colours::textTertiary);
            g.drawText ((decibels > 0.0f ? "+" : "") + juce::String ((int) decibels),
                        juce::Rectangle<float> (curve.getX() + 3.0f, y + 2.0f, 30.0f, 11.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    // Frequenzgang
    const auto response = createResponsePath (curve.reduced (1.0f));
    g.setColour (colours::accent.withAlpha (0.18f));

    juce::Path filled (response);
    filled.lineTo (curve.getRight(), curve.getCentreY());
    filled.lineTo (curve.getX(), curve.getCentreY());
    filled.closeSubPath();
    g.fillPath (filled);

    g.setColour (colours::accent);
    g.strokePath (response, juce::PathStrokeType (1.6f));

    g.setColour (colours::line);
    g.drawRect (curve, 1.0f);

    // Fußzeile
    g.setColour (colours::panel);
    g.fillRect (footerArea);
    g.setColour (colours::line);
    g.fillRect (footerArea.getX(), footerArea.getY(), footerArea.getWidth(), 1);

    draw::capsLabel (g, "Ausgang", outputLabelArea, 10.0f);

    if (auto* output = processor.getState().getParameter ("output"))
    {
        const float decibels = output->convertFrom0to1 (outputBar.getValue());
        g.setColour (colours::text);
        g.setFont (monoFont (11.0f));
        g.drawText ((decibels > 0.0f ? "+" : "") + juce::String (decibels, 1) + " dB", outputValueArea,
                    juce::Justification::centredRight, false);
    }

    draw::levelMeter (g, meterArea.toFloat(), processor.getOutputLevel());
}

void EqualizerEditor::timerCallback()
{
    repaint (curveArea);

    const float level = processor.getOutputLevel();

    if (std::abs (level - lastLevel) > 0.005f)
    {
        lastLevel = level;
        repaint (meterArea);
    }
}
} // namespace sis
