#include "TrackArea.h"
#include "../../Model/ClipGeometry.h"
#include "../../PluginProcessor.h"
#include "../SampleDrop.h"
#include "../Widgets.h"

namespace sis
{
namespace
{
    constexpr int clipInset = 6;        // Abstand Clip zu Spurrand
    constexpr int clipHeaderHeight = 15;
    constexpr int edgeGrab = 7;
    constexpr int fadeHandleSize = 11;
    constexpr int fadeHandleTop = 19;
    constexpr int dropStripHeight = 44; // Platz unter der letzten Spur: dort entsteht eine neue

    // In der Reihenfolge der Werkzeugleiste
    constexpr EditTool paletteTools[] = { EditTool::select, EditTool::erase, EditTool::split };

    geometry::ClipState toState (const Clip& c)
    {
        return { c.offset, c.natural, c.stretch, c.trimStart, c.trimEnd, c.fadeIn, c.fadeOut };
    }

    void applyState (Clip& c, const geometry::ClipState& s)
    {
        c.offset = s.offset;
        c.stretch = s.stretch;
        c.trimStart = s.trimStart;
        c.trimEnd = s.trimEnd;
        c.fadeIn = s.fadeIn;
        c.fadeOut = s.fadeOut;
    }

    juce::String trackLabel (const Track& track, int index)
    {
        return track.name.isNotEmpty() ? track.name : "Spur " + juce::String (index + 1);
    }

