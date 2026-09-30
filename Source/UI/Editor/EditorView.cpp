#include "EditorView.h"
#include "../../Model/ClipGeometry.h"
#include "../../PluginProcessor.h"

namespace sis
{
namespace
{
    constexpr int headerHeight = 50;
    constexpr int rulerHeight = 21;
    constexpr int buttonHeight = 28;
    constexpr int scrollHeight = 12;
}

EditorView::EditorView (StudioContext& c)
    : ctx (c), trackArea (c), sampleEditor (c)
{
    FlatButton::Style back;
    back.mono = true;
    back.fontSize = 11.0f;
    back.text = colours::accentHover;
    backButton.setStyle (back);
    backButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::showMapping, false); };

    trimModeButton.onClick = [this]
    {
        ctx.ui.edgeMode = EdgeMode::trim;
        ctx.ui.changed();
    };
    stretchModeButton.onClick = [this]
    {
        ctx.ui.edgeMode = EdgeMode::stretch;
        ctx.ui.changed();
    };
    snapButton.onClick = [this] { showGridMenu(); };

    // Zoom um die Mitte des sichtbaren Ausschnitts; Strg+Mausrad zoomt um den Zeiger
    zoomOutButton.onClick = [this] { trackArea.zoomBy (1.5, ctx.ui.timelineStart + 0.5 * ctx.ui.visibleLength); };
    zoomInButton.onClick = [this] { trackArea.zoomBy (1.0 / 1.5, ctx.ui.timelineStart + 0.5 * ctx.ui.visibleLength); };
    zoomOutButton.setTooltip ("Herauszoomen (Strg+Mausrad)");
    zoomInButton.setTooltip ("Hineinzoomen (Strg+Mausrad)");
    addTrackButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::addTrack, false); };
    bounceButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::bounceZone, false); };

    FlatButton::Style plain;
    plain.background = colours::surface;
    plain.border = colours::lineStrongAlt;
    plain.text = colours::text;
    plain.fontSize = 11.5f;
    addTrackButton.setStyle (plain);

    FlatButton::Style dark;
    dark.background = colours::text;
    dark.text = colours::white;
    dark.fontSize = 11.5f;
    bounceButton.setStyle (dark);

    for (auto* b : { &backButton, &trimModeButton, &stretchModeButton, &snapButton, &addTrackButton, &bounceButton,
                     &zoomOutButton, &zoomInButton })
        addAndMakeVisible (b);

    viewport.setViewedComponent (&trackArea, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    addAndMakeVisible (sampleEditor);

    timeScroll.setAutoHide (false);
    timeScroll.addListener (this);
    addChildComponent (timeScroll);

    updateStyles();
    ctx.ui.addChangeListener (this);
    ctx.model.addChangeListener (this);
}

EditorView::~EditorView()
{
    timeScroll.removeListener (this);
    ctx.ui.removeChangeListener (this);
    ctx.model.removeChangeListener (this);
}

void EditorView::updateStyles()
{
    auto segment = [] (bool active)
    {
        FlatButton::Style s;
        s.fontSize = 11.0f;
        s.background = active ? colours::surface : juce::Colours::transparentBlack;
        s.text = active ? colours::text : colours::textSecondary;
        return s;
    };

    trimModeButton.setStyle (segment (ctx.ui.edgeMode == EdgeMode::trim));
    stretchModeButton.setStyle (segment (ctx.ui.edgeMode == EdgeMode::stretch));

    FlatButton::Style snap;
    snap.fontSize = 11.5f;
    snap.background = ctx.ui.snapToGrid ? colours::accentSoft : colours::surface;
    snap.text = ctx.ui.snapToGrid ? colours::accentDark : colours::textSecondary;
    snap.border = ctx.ui.snapToGrid ? colours::accent : colours::lineStrongAlt;
    snapButton.setStyle (snap);
    snapButton.setButtonText (ctx.ui.snapToGrid ? "Raster " + TrackArea::timeLabel (ctx.ui.gridSeconds, ctx.ui.gridSeconds)
                                                : juce::String ("Raster aus"));

    FlatButton::Style zoom;
    zoom.fontSize = 12.0f;
    zoom.background = colours::surface;
    zoom.border = colours::lineStrongAlt;
    zoom.text = colours::text;
    zoomOutButton.setStyle (zoom);
    zoomInButton.setStyle (zoom);
}

