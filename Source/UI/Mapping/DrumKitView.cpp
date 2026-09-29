#include "DrumKitView.h"
#include "../../PluginProcessor.h"
#include "../SampleDrop.h"

namespace sis
{
namespace
{
    /** Platz eines Teils als Anteil der Fläche: x, y, Breite, Höhe. */
    struct Box { float x, y, w, h; };

    /* Ein Schlagzeug von oben: Becken außen und oben, Kessel in der Mitte, die HiHat links.
       Die Zahlen sind von Hand gesetzt – ein Raster sähe aus wie ein Raster und nicht wie
       ein Schlagzeug. Jedes Becken hat seinen eigenen Platz, so dass auch das volle Kit
       nirgends übereinanderliegt; nur die Hängetoms teilen sich den Bogen über der
       Bassdrum, je nachdem, wie viele es sind. */
    Box cymbalBox (const juce::String& id)
    {
        if (id == "splash") return { 0.00f, 0.03f, 0.12f, 0.08f };
        if (id == "crash")  return { 0.13f, 0.01f, 0.20f, 0.13f };
        if (id == "crash2") return { 0.45f, 0.00f, 0.19f, 0.12f };
        if (id == "ride")   return { 0.67f, 0.04f, 0.22f, 0.14f };
        if (id == "china")  return { 0.87f, 0.20f, 0.13f, 0.09f };
        return { 0.45f, 0.00f, 0.15f, 0.10f };
    }

    constexpr Box kickBox  { 0.42f, 0.44f, 0.26f, 0.30f };
    constexpr Box snareBox { 0.20f, 0.43f, 0.19f, 0.22f };
    constexpr Box hiHatBox { 0.01f, 0.26f, 0.18f, 0.12f };

    constexpr float switchHeight = 17.0f;
    constexpr float switchGap = 4.0f;
    constexpr double flashSeconds = 0.9;

    juce::String nameOf (const DrumPart& part)  { return juce::String::fromUTF8 (part.name); }
    juce::String nameOf (const DrumPiece& p)    { return juce::String::fromUTF8 (p.name); }
}

DrumKitView::DrumKitView (StudioContext& c) : ctx (c)
{
    ctx.model.addChangeListener (this);
}

DrumKitView::~DrumKitView()
{
    releaseNote();
    ctx.model.removeChangeListener (this);
}

void DrumKitView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Wird eine Zone woanders gewählt (Zonen-Panel, Editor), zeigt ihr Teil diese Spielweise
    if (const auto* zone = ctx.model.getSelectedZone())
        if (const auto* piece = findPieceForPart (zone->drumPart))
            articulation[piece->id] = zone->drumPart;

    layout();
    repaint();
}

void DrumKitView::resized()
{
    /* Das Kit behält sein Seitenverhältnis und wird mittig gestellt. Zöge man es über die
       ganze Fläche, würden aus den Becken flache Ovale – ein Schlagzeug, das man nicht
       wiedererkennt, hilft beim Zuordnen gar nichts. */
    constexpr float aspect = 1.4f;   // Breite zu Höhe

    auto area = getLocalBounds().reduced (14);
    const int byWidth = juce::roundToInt ((float) area.getWidth() / aspect);
    const int height = juce::jmin (area.getHeight(), byWidth);
    const int width = juce::roundToInt ((float) height * aspect);

    stage = area.withSizeKeepingCentre (width, height);
    layout();
}