    juce::String countText (int count, const char* one, const char* many)
    {
        return count == 1 ? "1 " + juce::String::fromUTF8 (one)
                          : juce::String (count) + " " + juce::String::fromUTF8 (many);
    }

}

TrackArea::TrackArea (StudioContext& c) : ctx (c)
{
    setWantsKeyboardFocus (true);
    ctx.model.addChangeListener (this);
    ctx.ui.addChangeListener (this);
}

TrackArea::~TrackArea()
{
    closePalette();
    ctx.model.removeChangeListener (this);
    ctx.ui.removeChangeListener (this);
}

Zone* TrackArea::getZone() const
{
    return ctx.model.getSelectedZone();
}

int TrackArea::getIdealHeight() const
{
    const auto* zone = getZone();
    return zone != nullptr ? (int) zone->tracks.size() * (rowHeight + 1) + dropStripHeight : 0;
}

double TrackArea::axisLength (const Zone* zone)
{
    if (zone == nullptr)
        return 1.0;

    // Auf volle Sekunden aufrunden und eine Sekunde Luft lassen, damit man hinter den
    // letzten Clip noch etwas legen kann
    const double seconds = std::ceil (zone->contentEnd() * InstrumentModel::timelineSeconds) + 1.0;
    return juce::jlimit (1.0, maxAxis, seconds / InstrumentModel::timelineSeconds);
}

float TrackArea::laneWidth() const
{
    return (float) juce::jmax (1, getWidth() - headerWidth);
}

float TrackArea::timeToX (double time) const
{
    return (float) headerWidth + (float) ((time - ctx.ui.timelineStart) / ctx.ui.visibleLength) * laneWidth();
}

double TrackArea::xToTime (float x) const
{
    return ctx.ui.timelineStart + (double) ((x - (float) headerWidth) / laneWidth()) * ctx.ui.visibleLength;
}

double TrackArea::tickStepSeconds (double pixelsPerSecond, double minPixels)
{
    static constexpr double steps[] = { 0.01, 0.02, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 30.0 };

    for (const double step : steps)
        if (step * pixelsPerSecond >= minPixels)
            return step;

    return 60.0;
}

juce::String TrackArea::timeLabel (double seconds, double step)
{
    if (step < 0.1 && seconds < 1.0)
        return juce::String (juce::roundToInt (seconds * 1000.0)) + " ms";

    const int decimals = step >= 1.0 ? 0 : (step >= 0.1 ? 1 : 2);
    return juce::String (seconds, decimals) + " s";
}

double TrackArea::snapped (double time) const
{
    return geometry::snapTime (time, ctx.ui.snapToGrid, ctx.ui.gridSeconds / InstrumentModel::timelineSeconds);
}

void TrackArea::zoomBy (double factor, double anchor)
{
    const double before = ctx.ui.visibleLength;
    const double after = juce::jlimit (UiState::minVisibleLength, UiState::maxVisibleLength, before * factor);

    if (std::abs (after - before) < 1.0e-9)
        return;

    // Die Stelle unter dem Zeiger bleibt stehen
    ctx.ui.timelineStart = juce::jmax (0.0, anchor - (anchor - ctx.ui.timelineStart) * after / before);
    ctx.ui.visibleLength = after;
    ctx.ui.changed();
}

juce::Rectangle<int> TrackArea::getRowBounds (int index) const
{
    return { 0, index * (rowHeight + 1), getWidth(), rowHeight };
}

juce::Rectangle<int> TrackArea::getLaneBounds (int index) const
{
    return getRowBounds (index).withTrimmedLeft (headerWidth);
}

juce::Rectangle<float> TrackArea::getClipBounds (int trackIndex, int clipIndex) const
{
    const auto* zone = getZone();
    if (zone == nullptr || ! juce::isPositiveAndBelow (trackIndex, (int) zone->tracks.size()))
        return {};

    const auto& track = zone->tracks[(size_t) trackIndex];

    if (! juce::isPositiveAndBelow (clipIndex, (int) track.clips.size()))
        return {};

    const auto& clip = track.clips[(size_t) clipIndex];

    if (! clip.hasSample())
        return {};

    const auto lane = getLaneBounds (trackIndex).toFloat();
    const auto length = juce::jmax (0.005, clip.length());

    return { timeToX (clip.offset), lane.getY() + (float) clipInset,
             (float) (length / ctx.ui.visibleLength) * laneWidth(), lane.getHeight() - 2.0f * (float) clipInset };
}

juce::Rectangle<int> TrackArea::getMuteBounds (int index) const
{
    auto head = getRowBounds (index).removeFromLeft (headerWidth).reduced (9, 8);
    return { head.getX(), head.getBottom() - 18, 21, 18 };
}

juce::Rectangle<int> TrackArea::getNameBounds (int index) const
{
    // Wie in paint: Innenfläche des Kopfes, erste Zeile, hinter dem Farbquadrat
    auto title = getRowBounds (index).withWidth (headerWidth).reduced (9, 8).removeFromTop (16);
    title.removeFromLeft (8 + 6);
    return title;
}

juce::Rectangle<int> TrackArea::getSoloBounds (int index) const
{
    return getMuteBounds (index).translated (21 + 5, 0);
}

int TrackArea::rowAt (juce::Point<int> p) const
{
    const auto* zone = getZone();
    if (zone == nullptr || p.y < 0)
        return -1;

    const int row = p.y / (rowHeight + 1);
    return juce::isPositiveAndBelow (row, (int) zone->tracks.size()) ? row : -1;
}

double TrackArea::positionToTime (int x) const
{
    return juce::jmax (0.0, xToTime ((float) x));
}

bool TrackArea::isStretching (const juce::MouseEvent& e) const
{
    const bool shift = e.mods.isShiftDown();
    return (ctx.ui.edgeMode == EdgeMode::stretch) != shift;
}

TrackArea::Hit TrackArea::hitAt (int trackIndex, juce::Point<int> p) const
{
    const auto* zone = getZone();

    if (zone == nullptr || p.x < headerWidth || ! juce::isPositiveAndBelow (trackIndex, (int) zone->tracks.size()))
        return {};

    const auto& track = zone->tracks[(size_t) trackIndex];
    const auto row = getRowBounds (trackIndex);
    const float x = (float) p.x;
    const float y = (float) p.y;

    // Von oben nach unten im Stapel: der oberste Clip fängt den Klick
    for (int c = (int) track.clips.size(); --c >= 0;)
    {
        const auto clip = getClipBounds (trackIndex, c);

        if (clip.isEmpty())
            continue;

        const auto& data = track.clips[(size_t) c];

        // Fade-Ecken oben
        const float handleTop = (float) (row.getY() + fadeHandleTop);
        if (y >= handleTop && y <= handleTop + (float) fadeHandleSize)
        {
            const float fadeInX = clip.getX() + (float) data.fadeIn * clip.getWidth();
            const float fadeOutX = clip.getRight() - (float) data.fadeOut * clip.getWidth();

            if (std::abs (x - fadeInX) <= fadeHandleSize * 0.5f + 1.0f)
                return { c, Drag::fadeIn };
            if (std::abs (x - fadeOutX) <= fadeHandleSize * 0.5f + 1.0f)
                return { c, Drag::fadeOut };
        }

        if (y < clip.getY() || y > clip.getBottom() || x < clip.getX() || x > clip.getRight())
            continue;

        if (x <= clip.getX() + edgeGrab)
            return { c, Drag::leftEdge };
        if (x >= clip.getRight() - edgeGrab)
            return { c, Drag::rightEdge };

        return { c, Drag::move };
    }

    return {};
}

//==============================================================================
void TrackArea::paintClip (juce::Graphics& g, const Track& track, int trackIndex, int clipIndex, bool audible) const
{
    const auto clip = getClipBounds (trackIndex, clipIndex);

    if (clip.isEmpty())
        return;

    const auto& data = track.clips[(size_t) clipIndex];
    const auto lane = getLaneBounds (trackIndex);
    const bool selected = ctx.ui.isClipSelected (data.uid);
    const float alpha = audible ? 1.0f : 0.3f;
    const float visibleEnd = (float) getWidth();

    {
        const juce::Graphics::ScopedSaveState clipState (g);
        g.reduceClipRegion (clip.toNearestInt());

        g.setColour (track.softColour.withMultipliedAlpha (alpha));
        g.fillRect (clip);

        // Kopfstreifen mit Samplename und Stretch-Anzeige; der Name bleibt lesbar,
        // auch wenn der Clip-Anfang links aus dem Bild gerollt ist
        auto header = clip.withHeight ((float) clipHeaderHeight);
        g.setColour ((selected ? track.colour.darker (0.45f) : track.colour).withMultipliedAlpha (alpha));
        g.fillRect (header);

        auto headerText = header.withLeft (juce::jmax (header.getX(), (float) lane.getX())).reduced (5.0f, 0.0f);
        headerText.setRight (juce::jmin (headerText.getRight(), visibleEnd - 5.0f));
        // Stretch-Faktor, davor der Pegel des Clips, falls er normalisiert ist
        auto badge = juce::String (data.stretch, 2) + juce::String::fromUTF8 ("×");

        if (std::abs (data.gain - 1.0f) > 0.001f)
        {
            const auto db = juce::Decibels::gainToDecibels (data.gain);
            badge = (db >= 0.0f ? "+" : "") + juce::String (db, 1) + " dB · "_u + badge;
        }
        const float badgeWidth = textWidth (monoFont (10.0f), badge) + 6.0f;
        g.setColour (colours::white.withMultipliedAlpha (alpha));
        g.setFont (monoFont (10.0f));
        g.drawText (badge, headerText.removeFromRight (badgeWidth).toNearestInt(), juce::Justification::centredRight, false);
        g.drawText (data.sample, headerText.toNearestInt(), juce::Justification::centredLeft, true);

        // Wellenform des gespielten Ausschnitts
        const auto peaks = ctx.model.waveformFor (data, trackIndex);
        const int total = (int) peaks.size();
        const int from = juce::jlimit (0, total - 1, (int) (data.trimStart * (total - 1)));
        const int to = juce::jlimit (from + 1, total - 1, (int) (data.trimEnd * (total - 1)));
        std::vector<float> slice (peaks.begin() + from, peaks.begin() + to + 1);
        if (data.reverse)
            std::reverse (slice.begin(), slice.end());

        auto waveArea = clip.withTrimmedTop ((float) clipHeaderHeight).reduced (0.0f, 4.0f);
        const int bars = juce::jlimit (1, (int) slice.size(), juce::roundToInt (clip.getWidth() / 3.0f));
        draw::waveBars (g, waveArea, slice, bars, 1.0f, 0.04f, track.colour.withMultipliedAlpha (alpha));

        // Fade-Flächen
        const auto fadeArea = clip.withTrimmedTop ((float) clipHeaderHeight);
        g.setColour (colours::surface.withAlpha (0.82f * alpha));
        if (data.fadeIn > 0.0)
        {
            juce::Path p;
            const float w = (float) data.fadeIn * clip.getWidth();
            p.addTriangle (fadeArea.getX(), fadeArea.getY(), fadeArea.getX() + w, fadeArea.getY(),
                           fadeArea.getX(), fadeArea.getBottom());
            g.fillPath (p);
        }
        if (data.fadeOut > 0.0)
        {
            juce::Path p;
            const float w = (float) data.fadeOut * clip.getWidth();
            p.addTriangle (fadeArea.getRight() - w, fadeArea.getY(), fadeArea.getRight(), fadeArea.getY(),
                           fadeArea.getRight(), fadeArea.getBottom());
            g.fillPath (p);
        }
    }

    // Rahmen: gewählte Clips kräftig, die anderen fein
    if (selected)
    {
        g.setColour (colours::accentDark);
        g.drawRect (clip, 2.0f);
    }
    else
    {
        g.setColour (track.colour.withMultipliedAlpha (0.85f));
        g.drawRect (clip, 1.0f);
    }

    // Griffe für Fade-in und Fade-out
    const float handleY = (float) (getRowBounds (trackIndex).getY() + fadeHandleTop);
    for (const bool isFadeIn : { true, false })
    {
        const float x = isFadeIn ? clip.getX() + (float) data.fadeIn * clip.getWidth()
                                 : clip.getRight() - (float) data.fadeOut * clip.getWidth();
        const juce::Rectangle<float> handle (x - fadeHandleSize * 0.5f, handleY,
                                             (float) fadeHandleSize, (float) fadeHandleSize);
        g.setColour (track.colour);
        g.fillRect (handle);
        g.setColour (colours::surface);
        g.drawRect (handle, 1.0f);
    }
}

void TrackArea::paintOverlaps (juce::Graphics& g, const Track& track, int trackIndex) const
{
    /* Wo ein Clip über einem anderen liegt, wird schraffiert: unter dem sichtbaren Clip
       liegt noch einer. Zu hören ist dort der obere – außer in seinen Fades, da scheint
       der untere durch (das ist dann ein Crossfade). */
    const auto lane = getLaneBounds (trackIndex).toFloat();

    for (size_t lower = 0; lower < track.clips.size(); ++lower)
    {
        const auto& a = track.clips[lower];

        if (! a.hasSample())
            continue;

        for (size_t upper = lower + 1; upper < track.clips.size(); ++upper)
        {
            const auto& b = track.clips[upper];

            if (! b.hasSample())
                continue;

            const double from = juce::jmax (a.offset, b.offset);
            const double to = juce::jmin (a.end(), b.end());

            if (to <= from)
                continue;

            const auto area = juce::Rectangle<float>::leftTopRightBottom (
                timeToX (from), lane.getY() + (float) (clipInset + clipHeaderHeight),
                timeToX (to), lane.getBottom() - (float) clipInset);

            const juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (area.toNearestInt());

            g.setColour (colours::text.withAlpha (0.30f));
            for (float x = area.getX() - area.getHeight(); x < area.getRight(); x += 7.0f)
                g.drawLine (x, area.getBottom(), x + area.getHeight(), area.getY(), 1.0f);

            g.setColour (colours::text.withAlpha (0.45f));
            g.fillRect (juce::Rectangle<float> (area.getX(), area.getY(), 1.0f, area.getHeight()));
            g.fillRect (juce::Rectangle<float> (area.getRight() - 1.0f, area.getY(), 1.0f, area.getHeight()));
        }
    }
}

void TrackArea::paint (juce::Graphics& g)
{
    g.fillAll (colours::workspace);

    auto* zone = getZone();
    if (zone == nullptr || zone->tracks.empty())
    {
        if (dropActive)
        {
            g.setColour (colours::accentSoft.withAlpha (0.6f));
            g.fillRect (getLocalBounds().withHeight (rowHeight));
        }

        g.setColour (colours::textTertiary);
        g.setFont (sansFont (12.0f));
        g.drawText (zone == nullptr ? "Keine Zone gewählt · im Mapping eine Zone oder ein Kit-Teil anklicken"_u
                                    : "Keine Spuren · ein Sample aus der Liste hierher ziehen oder „Spur hinzufügen“"_u,
                    getLocalBounds().withHeight (60), juce::Justification::centred, true);
        return;
    }

    const bool anySolo = std::any_of (zone->tracks.begin(), zone->tracks.end(),
                                      [] (const Track& t) { return t.solo; });
    const float visibleEnd = (float) getWidth();

    for (int i = 0; i < (int) zone->tracks.size(); ++i)
    {
        const auto& track = zone->tracks[(size_t) i];
        const bool current = i == zone->selectedTrack;
        const bool selected = current || ctx.ui.selectedTracks.count (i) > 0;
        const bool audible = ! track.mute && (! anySolo || track.solo);

        const auto row = getRowBounds (i);
        g.setColour (current ? colours::lowerZone : colours::workspace);
        g.fillRect (row);
        g.setColour (colours::keyLine);
        g.fillRect (row.getX(), row.getBottom(), row.getWidth(), 1);

        // Spur: alles, was mitrollt, wird auf die Spur beschnitten – der Kopf bleibt stehen
        const auto lane = getLaneBounds (i);
        {
            const juce::Graphics::ScopedSaveState laneState (g);
            g.reduceClipRegion (lane);

            // Rasterlinien im Abstand der Lineal-Striche – mit dem Zoom feiner oder gröber
            g.setColour (colours::lineFine);
            const double secondsVisible = ctx.ui.visibleLength * InstrumentModel::timelineSeconds;
            const double step = tickStepSeconds ((double) laneWidth() / secondsVisible);
            const double startSeconds = ctx.ui.timelineStart * InstrumentModel::timelineSeconds;

            for (auto n = (juce::int64) std::ceil (startSeconds / step); ; ++n)
            {
                const float x = timeToX ((double) n * step / InstrumentModel::timelineSeconds);
                if (x > visibleEnd)
                    break;
                if (n > 0)
                    g.fillRect (juce::Rectangle<float> (std::round (x), (float) lane.getY(), 1.0f, (float) lane.getHeight()));
            }

            if (! track.hasClips())
            {
                g.setColour (colours::textTertiary);
                g.setFont (monoFont (10.0f));
                g.drawText ("Sample aus der Liste hierher ziehen", lane.reduced (12, 0),
                            juce::Justification::centredLeft, true);
            }

            // Von unten nach oben im Stapel, dann die Überschneidungen darüber
            for (int c = 0; c < (int) track.clips.size(); ++c)
                paintClip (g, track, i, c, audible);

            paintOverlaps (g, track, i);

            // Schere: der Strich, an dem geschnitten würde
            if (ctx.ui.editTool == EditTool::split && hoverRow == i && hoverTime >= 0.0)
            {
                const float x = timeToX (hoverTime);
                g.setColour (colours::text);
                g.fillRect (juce::Rectangle<float> (std::round (x), (float) lane.getY() + 2.0f, 1.0f, (float) lane.getHeight() - 4.0f));
            }
        }

        // Spurkopf – nach der Spur gezeichnet, damit nichts Hereingerolltes darüber liegt
        auto head = row.withWidth (headerWidth);
        g.setColour (selected ? colours::surface : colours::lowerZone);
        g.fillRect (head);
        g.setColour (colours::line);
        g.fillRect (head.getRight() - 1, head.getY(), 1, head.getHeight());

        if (selected)
        {
            g.setColour (current ? colours::accent : colours::accent.withAlpha (0.5f));
            g.fillRect (head.getX(), head.getY(), 3, head.getHeight());
        }

        auto inner = head.reduced (9, 8);
        auto titleRow = inner.removeFromTop (16);
        g.setColour (track.colour);
        g.fillRect (titleRow.removeFromLeft (8).withSizeKeepingCentre (8, 8));
        titleRow.removeFromLeft (6);
        if (i != renamingTrack)
        {
            g.setColour (colours::text);
            g.setFont (sansFont (11.5f, Weight::medium));
            g.drawText (trackLabel (track, i), titleRow, juce::Justification::centredLeft, true);
        }

        const auto mute = getMuteBounds (i);
        const auto solo = getSoloBounds (i);
        const auto drawToggle = [&g] (juce::Rectangle<int> area, const juce::String& text, bool on, juce::Colour onColour)
        {
            g.setColour (on ? onColour : colours::surface);
            g.fillRect (area);
            g.setColour (colours::line);
            g.drawRect (area, 1);
            g.setColour (on ? colours::white : colours::textSecondary);
            g.setFont (monoFont (10.0f));
            g.drawText (text, area, juce::Justification::centred, false);
        };
        drawToggle (mute, "M", track.mute, colours::warning);
        drawToggle (solo, "S", track.solo, colours::accent);

        g.setColour (colours::textSecondary);
        g.setFont (monoFont (10.0f));
        g.drawText (gainToText (track.gain),
                    juce::Rectangle<int> (solo.getRight() + 5, solo.getY(),
                                          head.getRight() - 9 - (solo.getRight() + 5), 18),
                    juce::Justification::centredRight, false);
    }

    // Vorschau des hereingezogenen Samples: wo der Clip landet, und ob eine Spur entsteht
    if (dropActive && dropRow >= 0)
    {
        const bool newTrack = dropRow >= (int) zone->tracks.size();
        const auto row = newTrack ? juce::Rectangle<int> (0, (int) zone->tracks.size() * (rowHeight + 1),
                                                          getWidth(), dropStripHeight)
                                  : getRowBounds (dropRow);
        const auto lane = row.withTrimmedLeft (headerWidth);

        if (newTrack)
        {
            g.setColour (colours::accentSoft.withAlpha (0.5f));
            g.fillRect (row);
            g.setColour (colours::accentDark);
            g.setFont (monoFont (10.0f));
            g.drawText ("+ neue Spur", row.withWidth (headerWidth).reduced (10, 0), juce::Justification::centredLeft, false);
        }

        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (lane);

        const juce::Rectangle<float> ghost (timeToX (dropTime), (float) (lane.getY() + clipInset),
                                            juce::jmax (4.0f, (float) (dropLength / ctx.ui.visibleLength) * laneWidth()),
                                            (float) (lane.getHeight() - 2 * clipInset));
        g.setColour (colours::accent.withAlpha (0.18f));
        g.fillRect (ghost);
        g.setColour (colours::accent);
        g.drawRect (ghost, 1.5f);
    }

    // Locator und Wiedergabelinie, nur über den Spuren (nicht über den Köpfen)
    {
        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (getLocalBounds().withTrimmedLeft (headerWidth));

        const float locatorX = timeToX (ctx.ui.locator);
        g.setColour (colours::accent.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (locatorX, 0.0f, 1.0f, (float) getHeight()));

        if (ctx.ui.playing)
        {
            const float x = timeToX (ctx.ui.playhead);
            g.setColour (colours::text);
            g.fillRect (juce::Rectangle<float> (x, 0.0f, 1.0f, (float) getHeight()));
        }
    }
}

//==============================================================================
void TrackArea::mouseMove (const juce::MouseEvent& e)
{
    const int row = rowAt (e.getPosition());
    const int previousRow = hoverRow;
    const double previousTime = hoverTime;

    hoverRow = row;
    hoverTime = -1.0;

    auto newCursor = juce::MouseCursor (juce::MouseCursor::NormalCursor);

    if (e.x >= headerWidth)
    {
        if (ctx.ui.editTool != EditTool::select)
        {
            newCursor = toolIcons::cursorFor (ctx.ui.editTool);

            if (ctx.ui.editTool == EditTool::split && row >= 0 && hitAt (row, e.getPosition()).clip >= 0)
                hoverTime = snapped (positionToTime (e.x));
        }
        else if (row >= 0)
        {
            switch (hitAt (row, e.getPosition()).mode)
            {
                case Drag::leftEdge:
                case Drag::rightEdge:
                case Drag::fadeIn:
                case Drag::fadeOut:   newCursor = juce::MouseCursor::LeftRightResizeCursor; break;
                case Drag::move:      newCursor = juce::MouseCursor::DraggingHandCursor; break;
                case Drag::none:
                case Drag::erase:
                case Drag::palette:   break;
            }
        }
    }

    setMouseCursor (newCursor);

    if (previousRow != hoverRow || previousTime != hoverTime)
        repaint();
}

void TrackArea::mouseExit (const juce::MouseEvent&)
{
    if (hoverTime >= 0.0)
    {
        hoverTime = -1.0;
        repaint();
    }
}

void TrackArea::mouseDown (const juce::MouseEvent& e)
{
    // Damit Entf, X und die Zwischenablage hier ankommen
    grabKeyboardFocus();

    // Eine offen gebliebene Werkzeugleiste schließt der nächste Klick daneben
    if (palette != nullptr)
    {
        closePalette();
        return;
    }

    auto* zone = getZone();
    const int row = rowAt (e.getPosition());
    const bool extend = e.mods.isShiftDown();   // Shift erweitert die Auswahl

    if (zone == nullptr)
        return;

    // Spurkopf: Spur wählen, Stumm/Solo, Rechtsklick fürs Menü
    if (e.x < headerWidth)
    {
        if (row < 0)
            return;

        auto& track = zone->tracks[(size_t) row];

        if (e.mods.isPopupMenu())
        {
            if (ctx.ui.selectedTracks.count (row) == 0)
                selectTrack (row, false);

            showTrackMenu (row);
            return;
        }

        if (getMuteBounds (row).contains (e.getPosition()))
        {
            ctx.step (track.mute ? "Spur hörbar"_u : "Spur stumm"_u);
            track.mute = ! track.mute;
            ctx.model.notifyChanged();
        }
        else if (getSoloBounds (row).contains (e.getPosition()))
        {
            ctx.step ("Solo umgeschaltet"_u);
            track.solo = ! track.solo;
            ctx.model.notifyChanged();
        }
        else
        {
            selectTrack (row, extend);
        }

        return;
    }

    // Zeitleiste: rechte Taste gehalten öffnet die Werkzeugleiste
    if (e.mods.isRightButtonDown())
    {
        openPalette (e.getPosition());
        drag = Drag::palette;
        return;
    }

    if (row < 0)
    {
        if (! extend)
            clearClipSelection();
        return;
    }

    switch (ctx.ui.editTool)
    {
        case EditTool::erase:
            eraseClipAt (row, e.getPosition());
            drag = Drag::erase;      // weiter über andere Clips wischen
            return;

        case EditTool::split:
            splitClipAt (row, e.getPosition());
            return;

        case EditTool::select:
            break;
    }

    const auto hit = hitAt (row, e.getPosition());

    if (hit.clip < 0)
    {
        // Leere Stelle: Auswahl der Clips aufheben, die Spur wird die gewählte
        if (! extend)
        {
            clearClipSelection();
            selectTrack (row, false);
        }
        return;
    }

    auto& clip = zone->tracks[(size_t) row].clips[(size_t) hit.clip];

    /* Shift auf dem Clip-Körper erweitert die Auswahl. An Kante und Fade-Ecke bleibt Shift,
       was es war: der Umschalter zwischen Zuschneiden und Strecken. */
    if (extend && hit.mode == Drag::move)
    {
        selectClip (clip, row, true);
        return;
    }

    if (! ctx.ui.isClipSelected (clip.uid))
        selectClip (clip, row, false);
    else
    {
        ctx.ui.focusClip = clip.uid;
        ctx.ui.changed();
    }

    drag = hit.mode;
    dragTrack = row;
    dragClip = clip.uid;
    dragMoved = false;
    grabOffset = positionToTime (e.x) - clip.offset;
    moveOrigins.clear();
    dragStartRow = row;
    createdTracks = 0;

    // Verschieben nimmt alle gewählten Clips der Zone mit, samt ihrer Spur
    if (drag == Drag::move)
        for (int t = 0; t < (int) zone->tracks.size(); ++t)
            for (auto& c : zone->tracks[(size_t) t].clips)
                if (ctx.ui.isClipSelected (c.uid))
                    moveOrigins[c.uid] = { c.offset, t };
}

void TrackArea::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::palette)
    {
        if (palette != nullptr)
            palette->setHover (palette->itemAt (palette->getLocalPoint (this, e.getPosition())));
        return;
    }

    if (drag == Drag::erase)
    {
        const int row = rowAt (e.getPosition());
        if (row >= 0)
            eraseClipAt (row, e.getPosition());
        return;
    }

    if (drag == Drag::none)
        return;

    // Am Rand weiterrollen, damit ein Clip über den sichtbaren Ausschnitt hinaus gezogen werden kann
    if (e.x > getWidth() - 16)
        scrollBy (0.02 * ctx.ui.visibleLength);
    else if (e.x < headerWidth + 8 && ctx.ui.timelineStart > 0.0)
        scrollBy (-0.02 * ctx.ui.visibleLength);

    applyDrag (e);
}