void EditorView::paint (juce::Graphics& g)
{
    g.fillAll (colours::workspace);

    // Kopfzeile
    auto header = headerArea.reduced (14, 0);
    g.setColour (colours::line);
    g.fillRect (headerArea.getX(), headerArea.getBottom() - 1, headerArea.getWidth(), 1);

    header.removeFromLeft (backButton.getWidth() + 10);

    const auto* zone = ctx.model.getSelectedZone();
    const juce::String title = zone != nullptr ? "Zone " + zone->name : juce::String ("Keine Zone");
    const int titleWidth = juce::roundToInt (textWidth (sansFont (13.0f, Weight::semibold), title)) + 4;

    g.setColour (colours::text);
    g.setFont (sansFont (13.0f, Weight::semibold));
    g.drawText (title, header.removeFromLeft (titleWidth), juce::Justification::centredLeft, true);
    header.removeFromLeft (10);

    // Hinweis nennt immer die aktuelle Belegung; er nutzt den Platz bis zum Segmentschalter
    // und wird kürzer, statt abgeschnitten zu werden.
    header.setRight (juce::jmax (header.getX(), segmentArea.getX() - 10));

    const bool trimming = ctx.ui.edgeMode == EdgeMode::trim;
    const juce::String hints[] = {
        trimming ? "Kante schneidet zu · Shift streckt · Ecken = Fades · rechts halten: Werkzeuge · X: Crossfade"_u
                 : "Kante streckt · Shift schneidet zu · Ecken = Fades · rechts halten: Werkzeuge · X: Crossfade"_u,
        trimming ? "Kante schneidet zu · Shift streckt · rechts halten: Werkzeuge"_u
                 : "Kante streckt · Shift schneidet zu · rechts halten: Werkzeuge"_u,
        "Rechts halten: Werkzeuge"_u
    };

    const auto hintFont = monoFont (10.0f);
    g.setColour (colours::textTertiary);
    g.setFont (hintFont);

    for (const auto& hint : hints)
    {
        if (textWidth (hintFont, hint) <= (float) header.getWidth() || &hint == &hints[2])
        {
            g.drawText (hint, header, juce::Justification::centredLeft, false);
            break;
        }
    }

    // Rahmen des Segmentschalters
    g.setColour (colours::lineFine);
    g.fillRect (segmentArea);
    g.setColour (colours::line);
    g.drawRect (segmentArea, 1);

    // Zeitlineal: links die Anzeige des Locators, dann die Sekunden des sichtbaren Ausschnitts
    g.setColour (colours::lowerZone);
    g.fillRect (rulerArea);
    g.setColour (colours::line);
    g.fillRect (rulerArea.getX(), rulerArea.getBottom() - 1, rulerArea.getWidth(), 1);
    g.fillRect (rulerArea.getX() + TrackArea::headerWidth - 1, rulerArea.getY(), 1, rulerArea.getHeight());

    const auto locatorSeconds = ctx.ui.locator * InstrumentModel::timelineSeconds;
    g.setColour (ctx.ui.locator > 0.0 ? colours::accentDark : colours::textTertiary);
    g.setFont (monoFont (10.0f));
    g.drawText ("AB " + juce::String (locatorSeconds, 2) + " s",
                rulerArea.withWidth (TrackArea::headerWidth).reduced (10, 0).withTrimmedBottom (1),
                juce::Justification::centredLeft, false);

    const auto lane = getRulerLane();

    // Dieselbe Umrechnung wie in den Spuren, nur auf das Lineal verschoben
    const auto timeToX = [this, &lane] (double time)
    {
        return (float) lane.getX() + trackArea.timeToX (time) - (float) TrackArea::headerWidth;
    };

    {
        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (lane);

        const double secondsVisible = ctx.ui.visibleLength * InstrumentModel::timelineSeconds;
        const double step = TrackArea::tickStepSeconds ((double) lane.getWidth() / secondsVisible);
        const double startSeconds = ctx.ui.timelineStart * InstrumentModel::timelineSeconds;

        for (auto n = (juce::int64) std::floor (startSeconds / step); ; ++n)
        {
            const double seconds = (double) n * step;
            const float x = timeToX (seconds / InstrumentModel::timelineSeconds);

            if (x > (float) lane.getRight())
                break;

            g.setColour (colours::divider);
            g.fillRect (juce::Rectangle<float> (std::round (x), (float) lane.getY(), 1.0f, (float) lane.getHeight()));
            g.setColour (colours::textTertiary);
            g.drawText (TrackArea::timeLabel (seconds, step), juce::roundToInt (x) + 4, lane.getY(), 60, lane.getHeight(),
                        juce::Justification::centredLeft, false);
        }

        // Locator: Dreieck oben im Lineal
        const float x = timeToX (ctx.ui.locator);
        juce::Path marker;
        marker.addTriangle (x - 5.0f, (float) lane.getY(), x + 5.0f, (float) lane.getY(), x, (float) lane.getY() + 8.0f);
        g.setColour (colours::accent);
        g.fillPath (marker);
        g.fillRect (juce::Rectangle<float> (x, (float) lane.getY(), 1.0f, (float) lane.getHeight()));
    }
}

