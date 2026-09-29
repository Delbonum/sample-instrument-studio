#include "SampleBrowser.h"
#include "../../PluginProcessor.h"
#include "../SampleDrop.h"
#include "../Widgets.h"

namespace sis
{
SampleList::SampleList (InstrumentModel& m) : model (m)
{
    refresh();
}

void SampleList::setFilter (const juce::String& text)
{
    filter = text.trim().toLowerCase();
    refresh();
}

void SampleList::refresh()
{
    visible.clear();

    for (int i = 0; i < (int) model.samples.size(); ++i)
        if (filter.isEmpty() || model.samples[(size_t) i].name.toLowerCase().contains (filter))
            visible.push_back (i);

    hoverRow = -1;
    repaint();
}

int SampleList::getContentHeight() const
{
    return (int) visible.size() * rowHeight + 8;   // padding-bottom 8px
}

int SampleList::rowAt (juce::Point<int> p) const
{
    if (p.x < padX || p.x >= getWidth() - padX)
        return -1;

    const int row = p.y / rowHeight;
    return juce::isPositiveAndBelow (row, (int) visible.size()) ? row : -1;
}

void SampleList::paint (juce::Graphics& g)
{
    const auto nameFont = sansFont (12.0f);
    const auto metaFont = monoFont (10.0f);

    for (int row = 0; row < (int) visible.size(); ++row)
    {
        const int index = visible[(size_t) row];
        const auto& sample = model.samples[(size_t) index];
        const bool selected = index == model.selectedSample;

        auto r = juce::Rectangle<int> (padX, row * rowHeight, getWidth() - 2 * padX, rowHeight);

        if (row == hoverRow || selected)
        {
            g.setColour (row == hoverRow ? colours::rowHover : colours::accentSoft);
            g.fillRect (r);
        }

        r = r.reduced (6, 7);

        // Mini-Wellenform 42 × 22 px, 14 Balken
        const auto wave = r.removeFromLeft (42).withSizeKeepingCentre (42, 22).toFloat();
        draw::waveBars (g, wave, sample.peaks, 14, 1.0f, 0.1f, selected ? colours::accent : colours::miniWave);
        r.removeFromLeft (9);

        g.setColour (colours::text);
        g.setFont (nameFont);
        g.drawText (sample.name, r.removeFromTop (r.getHeight() / 2 + 1), juce::Justification::bottomLeft, true);

        g.setColour (colours::textTertiary);
        g.setFont (metaFont);
        g.drawText (sample.metaText(), r, juce::Justification::centredLeft, true);
    }
}

void SampleList::mouseMove (const juce::MouseEvent& e)
{
    const int row = rowAt (e.getPosition());
    if (row != hoverRow)
    {
        hoverRow = row;
        repaint();
    }
}

void SampleList::mouseExit (const juce::MouseEvent&)
{
    hoverRow = -1;
    repaint();
}

void SampleList::mouseDown (const juce::MouseEvent& e)
{
    const int row = rowAt (e.getPosition());
    if (row >= 0)
    {
        model.selectedSample = visible[(size_t) row];
        model.notifyChanged();
    }
}

void SampleList::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int row = rowAt (e.getPosition());

    if (row >= 0 && onSampleChosen != nullptr)
        onSampleChosen (visible[(size_t) row]);
}

void SampleList::mouseDrag (const juce::MouseEvent& e)
{
    /* Eine Zeile lässt sich in die Mitte ziehen – auf ein Kit-Teil, eine Zone, eine Taste
       oder eine Spur im Editor. Die kleine Schwelle hält einen etwas zittrigen Klick davon
       ab, gleich ein Ziehen zu werden. */
    if (dragging || e.getDistanceFromDragStart() < 5)
        return;

    const int row = rowAt (e.getMouseDownPosition());

    if (row < 0)
        return;

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);

    if (container == nullptr)
        return;

    dragging = true;

    const auto rowArea = juce::Rectangle<int> (padX, row * rowHeight, getWidth() - 2 * padX, rowHeight);
    const float scale = juce::Component::getApproximateScaleFactorForComponent (this);
    auto image = createComponentSnapshot (rowArea, true, scale);
    image.multiplyAllAlphas (0.85f);

    const juce::Point<int> grab (e.getMouseDownX() - rowArea.getX(), e.getMouseDownY() - rowArea.getY());
    container->startDragging (sampleDrop::describe (visible[(size_t) row]), this,
                              juce::ScaledImage (image, scale), false, &grab);
}

void SampleList::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

//==============================================================================
SampleBrowser::SampleBrowser (StudioContext& c) : ctx (c), list (c.model)
{
    search.setTextToShowWhenEmpty ("Suchen …"_u, colours::textTertiary);
    search.setFont (sansFont (12.0f));
    search.setIndents (8, 6);
    search.setJustification (juce::Justification::centredLeft);
    search.onTextChange = [this]
    {
        list.setFilter (search.getText());
        layoutList();
    };
    search.onEscapeKey = [this]
    {
        search.clear();
        list.setFilter ({});
        layoutList();
        search.giveAwayKeyboardFocus();
    };
    addAndMakeVisible (search);

    list.onSampleChosen = [this] (int index) { assignSampleToZone (index); };

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    ctx.model.addChangeListener (this);
}

