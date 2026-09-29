#include "ToolBar.h"
#include "../../PluginProcessor.h"

namespace sis
{
namespace
{
    constexpr int pad = 12;
    constexpr int gap = 6;
}

ToolBar::ToolBar (StudioContext& c) : ctx (c)
{
    auto tabAction = [this] (View v)
    {
        return [this, v]
        {
            ctx.ui.view = v;
            ctx.ui.changed();
        };
    };

    mappingTab.onClick = tabAction (View::mapping);
    editorTab.onClick = tabAction (View::editor);
    exportTab.onClick = tabAction (View::exportView);

    playButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::togglePlayback, false); };
    loopButton.onClick = [this]
    {
        ctx.ui.looping = ! ctx.ui.looping;
        ctx.ui.changed();
    };
    exportButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::exportInstrument, false); };

    FlatButton::Style exportStyle;
    exportStyle.background = colours::accent;
    exportStyle.hoverBackground = colours::accentHover;
    exportStyle.text = colours::white;
    exportStyle.weight = Weight::medium;
    exportButton.setStyle (exportStyle);

    auto& gain = ctx.processor.getMasterGain();
    masterBar.setTrackHeight (6.0f);
    masterBar.setValue (gain.get());
    masterBar.onDragStart = [&gain] { gain.beginChangeGesture(); };
    masterBar.onDragEnd = [&gain] { gain.endChangeGesture(); };
    masterBar.onValueChange = [this, &gain] (float v)
    {
        gain.setValueNotifyingHost (gain.convertTo0to1 (v));
        repaint (masterValueArea);
    };

    for (auto* b : { &mappingTab, &editorTab, &exportTab, &playButton, &loopButton, &exportButton })
        addAndMakeVisible (b);
    addAndMakeVisible (masterBar);

    updateStyles();
    ctx.ui.addChangeListener (this);
    startTimerHz (20);   // Pegelanzeige und Master-Automation des Hosts
}

ToolBar::~ToolBar()
{
    ctx.ui.removeChangeListener (this);
}

void ToolBar::updateStyles()
{
    auto tabStyle = [] (bool active)
    {
        FlatButton::Style s;
        s.fontSize = 12.5f;
        s.weight = Weight::medium;
        s.background = active ? colours::surface : juce::Colours::transparentBlack;
        s.text = active ? colours::text : colours::textSecondary;
        s.underline = active ? colours::accent : juce::Colours::transparentBlack;
        return s;
    };

    mappingTab.setStyle (tabStyle (ctx.ui.view == View::mapping));
    editorTab.setStyle (tabStyle (ctx.ui.view == View::editor));
    exportTab.setStyle (tabStyle (ctx.ui.view == View::exportView));

    const bool playing = ctx.ui.playing;
    FlatButton::Style play;
    play.background = playing ? colours::accent : colours::surface;
    play.text = playing ? colours::white : colours::textSecondary;
    play.border = colours::line;
    playButton.setStyle (play);
    playButton.setIcon (playing ? draw::stopIcon : draw::playIcon);
    playButton.setTooltip (playing ? "Stopp (Leertaste)" : "Wiedergabe (Leertaste)");

    FlatButton::Style loop;
    loop.mono = true;
    loop.fontSize = 11.0f;
    loop.background = ctx.ui.looping ? colours::accentSoft : colours::surface;
    loop.text = ctx.ui.looping ? colours::accentDark : colours::textSecondary;
    loop.border = colours::line;
    loopButton.setStyle (loop);
}

void ToolBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::bars);
    draw::bottomLine (g, getLocalBounds(), colours::line);

    g.setColour (colours::textTertiary);
    g.setFont (monoFont (10.5f).withExtraKerningFactor (0.04f));
    g.drawText ("MASTER", masterLabelArea, juce::Justification::centredLeft, false);

    g.setColour (colours::textSecondary);
    g.setFont (monoFont (11.0f));
    g.drawText (gainToText (ctx.processor.getMasterGain().get()), masterValueArea,
                juce::Justification::centredLeft, false);

    draw::levelMeter (g, meterArea.toFloat(), ctx.processor.getOutputLevel());
}

void ToolBar::resized()
{
    auto area = getLocalBounds().withTrimmedBottom (1).reduced (pad, 0);
    const int centreY = area.getCentreY();

    auto place = [centreY] (juce::Component& c, int x, int w, int h)
    {
        c.setBounds (x, centreY - h / 2, w, h);
    };

    // Reiter links (padding 7px 13px + 2px Unterstrich)
    int x = area.getX();
    for (auto* tab : { &mappingTab, &editorTab, &exportTab })
    {
        const int w = tab->getTextWidth() + 26;
        place (*tab, x, w, 32);
        x += w + gap;
    }

    // Rechts, von außen nach innen
    int right = area.getRight();
    const int exportWidth = exportButton.getTextWidth() + 26;
    right -= exportWidth;
    place (exportButton, right, exportWidth, 30);

    right -= gap + 52;
    masterValueArea = { right, centreY - 8, 52, 16 };

    right -= gap + 88;
    place (masterBar, right, 88, 14);
    masterBar.setBounds (masterBar.getBounds().translated (0, -3));
    meterArea = { right, masterBar.getBounds().getBottom() + 1, 88, 3 };

    const int labelWidth = juce::roundToInt (textWidth (monoFont (10.5f).withExtraKerningFactor (0.04f), "MASTER")) + 2;
    right -= gap + labelWidth;
    masterLabelArea = { right, centreY - 8, labelWidth, 16 };
    right -= 6;   // margin-left: 6px

    const int loopWidth = loopButton.getTextWidth() + 20;
    right -= gap + loopWidth;
    place (loopButton, right, loopWidth, 30);

    right -= gap + 34;
    place (playButton, right, 34, 30);
}

void ToolBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateStyles();
    resized();
}

void ToolBar::timerCallback()
{
    const float gain = ctx.processor.getMasterGain().get();

    if (! juce::approximatelyEqual (gain, masterBar.getValue()))
    {
        masterBar.setValue (gain);
        repaint (masterValueArea);
    }

    const float level = ctx.processor.getOutputLevel();

    if (std::abs (level - lastLevel) > 0.005f)
    {
        lastLevel = level;
        repaint (meterArea);
    }
}
} // namespace sis