void EditorView::showGridMenu()
{
    static constexpr double widths[] = { 0.01, 0.05, 0.1, 0.25, 0.5, 1.0 };

    juce::PopupMenu menu;
    menu.addItem ("Aus", true, ! ctx.ui.snapToGrid, [this]
    {
        ctx.ui.snapToGrid = false;
        ctx.ui.changed();
    });
    menu.addSeparator();

    for (const double width : widths)
    {
        const bool current = ctx.ui.snapToGrid && std::abs (ctx.ui.gridSeconds - width) < 1.0e-9;
        menu.addItem (TrackArea::timeLabel (width, width), true, current, [this, width]
        {
            ctx.ui.snapToGrid = true;
            ctx.ui.gridSeconds = width;
            ctx.ui.changed();
        });
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&snapButton));
}

juce::Rectangle<int> EditorView::getRulerLane() const
{
    const int width = juce::jmax (1, trackArea.getWidth() - TrackArea::headerWidth);
    return rulerArea.withTrimmedLeft (TrackArea::headerWidth).withWidth (width).withTrimmedBottom (1);
}

void EditorView::setLocatorFrom (int x)
{
    const auto lane = getRulerLane();
    const double time = trackArea.xToTime ((float) (x - lane.getX() + TrackArea::headerWidth));

    ctx.ui.locator = juce::jlimit (0.0, TrackArea::maxAxis, trackArea.snapped (time));
    ctx.ui.changed();
}

void EditorView::mouseDown (const juce::MouseEvent& e)
{
    if (! rulerArea.contains (e.getPosition()))
        return;

    if (e.x < rulerArea.getX() + TrackArea::headerWidth)
    {
        // Die Anzeige links setzt den Locator auf den Anfang zurück
        ctx.ui.locator = 0.0;
        ctx.ui.changed();
        return;
    }

    setLocatorFrom (e.x);
}

void EditorView::mouseDrag (const juce::MouseEvent& e)
{
    if (rulerArea.contains (e.getMouseDownPosition()) && e.getMouseDownX() >= rulerArea.getX() + TrackArea::headerWidth)
        setLocatorFrom (e.x);
}