SampleBrowser::~SampleBrowser()
{
    ctx.model.removeChangeListener (this);
}

void SampleBrowser::assignSampleToZone (int sampleIndex)
{
    auto& model = ctx.model;

    if (! juce::isPositiveAndBelow (sampleIndex, (int) model.samples.size()))
        return;

    auto* zone = model.getSelectedZone();

    if (zone == nullptr)
    {
        ctx.toast ("Erst eine Zone im Mapping wählen"_u);
        return;
    }

    const auto& sample = model.samples[(size_t) sampleIndex];

    if (! sample.file.existsAsFile())
    {
        ctx.toast (sample.name + " ist ein Beispiel-Eintrag ohne Datei"_u);
        return;
    }

    ctx.step (sample.name + " als Spur angelegt"_u);
    model.addSampleTrack (*zone, sample);

    ctx.ui.view = View::editor;
    ctx.ui.changed();
    ctx.toast (sample.name + " in Zone "_u + zone->name + " eingefügt"_u);
}

void SampleBrowser::setBorderOnLeft (bool shouldBeOnLeft)
{
    if (borderOnLeft != shouldBeOnLeft)
    {
        borderOnLeft = shouldBeOnLeft;
        resized();
        repaint();
    }
}

void SampleBrowser::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::line);
    g.fillRect (borderOnLeft ? 0 : getWidth() - 1, 0, 1, getHeight());

    auto header = getLocalBounds().withTrimmedRight (borderOnLeft ? 0 : 1)
                                  .withTrimmedLeft (borderOnLeft ? 1 : 0)
                                  .removeFromTop (metrics::panelHeader);
    draw::bottomLine (g, header, colours::lineFine);
    draw::capsLabel (g, "Samples", header.reduced (12, 0), 10.5f);

    // Ablagefläche: gestrichelter Rand
    const auto drop = dropArea.toFloat();
    const float dashes[] = { 3.0f, 3.0f };
    g.setColour (dragOver ? colours::accent : colours::lineStrongAlt);
    for (const auto& edge : { juce::Line<float> (drop.getTopLeft(), drop.getTopRight()),
                              juce::Line<float> (drop.getTopRight(), drop.getBottomRight()),
                              juce::Line<float> (drop.getBottomRight(), drop.getBottomLeft()),
                              juce::Line<float> (drop.getBottomLeft(), drop.getTopLeft()) })
        g.drawDashedLine (edge, dashes, 2, 1.0f);

    if (dragOver)
    {
        g.setColour (colours::accentSoft.withAlpha (0.6f));
        g.fillRect (dropArea.reduced (1));
    }

    auto text = dropArea.withSizeKeepingCentre (dropArea.getWidth() - 20, 33);
    g.setColour (colours::textSecondary);
    g.setFont (sansFont (11.5f));
    g.drawText ("Dateien hierher ziehen", text.removeFromTop (16), juce::Justification::centred, true);
    text.removeFromTop (4);
    g.setColour (colours::textTertiary);
    g.setFont (monoFont (10.0f));
    g.drawText ("WAV · AIFF · FLAC · MP3"_u, text, juce::Justification::centred, true);
}

void SampleBrowser::resized()
{
    auto area = getLocalBounds().withTrimmedRight (borderOnLeft ? 0 : 1)
                                .withTrimmedLeft (borderOnLeft ? 1 : 0);
    area.removeFromTop (metrics::panelHeader);

    auto searchRow = area.removeFromTop (49).reduced (12, 10);
    search.setBounds (searchRow);

    auto bottom = area.removeFromBottom (8 + 61 + 12);
    dropArea = bottom.withTrimmedTop (8).withTrimmedBottom (12).reduced (10, 0);

    viewport.setBounds (area);
    layoutList();
}

void SampleBrowser::layoutList()
{
    const int height = list.getContentHeight();
    const bool needsScroll = height > viewport.getHeight();
    list.setSize (viewport.getWidth() - (needsScroll ? viewport.getScrollBarThickness() : 0), height);
}

bool SampleBrowser::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (InstrumentModel::isSupportedAudioFile (f))
            return true;
    return false;
}

void SampleBrowser::fileDragEnter (const juce::StringArray&, int, int)
{
    dragOver = true;
    repaint();
}

void SampleBrowser::fileDragExit (const juce::StringArray&)
{
    dragOver = false;
    repaint();
}

void SampleBrowser::filesDropped (const juce::StringArray& paths, int, int)
{
    dragOver = false;
    repaint();

    juce::Array<juce::File> files;
    for (const auto& p : paths)
        files.add (juce::File (p));

    const int added = ctx.model.importSamples (files, ctx.processor.getFormatManager());
    ctx.toast (added == 1 ? juce::String ("1 Sample importiert")
                          : added > 0 ? juce::String (added) + " Samples importiert"
                                      : "Keine neuen Audiodateien gefunden"_u);
}

void SampleBrowser::changeListenerCallback (juce::ChangeBroadcaster*)
{
    list.refresh();
    layoutList();
}
} // namespace sis