void DrumKitView::layout()
{
    pieces.clear();

    const auto area = stage.toFloat();

    if (area.isEmpty())
        return;

    const auto toRect = [&area] (Box b)
    {
        return juce::Rectangle<float> (area.getX() + b.x * area.getWidth(), area.getY() + b.y * area.getHeight(),
                                       b.w * area.getWidth(), b.h * area.getHeight());
    };

    std::vector<const DrumPiece*> cymbals, rackToms, floorToms, percussion;
    const DrumPiece *kick = nullptr, *snare = nullptr, *hiHat = nullptr;

    for (const auto& piece : drumPieces())
    {
        if (! ctx.model.hasKitPiece (piece.id))
            continue;

        switch (piece.kind)
        {
            case DrumPieceKind::kick:       kick = &piece; break;
            case DrumPieceKind::snare:      snare = &piece; break;
            case DrumPieceKind::hiHat:      hiHat = &piece; break;
            case DrumPieceKind::rackTom:    rackToms.push_back (&piece); break;
            case DrumPieceKind::floorTom:   floorToms.push_back (&piece); break;
            case DrumPieceKind::cymbal:     cymbals.push_back (&piece); break;
            case DrumPieceKind::percussion: percussion.push_back (&piece); break;
        }
    }

    const auto place = [this, &toRect] (const DrumPiece* piece, Box box, bool cymbal)
    {
        if (piece != nullptr)
            pieces.push_back ({ piece, toRect (box), cymbal, {} });
    };

    // Reihenfolge = Zeichenreihenfolge: hinten die Becken, vorne die Bassdrum
    for (const auto* cymbal : cymbals)
        place (cymbal, cymbalBox (cymbal->id), true);

    if (floorToms.size() == 1)
    {
        place (floorToms[0], { 0.72f, 0.33f, 0.20f, 0.24f }, false);
    }
    else
    {
        for (size_t i = 0; i < floorToms.size(); ++i)
            place (floorToms[i], i == 0 ? Box { 0.71f, 0.27f, 0.17f, 0.20f }
                                        : Box { 0.76f, 0.50f, 0.20f, 0.24f }, false);
    }

    // Hängetoms im Bogen über der Bassdrum: je mehr, desto kleiner
    if (! rackToms.empty())
    {
        constexpr float left = 0.33f, right = 0.67f;
        const int count = (int) rackToms.size();
        const float slot = (right - left) / (float) count;
        const float width = juce::jmin (0.16f, slot - 0.012f);
        const float middle = (float) (count - 1) * 0.5f;

        for (int i = 0; i < count; ++i)
        {
            const float lift = middle > 0.0f ? 0.025f * std::abs ((float) i - middle) / middle : 0.0f;
            const float centre = left + slot * ((float) i + 0.5f);
            place (rackToms[(size_t) i], { centre - width * 0.5f, 0.15f + lift, width, width * 1.15f }, false);
        }
    }

    place (hiHat, hiHatBox, true);
    place (snare, snareBox, false);
    place (kick, kickBox, false);

    for (size_t i = 0; i < percussion.size(); ++i)
        place (percussion[i], { 0.03f + 0.17f * (float) i, 0.80f, 0.14f, 0.10f }, false);

    // Umschalter der Spielweisen unter dem Teil
    const auto font = monoFont (9.5f);

    for (auto& placed : pieces)
    {
        if (placed.piece->articulations.size() < 2)
            continue;

        std::vector<float> widths;
        float total = 0.0f;

        for (const auto* id : placed.piece->articulations)
        {
            const auto* part = findDrumPart (id);
            const float w = textWidth (font, juce::String::fromUTF8 (part != nullptr ? part->shortName : id)) + 14.0f;
            widths.push_back (w);
            total += w;
        }

        float x = juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - total),
                                placed.bounds.getCentreX() - total * 0.5f);
        const float y = placed.bounds.getBottom() + switchGap;

        for (size_t i = 0; i < widths.size(); ++i)
        {
            if (const auto* part = findDrumPart (placed.piece->articulations[i]))
                placed.switches.push_back ({ part, { x, y, widths[i], switchHeight } });

            x += widths[i];
        }
    }
}

const DrumPart& DrumKitView::currentPart (const DrumPiece& piece) const
{
    const auto found = articulation.find (piece.id);

    if (found != articulation.end())
        if (const auto* part = findDrumPart (found->second))
            return *part;

    return *findDrumPart (piece.articulations.front());
}

DrumKitView::Hit DrumKitView::hitAt (juce::Point<float> position) const
{
    // Umschalter zuerst: sie liegen teils über fremden Teilen und sind klein
    for (const auto& placed : pieces)
        for (const auto& [part, area] : placed.switches)
            if (area.contains (position))
                return { placed.piece, part };

    /* Von hinten nach vorn suchen: gezeichnet wird in Listenreihenfolge, also liegt das
       zuletzt gezeichnete Teil oben und muss zuerst treffen. */
    for (auto placed = pieces.rbegin(); placed != pieces.rend(); ++placed)
    {
        const auto centre = placed->bounds.getCentre();
        const float dx = (position.x - centre.x) / (placed->bounds.getWidth() * 0.5f);
        const float dy = (position.y - centre.y) / (placed->bounds.getHeight() * 0.5f);

        if (dx * dx + dy * dy <= 1.0f)
            return { placed->piece, &currentPart (*placed->piece) };
    }

    return {};
}

//==============================================================================
void DrumKitView::paint (juce::Graphics& g)
{
    g.fillAll (colours::workspace);

    for (const auto& placed : pieces)
        paintPiece (g, placed);

    if (pieces.empty())
    {
        g.setColour (colours::textTertiary);
        g.setFont (sansFont (12.0f));
        g.drawText ("Das Kit ist leer · „Teil hinzufügen“ stellt etwas auf"_u, getLocalBounds(),
                    juce::Justification::centred, true);
    }
}