void TrackArea::mouseUp (const juce::MouseEvent& e)
{
    if (drag == Drag::palette)
    {
        drag = Drag::none;

        if (palette == nullptr)
            return;

        const int item = palette->itemAt (palette->getLocalPoint (this, e.getPosition()));

        if (item >= 0)
            setTool (paletteTools[item]);
        else if (e.getDistanceFromDragStart() < 4)
            palette->makeSticky();       // nur kurz geklickt: offen lassen, Linksklick wählt
        else
            closePalette();

        return;
    }

    if (drag == Drag::move && dragMoved)
        finishMove();

    drag = Drag::none;
    dragTrack = -1;
    dragClip = 0;
    moveOrigins.clear();
}

void TrackArea::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Doppelklick in den Spurkopf (nicht auf Stumm/Solo) benennt die Spur um
    const int row = rowAt (e.getPosition());

    if (row < 0 || e.x >= headerWidth || getMuteBounds (row).contains (e.getPosition())
        || getSoloBounds (row).contains (e.getPosition()))
        return;

    startRename (row);
}

void TrackArea::startRename (int trackIndex)
{
    auto* zone = getZone();

    if (zone == nullptr || ! juce::isPositiveAndBelow (trackIndex, (int) zone->tracks.size()))
        return;

    finishRename (true);

    renamingTrack = trackIndex;
    nameEditor = std::make_unique<juce::TextEditor>();
    nameEditor->setFont (sansFont (11.5f, Weight::medium));
    nameEditor->setIndents (3, 0);
    nameEditor->setJustification (juce::Justification::centredLeft);
    nameEditor->setText (zone->tracks[(size_t) trackIndex].name, false);
    nameEditor->setBounds (getNameBounds (trackIndex).expanded (3, 3));

    /* Übernehmen und Abbrechen erst nach der Tastenbehandlung: das Textfeld darf sich nicht
       in seinem eigenen Rückruf abbauen. */
    juce::Component::SafePointer<TrackArea> safe (this);
    const auto later = [safe] (bool keep)
    {
        juce::MessageManager::callAsync ([safe, keep] { if (safe != nullptr) safe->finishRename (keep); });
    };

    nameEditor->onReturnKey = [later] { later (true); };
    nameEditor->onEscapeKey = [later] { later (false); };
    nameEditor->onFocusLost = [later] { later (true); };

    addAndMakeVisible (*nameEditor);
    nameEditor->selectAll();
    nameEditor->grabKeyboardFocus();
    repaint();
}