void EditorView::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    if (std::abs (newRangeStart - ctx.ui.timelineStart) > 1.0e-9)
    {
        ctx.ui.timelineStart = juce::jmax (0.0, newRangeStart);
        ctx.ui.changed();
    }
}

void EditorView::updateScrollBar()
{
    const double total = juce::jmax (TrackArea::axisLength (ctx.model.getSelectedZone()),
                                     ctx.ui.timelineStart + ctx.ui.visibleLength);
    timeScroll.setRangeLimits (0.0, total, juce::dontSendNotification);
    timeScroll.setCurrentRange (ctx.ui.timelineStart, ctx.ui.visibleLength, juce::dontSendNotification);
}

void EditorView::resized()
{
    auto area = getLocalBounds();

    headerArea = area.removeFromTop (headerHeight);
    auto header = headerArea.reduced (14, 0);
    const int centreY = header.getCentreY();

    auto place = [centreY] (juce::Component& c, juce::Rectangle<int>& row, int width, int height, bool fromRight)
    {
        auto r = fromRight ? row.removeFromRight (width) : row.removeFromLeft (width);
        c.setBounds (r.withSizeKeepingCentre (width, height));
    };

    place (backButton, header, backButton.getTextWidth() + 4, 20, false);

    place (bounceButton, header, bounceButton.getTextWidth() + 24, buttonHeight, true);
    header.removeFromRight (10);
    place (addTrackButton, header, addTrackButton.getTextWidth() + 20, buttonHeight, true);
    header.removeFromRight (10);
    place (snapButton, header, snapButton.getTextWidth() + 20, buttonHeight, true);
    header.removeFromRight (6);
    place (zoomInButton, header, buttonHeight, buttonHeight, true);
    header.removeFromRight (2);
    place (zoomOutButton, header, buttonHeight, buttonHeight, true);
    header.removeFromRight (10);

    const int segmentWidth = trimModeButton.getTextWidth() + stretchModeButton.getTextWidth() + 44;
    auto segment = header.removeFromRight (segmentWidth).withSizeKeepingCentre (segmentWidth, buttonHeight);
    segmentArea = segment;
    auto inner = segment.reduced (3);
    trimModeButton.setBounds (inner.removeFromLeft (trimModeButton.getTextWidth() + 20));
    stretchModeButton.setBounds (inner);

    rulerArea = area.removeFromTop (rulerHeight);

    sampleEditor.setBounds (area.removeFromBottom (metrics::lowerZone));

    // Rollbalken für die Zeit nur, wenn es mehr als die sichtbaren 8 Sekunden gibt
    const bool scrolls = TrackArea::axisLength (ctx.model.getSelectedZone()) > ctx.ui.visibleLength + 1.0e-6
                         || ctx.ui.timelineStart > 0.0;
    timeScroll.setVisible (scrolls);

    if (scrolls)
        timeScroll.setBounds (area.removeFromBottom (scrollHeight).withTrimmedLeft (TrackArea::headerWidth));

    viewport.setBounds (area);

    const int height = trackArea.getIdealHeight();
    const bool needsScroll = height > viewport.getHeight();
    trackArea.setSize (viewport.getWidth() - (needsScroll ? viewport.getScrollBarThickness() : 0),
                       juce::jmax (height, viewport.getHeight()));

    updateScrollBar();
}

void EditorView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Eine andere Zone beginnt wieder vorn
    const auto zoneId = ctx.model.selectedZoneId;

    if (zoneId != shownZoneId)
    {
        shownZoneId = zoneId;

        if (ctx.ui.timelineStart != 0.0)
        {
            ctx.ui.timelineStart = 0.0;
            ctx.ui.changed();   // kommt hier gleich noch einmal an
        }
    }

    updateStyles();
    resized();
    repaint();
}
} // namespace sis