void DrumKitView::paintPiece (juce::Graphics& g, const Placed& placed) const
{
    const auto& part = currentPart (*placed.piece);
    const auto& bounds = placed.bounds;
    const auto* zone = ctx.model.findZoneForDrumPart (part.id);
    const auto* selected = ctx.model.getSelectedZone();
    const int numSamples = zone != nullptr ? zone->numSamples() : 0;
    const bool hasSample = numSamples > 0;
    const bool isSelected = zone != nullptr && selected != nullptr && zone->id == selected->id;
    const bool isDropTarget = dropTarget != nullptr && findPieceForPart (dropTarget->id) == placed.piece;
    const auto colour = zone != nullptr ? zone->colour : colours::lineStrong;

    // Fläche: mit Sample kräftiger, damit man ein belegtes Teil auf einen Blick erkennt
    if (isDropTarget)
        g.setColour (colours::accentSoft);
    else if (zone != nullptr)
        g.setColour (colour.withAlpha (hasSample ? (placed.cymbal ? 0.26f : 0.34f) : (placed.cymbal ? 0.10f : 0.14f)));
    else
        g.setColour (colours::zoneIdle);

    g.fillEllipse (bounds);

    // Rand: durchgezogen, sobald das Teil eine Zone hat, sonst gestrichelt
    if (isDropTarget)
    {
        g.setColour (colours::accent);
        g.drawEllipse (bounds, 2.0f);
    }
    else if (zone != nullptr)
    {
        g.setColour (isSelected ? colours::accent : colour);
        g.drawEllipse (bounds, isSelected ? 2.0f : 1.0f);
    }
    else
    {
        juce::Path ring;
        ring.addEllipse (bounds);

        juce::Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, ring, dashes, 2);

        g.setColour (colours::lineStrong);
        g.fillPath (dashed);
    }

    // Becken bekommen zwei Rillen, damit man sie von einem Kessel unterscheidet
    if (placed.cymbal)
    {
        g.setColour ((zone != nullptr ? colour : colours::lineStrong).withAlpha (0.35f));

        for (float shrink : { 0.72f, 0.44f })
            g.drawEllipse (bounds.withSizeKeepingCentre (bounds.getWidth() * shrink, bounds.getHeight() * shrink), 1.0f);
    }

    // Aufleuchten nach dem Ablegen eines Samples
    if (flashing != nullptr && flashing == &part)
    {
        const double t = (juce::Time::getMillisecondCounterHiRes() - flashStart) / (flashSeconds * 1000.0);

        if (t >= 0.0 && t < 1.0)
        {
            const float grow = 4.0f + 10.0f * (float) t;
            g.setColour (colours::accent.withAlpha ((float) (1.0 - t)));
            g.drawEllipse (bounds.expanded (grow), 2.5f);
        }
    }

    // Beschriftung: Name, Taste und – sobald eines darin liegt – das Sample
    const int lineHeight = 13;
    const bool roomForSample = hasSample && bounds.getHeight() >= 3.0f * (float) lineHeight + 8.0f;
    auto text = bounds.toNearestInt().reduced (6, 0);
    text = text.withSizeKeepingCentre (text.getWidth(), (roomForSample ? 3 : 2) * lineHeight);

    g.setColour (isSelected ? colours::accent : (hasSample ? colours::text : colours::textTertiary));
    g.setFont (sansFont (11.0f, hasSample ? Weight::medium : Weight::regular));
    g.drawText (nameOf (part), text.removeFromTop (lineHeight), juce::Justification::centred, true);

    g.setColour (colours::textTertiary);
    g.setFont (monoFont (9.5f));
    g.drawText (noteName (zone != nullptr ? zone->lowNote : part.note), text.removeFromTop (lineHeight),
                juce::Justification::centred, false);

    if (roomForSample)
    {
        const auto* track = zone->getSelectedTrack();
        const auto clip = track != nullptr && track->hasClips() ? track->clips.front().sample : juce::String();

        g.setColour (colours::textSecondary);
        g.drawText (clip, text, juce::Justification::centred, true);
    }

    /* Marke oben rechts am Rand: wie viele Samples im Teil liegen. Das ist der Hinweis,
       dass ein hereingezogenes Sample angekommen ist – ohne in den Editor zu wechseln. */
    if (hasSample)
    {
        const auto centre = bounds.getCentre();
        const juce::Point<float> onRim (centre.x + bounds.getWidth() * 0.5f * 0.72f,
                                        centre.y - bounds.getHeight() * 0.5f * 0.70f);
        const auto badge = juce::Rectangle<float> (16.0f, 16.0f).withCentre (onRim);

        g.setColour (colours::accent);
        g.fillEllipse (badge);
        g.setColour (colours::surface);
        g.drawEllipse (badge, 1.5f);
        g.setColour (colours::white);
        g.setFont (monoFont (9.5f, Weight::semibold));
        g.drawText (juce::String (numSamples), badge.toNearestInt(), juce::Justification::centred, false);
    }

    // Umschalter der Spielweisen
    if (placed.switches.empty())
        return;

    auto frame = placed.switches.front().second.getUnion (placed.switches.back().second);
    g.setColour (colours::lineFine);
    g.fillRoundedRectangle (frame, 3.0f);

    for (const auto& [option, area] : placed.switches)
    {
        const auto* optionZone = ctx.model.findZoneForDrumPart (option->id);
        const bool optionHasSample = optionZone != nullptr && optionZone->numSamples() > 0;
        const bool current = option == &part;
        const bool dropHere = dropTarget == option;

        if (current || dropHere)
        {
            g.setColour (dropHere ? colours::accentSoft : colours::surface);
            g.fillRoundedRectangle (area.reduced (1.0f), 3.0f);
            g.setColour (dropHere ? colours::accent : colours::line);
            g.drawRoundedRectangle (area.reduced (1.0f), 3.0f, 1.0f);
        }

        auto label = area.reduced (3.0f, 0.0f);

        // Punkt: in dieser Spielweise liegt schon ein Sample
        if (optionHasSample)
        {
            g.setColour (colours::accent);
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ label.getX() + 3.0f, label.getCentreY() }));
        }

        label.removeFromLeft (5.0f);
        g.setColour (current ? colours::text : colours::textSecondary);
        g.setFont (monoFont (9.5f));
        g.drawText (juce::String::fromUTF8 (option->shortName), label.toNearestInt(), juce::Justification::centred, false);
    }

    g.setColour (colours::line);
    g.drawRoundedRectangle (frame, 3.0f, 1.0f);
}