void TrackArea::finishRename (bool keep)
{
    if (nameEditor == nullptr)
        return;

    const auto text = nameEditor->getText().trim();
    const int index = renamingTrack;

    // Erst abbauen: das Umbenennen meldet eine Änderung, und die zeichnet neu
    removeChildComponent (nameEditor.get());
    nameEditor.reset();
    renamingTrack = -1;

    auto* zone = getZone();

    if (keep && zone != nullptr && juce::isPositiveAndBelow (index, (int) zone->tracks.size())
        && text.isNotEmpty() && text != zone->tracks[(size_t) index].name)
    {
        ctx.step ("Spur umbenannt");
        zone->tracks[(size_t) index].name = text;
        ctx.model.notifyChanged();
    }

    repaint();

    // Nach Enter oder Esc gehören die Tasten wieder den Spuren – nach einem Klick woandershin nicht
    if (juce::Component::getCurrentlyFocusedComponent() == nullptr)
        grabKeyboardFocus();
}

void TrackArea::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Strg+Rad zoomt um den Zeiger, waagerecht wischen oder Shift+Rad rollt die Zeit
    if (e.mods.isCommandDown())
    {
        if (e.x >= headerWidth)
            zoomBy (wheel.deltaY > 0.0f ? 0.8 : 1.25, positionToTime (e.x));
        return;
    }

    if (wheel.deltaX != 0.0f || e.mods.isShiftDown())
    {
        const float delta = wheel.deltaX != 0.0f ? -wheel.deltaX : -wheel.deltaY;
        scrollBy ((double) delta * 0.5 * ctx.ui.visibleLength);
        return;
    }

    Component::mouseWheelMove (e, wheel);
}