//==============================================================================
void DrumKitView::play (const DrumPart& part)
{
    releaseNote();

    const auto* zone = ctx.model.findZoneForDrumPart (part.id);
    soundingNote = zone != nullptr ? zone->lowNote : part.note;
    ctx.processor.getKeyboardState().noteOn (1, soundingNote, 0.9f);
}

void DrumKitView::releaseNote()
{
    if (soundingNote >= 0)
    {
        ctx.processor.getKeyboardState().noteOff (1, soundingNote, 0.0f);
        soundingNote = -1;
    }
}

void DrumKitView::choose (const DrumPiece& piece, const DrumPart& part)
{
    articulation[piece.id] = part.id;

    /* Ein Klang ohne Zone bekommt hier seine – sonst hätte das nächste Sample aus dem
       Browser kein Ziel, und der Klick fühlte sich folgenlos an. */
    if (auto* zone = ctx.model.findZoneForDrumPart (part.id))
    {
        ctx.model.selectZone (zone->id);
        repaint();
    }
    else
    {
        ctx.step (nameOf (part) + " angelegt");
        ctx.model.addDrumZone (part);
    }

    play (part);
}

void DrumKitView::mouseDown (const juce::MouseEvent& e)
{
    const auto hit = hitAt (e.position);

    if (e.mods.isPopupMenu())
    {
        if (hit.piece != nullptr)
            showPieceMenu (*hit.piece);
        else
            buildAddMenu().showMenuAsync (juce::PopupMenu::Options().withMousePosition());

        return;
    }

    /* Teil und Klang selbst weitergeben, nicht die Stelle in `pieces`: die Änderung am
       Modell baut die Liste neu auf. Die Tabellen in DrumKit.cpp stehen dagegen fest. */
    if (hit.piece != nullptr && hit.part != nullptr)
        choose (*hit.piece, *hit.part);
}

void DrumKitView::mouseUp (const juce::MouseEvent&)
{
    releaseNote();
}

void DrumKitView::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (hitAt (e.position).piece != nullptr ? juce::MouseCursor::PointingHandCursor
                                                        : juce::MouseCursor::NormalCursor);
}

void DrumKitView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto hit = hitAt (e.position);

    if (hit.part != nullptr)
        if (const auto* zone = ctx.model.findZoneForDrumPart (hit.part->id))
            if (onOpenPart != nullptr)
                onOpenPart (zone->id);
}