void TrackArea::scrollBy (double delta)
{
    const double shown = ctx.ui.visibleLength;
    const double limit = juce::jmax (0.0, juce::jmax (axisLength (getZone()), ctx.ui.timelineStart + shown) - shown);
    const double next = juce::jlimit (0.0, juce::jmax (limit, drag != Drag::none ? maxAxis - shown : 0.0),
                                      ctx.ui.timelineStart + delta);

    if (std::abs (next - ctx.ui.timelineStart) > 1.0e-9)
    {
        ctx.ui.timelineStart = next;
        ctx.ui.changed();
    }
}

bool TrackArea::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (palette != nullptr)
            closePalette();
        else
            clearClipSelection();

        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelectedClips();
        return true;
    }

    // X ohne Zusatztaste: Crossfade. Strg+X gehört der Zwischenablage (Befehle der Shell).
    if (juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode()) == 'X'
        && ! key.getModifiers().isAnyModifierKeyDown())
    {
        crossfadeSelected();
        return true;
    }

    return false;
}

void TrackArea::applyDrag (const juce::MouseEvent& e)
{
    auto* zone = getZone();
    if (zone == nullptr || ! juce::isPositiveAndBelow (dragTrack, (int) zone->tracks.size()))
        return;

    auto* clip = zone->tracks[(size_t) dragTrack].findClip (dragClip);

    if (clip == nullptr)
        return;

    const double pointerTime = positionToTime (e.x);
    const bool snapping = ctx.ui.snapToGrid;
    const bool stretching = isStretching (e);
    const double grid = ctx.ui.gridSeconds / InstrumentModel::timelineSeconds;

    auto state = toState (*clip);

    switch (drag)
    {
        case Drag::move:      state = geometry::moveClip (state, pointerTime, grabOffset, snapping, maxAxis, grid); break;
        case Drag::rightEdge: state = geometry::dragRightEdge (state, pointerTime, snapping, stretching, maxAxis, grid); break;
        case Drag::leftEdge:  state = geometry::dragLeftEdge (state, pointerTime, snapping, stretching, grid); break;
        case Drag::fadeIn:    state = geometry::dragFadeIn (state, pointerTime); break;
        case Drag::fadeOut:   state = geometry::dragFadeOut (state, pointerTime); break;
        case Drag::none:
        case Drag::erase:
        case Drag::palette:   return;
    }

    // Der Name der Geste hält den ganzen Zug als einen Schritt zusammen
    switch (drag)
    {
        case Drag::move:      ctx.step ("Clip verschoben"_u); break;
        case Drag::rightEdge:
        case Drag::leftEdge:  ctx.step (stretching ? "Clip gestreckt"_u : "Clip zugeschnitten"_u); break;
        case Drag::fadeIn:    ctx.step ("Fade ein geändert"_u); break;
        case Drag::fadeOut:   ctx.step ("Fade aus geändert"_u); break;
        case Drag::none:
        case Drag::erase:
        case Drag::palette:   break;
    }

    if (drag != Drag::move || moveOrigins.empty())
    {
        applyState (*clip, state);
        dragMoved = true;
        ctx.model.notifyChanged();
        return;
    }

    /* Waagerecht um dasselbe Stück wie der gegriffene Clip, senkrecht um so viele Spuren,
       wie der Zeiger gewandert ist. Grenzen und neue Spuren regelt das Modell. */
    const auto grabbed = moveOrigins.find (dragClip);
    const double origin = grabbed != moveOrigins.end() ? grabbed->second.offset : clip->offset;
    const int pointerRow = e.y < 0 ? 0 : e.y / (rowHeight + 1);

    zone->moveClips (moveOrigins, state.offset - origin, pointerRow - dragStartRow, createdTracks);

    // Die Spur des gezogenen Clips ist die gewählte – Inspektor und Einfügen folgen ihr
    dragTrack = zone->trackOfClip (dragClip);
    if (dragTrack >= 0 && zone->selectedTrack != dragTrack)
    {
        zone->selectedTrack = dragTrack;
        ctx.ui.selectedTracks = { dragTrack };
        ctx.ui.changed();
    }

    dragMoved = true;
    ctx.model.notifyChanged();
}

void TrackArea::finishMove()
{
    /* Wer verschoben wurde, ist „zuletzt gesetzt“ und kommt im Stapel nach oben – so gilt
       die Regel „der obere klingt“ auch nach dem Ziehen. */
    auto* zone = getZone();

    if (zone == nullptr)
        return;

    ctx.step ("Clip verschoben"_u);

    for (auto& track : zone->tracks)
        std::stable_partition (track.clips.begin(), track.clips.end(),
                               [this] (const Clip& c) { return moveOrigins.count (c.uid) == 0; });

    ctx.model.notifyChanged();
}

//==============================================================================
void TrackArea::selectTrack (int trackIndex, bool extend)
{
    auto* zone = getZone();

    if (zone == nullptr || ! juce::isPositiveAndBelow (trackIndex, (int) zone->tracks.size()))
        return;

    auto& selection = ctx.ui.selectedTracks;

    if (extend)
    {
        if (selection.count (trackIndex) > 0 && selection.size() > 1)
        {
            selection.erase (trackIndex);

            // Die Hauptspur muss eine der gewählten bleiben
            if (zone->selectedTrack == trackIndex)
            {
                zone->selectedTrack = *selection.begin();
                ctx.model.notifyChanged();
            }

            ctx.ui.changed();
            return;
        }

        selection.insert (zone->selectedTrack);
        selection.insert (trackIndex);
    }
    else
    {
        selection = { trackIndex };
    }

    if (zone->selectedTrack != trackIndex)
    {
        zone->selectedTrack = trackIndex;
        ctx.model.notifyChanged();
    }

    ctx.ui.changed();
}

void TrackArea::selectClip (const Clip& clip, int trackIndex, bool extend)
{
    auto* zone = getZone();

    if (zone == nullptr)
        return;

    auto& selection = ctx.ui.selectedClips;

    if (extend && selection.count (clip.uid) > 0)
    {
        selection.erase (clip.uid);
    }
    else
    {
        if (! extend)
            selection.clear();

        selection.insert (clip.uid);
        ctx.ui.focusClip = clip.uid;
    }

    // Die Spur des Clips wird die gewählte – Inspektor, Sample-Editor und Einfügen folgen ihr
    if (ctx.ui.selectedTracks.size() <= 1)
        ctx.ui.selectedTracks = { trackIndex };

    if (zone->selectedTrack != trackIndex)
    {
        zone->selectedTrack = trackIndex;
        ctx.model.notifyChanged();
    }

    ctx.ui.changed();
}