//==============================================================================
juce::PopupMenu DrumKitView::buildAddMenu()
{
    const auto& all = drumPieces();
    const bool tomsFull = ctx.model.numKitToms() >= maxDrumToms;

    juce::PopupMenu menu;

    const auto section = [&] (const juce::String& title, std::initializer_list<DrumPieceKind> kinds)
    {
        menu.addSectionHeader (title);

        for (size_t i = 0; i < all.size(); ++i)
        {
            const auto& piece = all[i];

            if (std::find (kinds.begin(), kinds.end(), piece.kind) == kinds.end())
                continue;

            const bool present = ctx.model.hasKitPiece (piece.id);
            const bool blocked = ! present && isTom (piece.kind) && tomsFull;

            juce::PopupMenu::Item item (nameOf (piece) + (blocked ? " (höchstens 5 Toms)"_u : juce::String()));
            item.itemID = (int) i + 1;
            item.isTicked = present;
            item.isEnabled = ! present && ! blocked;
            item.action = [this, &piece]
            {
                ctx.step (nameOf (piece) + " aufgebaut");
                ctx.model.addKitPiece (piece);
                ctx.toast (nameOf (piece) + " steht im Kit · Klick legt die Zone an, oder ein Sample daraufziehen"_u);
            };
            menu.addItem (item);
        }
    };

    section ("Becken", { DrumPieceKind::cymbal });
    section ("Toms", { DrumPieceKind::rackTom, DrumPieceKind::floorTom });
    section ("Percussion", { DrumPieceKind::percussion });
    section ("Grundausstattung", { DrumPieceKind::kick, DrumPieceKind::snare, DrumPieceKind::hiHat });

    return menu;
}

void DrumKitView::showAddPieceMenu (juce::Component& anchor)
{
    buildAddMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&anchor));
}

void DrumKitView::showPieceMenu (const DrumPiece& piece)
{
    juce::PopupMenu menu;
    menu.addSectionHeader (nameOf (piece));

    const auto& part = currentPart (piece);

    if (const auto* zone = ctx.model.findZoneForDrumPart (part.id))
    {
        const auto zoneId = zone->id;
        menu.addItem (nameOf (part) + " im Editor öffnen"_u, [this, zoneId]
        {
            ctx.model.selectZone (zoneId);
            if (onOpenPart != nullptr)
                onOpenPart (zoneId);
        });
    }

    int filled = 0;
    for (const auto* id : piece.articulations)
        if (const auto* zone = ctx.model.findZoneForDrumPart (id))
            filled += zone->numSamples();

    menu.addItem (filled > 0 ? "Aus dem Kit nehmen (samt Spuren)"_u : juce::String ("Aus dem Kit nehmen"), [this, &piece]
    {
        ctx.step (nameOf (piece) + " entfernt");
        ctx.model.removeKitPiece (piece);
        ctx.toast (nameOf (piece) + " entfernt · Strg+Z holt es zurück"_u);
    });

    menu.addSeparator();
    menu.addSubMenu ("Teil hinzufügen"_u, buildAddMenu());

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

//==============================================================================
bool DrumKitView::isInterestedInDragSource (const SourceDetails& details)
{
    return sampleDrop::indexOf (details) >= 0;
}

void DrumKitView::itemDragMove (const SourceDetails& details)
{
    const auto* target = hitAt (details.localPosition.toFloat()).part;

    if (target != dropTarget)
    {
        dropTarget = target;
        repaint();
    }
}

void DrumKitView::itemDragExit (const SourceDetails&)
{
    dropTarget = nullptr;
    repaint();
}

void DrumKitView::itemDropped (const SourceDetails& details)
{
    dropTarget = nullptr;
    const auto hit = hitAt (details.localPosition.toFloat());

    if (hit.piece == nullptr || hit.part == nullptr)
    {
        ctx.toast ("Auf ein Teil des Kits ziehen"_u);
        repaint();
        return;
    }

    articulation[hit.piece->id] = hit.part->id;
    sampleDrop::intoDrumPart (ctx, *hit.part, sampleDrop::indexOf (details));
    flash (*hit.part);
}

void DrumKitView::flash (const DrumPart& part)
{
    flashing = &part;
    flashStart = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

void DrumKitView::timerCallback()
{
    if (juce::Time::getMillisecondCounterHiRes() - flashStart > flashSeconds * 1000.0)
    {
        flashing = nullptr;
        stopTimer();
    }

    repaint();
}
} // namespace sis