void TrackArea::clearClipSelection()
{
    if (! ctx.ui.selectedClips.empty())
    {
        ctx.ui.selectedClips.clear();
        ctx.ui.changed();
    }
}

//==============================================================================
void TrackArea::deleteSelectedClips()
{
    auto* zone = getZone();

    if (zone == nullptr)
        return;

    int removed = 0;

    for (auto& track : zone->tracks)
        for (const auto& clip : track.clips)
            if (ctx.ui.isClipSelected (clip.uid))
                ++removed;

    if (removed == 0)
    {
        ctx.toast ("Kein Clip gewählt · Spuren löscht der Rechtsklick auf den Spurkopf"_u);
        return;
    }

    ctx.step (removed == 1 ? "Clip gelöscht"_u : "Clips gelöscht"_u);

    for (auto& track : zone->tracks)
        track.clips.erase (std::remove_if (track.clips.begin(), track.clips.end(),
                                           [this] (const Clip& c) { return ctx.ui.isClipSelected (c.uid); }),
                           track.clips.end());

    ctx.ui.selectedClips.clear();
    ctx.model.notifyChanged();
    ctx.ui.changed();
    ctx.toast (countText (removed, "Clip gelöscht", "Clips gelöscht") + " · Strg+Z holt sie zurück"_u);
}

void TrackArea::eraseClipAt (int trackIndex, juce::Point<int> position)
{
    auto* zone = getZone();
    const auto hit = hitAt (trackIndex, position);

    if (zone == nullptr || hit.clip < 0)
        return;

    auto& clips = zone->tracks[(size_t) trackIndex].clips;
    const auto uid = clips[(size_t) hit.clip].uid;

    ctx.step ("Clip gelöscht"_u);
    clips.erase (clips.begin() + hit.clip);
    ctx.ui.selectedClips.erase (uid);
    ctx.model.notifyChanged();
}

void TrackArea::splitClipAt (int trackIndex, juce::Point<int> position)
{
    auto* zone = getZone();
    const auto hit = hitAt (trackIndex, position);

    if (zone == nullptr || hit.clip < 0)
        return;

    auto& clips = zone->tracks[(size_t) trackIndex].clips;
    auto& original = clips[(size_t) hit.clip];
    const double time = snapped (positionToTime (position.x));

    geometry::ClipState left, right;

    if (! geometry::splitClip (toState (original), time, left, right))
        return;

    ctx.step ("Clip geschnitten");

    Clip second = original;
    second.uid = Clip::nextUid();
    applyState (original, left);
    applyState (second, right);

    // Das rechte Stück liegt im Stapel gleich über dem linken – wie der Clip vorher
    clips.insert (clips.begin() + hit.clip + 1, second);
    ctx.model.notifyChanged();
}

void TrackArea::crossfadeSelected()
{
    auto* zone = getZone();

    if (zone == nullptr)
        return;

    int made = 0;

    for (auto& track : zone->tracks)
    {
        std::vector<Clip*> chosen;

        for (auto& clip : track.clips)
            if (ctx.ui.isClipSelected (clip.uid) && clip.hasSample())
                chosen.push_back (&clip);

        for (size_t a = 0; a < chosen.size(); ++a)
        {
            for (size_t b = a + 1; b < chosen.size(); ++b)
            {
                auto first = toState (*chosen[a]);
                auto second = toState (*chosen[b]);

                if (! geometry::crossfade (first, second))
                    continue;

                if (made == 0)
                    ctx.step ("Crossfade");

                applyState (*chosen[a], first);
                applyState (*chosen[b], second);
                ++made;
            }
        }
    }

    if (made == 0)
    {
        ctx.toast ("Für einen Crossfade zwei sich überschneidende Clips derselben Spur wählen (Shift+Klick)"_u);
        return;
    }

    ctx.model.notifyChanged();
    ctx.toast (made == 1 ? "Crossfade über die Überschneidung gelegt"_u
                         : juce::String (made) + " Crossfades gelegt");
}

void TrackArea::removeTracks (int clickedTrack)
{
    auto* zone = getZone();

    if (zone == nullptr)
        return;

    std::vector<int> doomed;

    if (ctx.ui.selectedTracks.count (clickedTrack) > 0 && ctx.ui.selectedTracks.size() > 1)
        doomed.assign (ctx.ui.selectedTracks.begin(), ctx.ui.selectedTracks.end());
    else
        doomed = { clickedTrack };

    const auto name = doomed.size() == 1 && juce::isPositiveAndBelow (clickedTrack, (int) zone->tracks.size())
                          ? trackLabel (zone->tracks[(size_t) clickedTrack], clickedTrack)
                          : juce::String ((int) doomed.size()) + " Spuren";

    ctx.step (name + " gelöscht"_u);

    // Von hinten nach vorn, damit die Nummern der übrigen stimmen
    const auto zoneId = zone->id;
    std::sort (doomed.rbegin(), doomed.rend());

    for (const int index : doomed)
        ctx.processor.removeTrack (zoneId, index);

    if (auto* remaining = getZone())
        ctx.ui.selectedTracks = { remaining->selectedTrack };

    ctx.ui.changed();
    ctx.toast (name + " gelöscht · Strg+Z holt sie zurück"_u);
}

void TrackArea::showTrackMenu (int trackIndex)
{
    const auto count = ctx.ui.selectedTracks.count (trackIndex) > 0 ? (int) ctx.ui.selectedTracks.size() : 1;

    juce::PopupMenu menu;
    menu.addItem (count > 1 ? juce::String (count) + " Spuren löschen"_u : "Spur löschen"_u,
                  [this, trackIndex] { removeTracks (trackIndex); });
    menu.addSeparator();
    menu.addItem ("Spur hinzufügen"_u, [this] { ctx.commands.invokeDirectly (cmd::addTrack, false); });
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

//==============================================================================
void TrackArea::copyClips()
{
    auto* zone = getZone();
    auto copied = zone != nullptr ? zone->copyClips (ctx.ui.selectedClips) : std::vector<ClipboardClip>();

    if (copied.empty())
    {
        ctx.toast ("Kein Clip gewählt"_u);
        return;
    }

    ctx.ui.clipboard = std::move (copied);
    ctx.toast (countText ((int) ctx.ui.clipboard.size(), "Clip kopiert", "Clips kopiert"));
}

void TrackArea::cutClips()
{
    auto* zone = getZone();

    if (zone == nullptr || ctx.ui.selectedClips.empty())
    {
        ctx.toast ("Kein Clip gewählt"_u);
        return;
    }

    copyClips();

    ctx.step ("Ausgeschnitten");

    for (auto& track : zone->tracks)
        track.clips.erase (std::remove_if (track.clips.begin(), track.clips.end(),
                                           [this] (const Clip& c) { return ctx.ui.isClipSelected (c.uid); }),
                           track.clips.end());

    ctx.ui.selectedClips.clear();
    ctx.model.notifyChanged();
    ctx.ui.changed();
    ctx.toast (countText ((int) ctx.ui.clipboard.size(), "Clip ausgeschnitten", "Clips ausgeschnitten"));
}

void TrackArea::pasteClips()
{
    auto* zone = getZone();

    if (ctx.ui.clipboard.empty())
    {
        ctx.toast ("Die Zwischenablage ist leer"_u);
        return;
    }

    if (zone == nullptr || zone->getSelectedTrack() == nullptr)
    {
        ctx.toast ("Erst eine Spur wählen"_u);
        return;
    }

    ctx.step ("Eingefügt"_u);

    int newTracks = 0;
    const auto pasted = zone->pasteClips (ctx.ui.clipboard, zone->selectedTrack, ctx.ui.locator, &newTracks);

    ctx.ui.selectedClips = { pasted.begin(), pasted.end() };
    ctx.ui.focusClip = pasted.empty() ? 0 : pasted.back();

    ctx.model.notifyChanged();
    ctx.ui.changed();

    auto message = countText ((int) ctx.ui.clipboard.size(), "Clip eingefügt", "Clips eingefügt");
    if (newTracks > 0)
        message << " · "_u << countText (newTracks, "neue Spur", "neue Spuren");
    ctx.toast (message);
}

//==============================================================================
void TrackArea::openPalette (juce::Point<int> position)
{
    closePalette();

    auto* top = getTopLevelComponent();

    if (top == nullptr)
        return;

    palette = std::make_unique<ToolPalette>();
    palette->setCurrent (ctx.ui.editTool);
    palette->onChoose = [this] (EditTool tool) { setTool (tool); };
    palette->onDismiss = [this] { closePalette(); };

    /* Über dem Zeiger, damit ein bloßer Klick auf nichts fällt und die Leiste offen bleibt;
       ist oben kein Platz, darunter. */
    const auto at = top->getLocalPoint (this, position);
    const auto size = ToolPalette::idealSize();
    auto bounds = size.withPosition (at.x - size.getWidth() / 2, at.y - size.getHeight() - 10);

    if (bounds.getY() < 0)
        bounds.setY (at.y + 14);

    palette->setBounds (bounds.constrainedWithin (top->getLocalBounds()));
    top->addAndMakeVisible (*palette);
    palette->toFront (false);
}

void TrackArea::closePalette()
{
    if (palette == nullptr)
        return;

    if (auto* parent = palette->getParentComponent())
        parent->removeChildComponent (palette.get());

    palette.reset();
}

void TrackArea::setTool (EditTool tool)
{
    closePalette();

    if (ctx.ui.editTool == tool)
        return;

    ctx.ui.editTool = tool;
    ctx.ui.changed();

    ctx.toast (tool == EditTool::erase ? "Werkzeug Löschen · Klick auf einen Clip löscht ihn"_u
             : tool == EditTool::split ? "Werkzeug Schere · Klick auf einen Clip schneidet ihn am Strich"_u
                                       : "Werkzeug Auswahl"_u);
}

//==============================================================================
bool TrackArea::isInterestedInDragSource (const SourceDetails& details)
{
    return sampleDrop::indexOf (details) >= 0;
}

void TrackArea::itemDragMove (const SourceDetails& details)
{
    const auto* zone = getZone();
    const int index = sampleDrop::indexOf (details);

    dropActive = true;
    dropRow = -1;

    if (zone != nullptr)
    {
        const int row = details.localPosition.y / (rowHeight + 1);
        dropRow = juce::jmin (row, (int) zone->tracks.size());

        if (juce::isPositiveAndBelow (index, (int) ctx.model.samples.size()))
            dropLength = ctx.model.samples[(size_t) index].lengthSeconds / InstrumentModel::timelineSeconds;

        dropTime = snapped (positionToTime (details.localPosition.x));
    }

    repaint();
}

void TrackArea::itemDragExit (const SourceDetails&)
{
    dropActive = false;
    dropRow = -1;
    repaint();
}

void TrackArea::itemDropped (const SourceDetails& details)
{
    dropActive = false;
    dropRow = -1;
    repaint();

    auto* zone = getZone();

    if (zone == nullptr)
    {
        ctx.toast ("Erst im Mapping eine Zone oder ein Kit-Teil wählen"_u);
        return;
    }

    const auto* sample = sampleDrop::resolve (ctx, sampleDrop::indexOf (details));

    if (sample == nullptr)
        return;

    const double length = sample->lengthSeconds / InstrumentModel::timelineSeconds;
    const double time = juce::jlimit (0.0, juce::jmax (0.0, maxAxis - length),
                                      snapped (positionToTime (details.localPosition.x)));
    const int row = details.localPosition.y / (rowHeight + 1);

    ctx.step (sample->name + " eingefügt"_u);

    int target = row;

    if (! juce::isPositiveAndBelow (row, (int) zone->tracks.size()))
    {
        // Unter die letzte Spur: neue Spur
        zone->appendTrack();
        target = (int) zone->tracks.size() - 1;
    }

    // Obenauf – eine Spur trägt beliebig viele Clips
    auto& track = zone->tracks[(size_t) target];
    const auto& clip = track.addSample (*sample, time);

    zone->selectedTrack = target;
    ctx.ui.selectedClips = { clip.uid };
    ctx.ui.focusClip = clip.uid;
    ctx.ui.selectedTracks = { target };

    ctx.model.notifyChanged();
    ctx.ui.changed();
    ctx.toast (sample->name + " → "_u + trackLabel (track, target));

    grabKeyboardFocus();
}

//==============================================================================
void TrackArea::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Eine andere Zone: die Auswahl galt der alten
    const auto zoneId = ctx.model.selectedZoneId;

    if (zoneId != shownZoneId)
    {
        shownZoneId = zoneId;
        finishRename (false);
        ctx.ui.selectedClips.clear();
        ctx.ui.selectedTracks.clear();
    }

    if (ctx.ui.playing && ! isTimerRunning())
        startTimerHz (30);
    else if (! ctx.ui.playing && isTimerRunning())
        stopTimer();

    repaint();
}

void TrackArea::timerCallback()
{
    // Die Laufmarke nicht aus dem Bild laufen lassen: umblättern, sobald sie den Rand erreicht
    const double start = ctx.ui.timelineStart;

    if (ctx.ui.playing && (ctx.ui.playhead > start + ctx.ui.visibleLength || ctx.ui.playhead < start))
    {
        ctx.ui.timelineStart = juce::jmax (0.0, ctx.ui.playhead - 0.05 * ctx.ui.visibleLength);
        ctx.ui.changed();
    }

    repaint();
}
} // namespace sis
