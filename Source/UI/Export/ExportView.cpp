#include "ExportView.h"
#include "../../PluginProcessor.h"
#include "../StudioLookAndFeel.h"

#include <set>

namespace sis
{
namespace
{
    constexpr int padX = 16;
    constexpr int padY = 14;
    constexpr int cardHeight = 72;
    constexpr int targetHeight = 46;
    constexpr int targetGap = 6;
    constexpr int maxListWidth = 620;
    constexpr int boxSize = 15;
}

ExportView::ExportView (StudioContext& c) : ctx (c)
{
    auto& targets = ctx.model.targets;

    pluginTemplate = InstrumentExporter::findPluginTemplate();
    standaloneTemplate = InstrumentExporter::findStandaloneTemplate();

    /* Audio Unit steht hier nicht: das Format gibt es nur unter macOS, und solange es
       keine Mac-Fassung gibt, wäre die Zeile ein Versprechen ohne Deckung. Kommt sie
       zurück, gehört sie zwischen VST3 und Standalone (siehe README). */
    targetRows[0] = { "VST3-Instrument", "Cubase, Live, Reaper, Studio One",
                      pluginTemplate.isDirectory(), &targets.vst3,
                      "Plugin-Vorlage nicht gefunden"_u };
    targetRows[1] = { "Standalone-App", "Ohne DAW spielbar", standaloneTemplate.existsAsFile(),
                      &targets.standalone, "Programmvorlage nicht gefunden"_u };
    targetRows[2] = { "Projektdatei .sisp"_u, "Weiterbearbeiten, teilen", true, &targets.project, {} };

    FlatButton::Style plain;
    plain.background = colours::white;
    plain.border = colours::lineStrongAlt;
    plain.text = colours::text;
    plain.fontSize = 11.5f;
    folderButton.setStyle (plain);
    folderButton.onClick = [this] { chooseFolder(); };

    FlatButton::Style accent;
    accent.background = colours::accent;
    accent.hoverBackground = colours::accentHover;
    accent.text = colours::white;
    accent.fontSize = 12.5f;
    accent.weight = Weight::medium;
    exportButton.setStyle (accent);
    exportButton.onClick = [this] { startExport(); };

    addAndMakeVisible (folderButton);
    addAndMakeVisible (exportButton);

    targetFolder = getDefaultFolder();
    ctx.model.addChangeListener (this);
}

ExportView::~ExportView()
{
    ctx.model.removeChangeListener (this);
    exporter.cancelAndWait();
}

//==============================================================================
juce::File ExportView::getDefaultFolder() const
{
    const auto documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    return documents.getChildFile ("Instrumente")
                    .getChildFile (juce::File::createLegalFileName (ctx.model.name));
}

juce::Array<juce::File> ExportView::collectUsedSamples() const
{
    juce::Array<juce::File> files;
    std::set<juce::String> seen;

    for (const auto& zone : ctx.model.zones)
    {
        for (const auto& track : zone.tracks)
        {
            const auto* sample = ctx.model.findSample (track.clip);

            if (sample == nullptr || ! sample->file.existsAsFile())
                continue;

            const auto path = sample->file.getFullPathName();

            if (seen.insert (path).second)
                files.add (sample->file);
        }
    }

    return files;
}

juce::int64 ExportView::getSampleBytes() const
{
    juce::int64 bytes = 0;

    for (const auto& file : collectUsedSamples())
        bytes += file.getSize();

    return bytes;
}

juce::int64 ExportView::pluginTemplateBytes() const
{
    if (! pluginTemplate.isDirectory())
        return 0;

    juce::int64 bytes = 0;

    for (const auto& file : pluginTemplate.findChildFiles (juce::File::findFiles, true))
        bytes += file.getSize();

    return bytes;
}

juce::String ExportView::sizeText (juce::int64 bytes) const
{
    if (bytes <= 0)
        return "0 MB";

    const double megabytes = (double) bytes / (1024.0 * 1024.0);
    return megabytes < 10.0 ? juce::String (megabytes, 1) + " MB"
                            : juce::String (juce::roundToInt (megabytes)) + " MB";
}

//==============================================================================
void ExportView::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Zielordner wählen"_u, targetFolder);

    juce::Component::SafePointer<ExportView> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe] (const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();

        if (safe == nullptr || folder == juce::File())
            return;

        safe->targetFolder = folder;
        safe->repaint();
    });
}

void ExportView::startExport()
{
    if (exporter.isExporting())
        return;

    if (targetFolder == juce::File())
        targetFolder = getDefaultFolder();

    ExportRequest request;
    request.targetFolder = targetFolder;
    request.instrumentName = ctx.model.name.isNotEmpty() ? ctx.model.name : "Instrument";
    request.projectState = ctx.processor.createProjectState();
    request.samples = collectUsedSamples();
    request.copySamples = copySamples;
    request.exportProjectFolder = ctx.model.targets.project;
    request.exportPlugin = ctx.model.targets.vst3 && pluginTemplate.isDirectory();
    request.pluginTemplate = pluginTemplate;
    request.exportStandalone = ctx.model.targets.standalone && standaloneTemplate.existsAsFile();
    request.standaloneTemplate = standaloneTemplate;

    if (! request.exportProjectFolder && ! request.exportPlugin && ! request.exportStandalone)
    {
        ctx.toast ("Kein Ziel gewählt"_u);
        return;
    }

    if (request.copySamples && request.samples.isEmpty())
        ctx.toast ("Keine Samples zugewiesen – es wird nur die Projektdatei geschrieben"_u);

    statusText = "Exportiere …"_u;
    displayedProgress = 0.0;
    exportButton.setButtonText ("Exportiere …"_u);

    juce::Component::SafePointer<ExportView> safe (this);
    const bool started = exporter.start (std::move (request), [safe] (ExportOutcome outcome)
    {
        if (safe == nullptr)
            return;

        safe->stopTimer();
        safe->displayedProgress = outcome.succeeded ? 1.0 : 0.0;
        safe->exportButton.setButtonText ("Exportieren");
        auto written = outcome.projectFile;

        if (outcome.standaloneApp != juce::File())
            written = outcome.standaloneApp;

        if (outcome.pluginBundle != juce::File())
            written = outcome.pluginBundle;

        safe->statusText = outcome.succeeded ? outcome.message + " · "_u + written.getFullPathName()
                                             : "Fehlgeschlagen: "_u + outcome.message;
        safe->repaint();
        safe->ctx.toast (outcome.succeeded ? "Exportiert · "_u + written.getFileName() : outcome.message);
    });

    if (started)
        startTimerHz (30);
}

//==============================================================================
void ExportView::resized()
{
    auto area = getLocalBounds().reduced (padX, padY);

    titleArea = area.removeFromTop (20);
    area.removeFromTop (12);

    // Vier Kennzahlen-Karten nebeneinander, bei schmalem Fenster zwei Reihen
    const bool wide = area.getWidth() >= 4 * 150 + 30;
    auto cards = area.removeFromTop (wide ? cardHeight : 2 * cardHeight + 10);
    cardsArea = cards;

    if (wide)
    {
        const int cardWidth = (cards.getWidth() - 30) / 4;

        for (int i = 0; i < 4; ++i)
            cardAreas[(size_t) i] = { cards.getX() + i * (cardWidth + 10), cards.getY(), cardWidth, cardHeight };
    }
    else
    {
        const int cardWidth = (cards.getWidth() - 10) / 2;

        for (int i = 0; i < 4; ++i)
            cardAreas[(size_t) i] = { cards.getX() + (i % 2) * (cardWidth + 10),
                                      cards.getY() + (i / 2) * (cardHeight + 10), cardWidth, cardHeight };
    }

    area.removeFromTop (14);

    auto list = area.removeFromLeft (juce::jmin (maxListWidth, area.getWidth()));

    for (size_t i = 0; i < targetAreas.size(); ++i)
    {
        targetAreas[i] = list.removeFromTop (targetHeight);
        list.removeFromTop (targetGap);
    }

    optionArea = list.removeFromTop (30);
    list.removeFromTop (14);

    auto path = list.removeFromTop (34);
    const int buttonWidth = folderButton.getTextWidth() + 24;
    folderButton.setBounds (path.removeFromRight (buttonWidth));
    path.removeFromRight (10);
    pathArea = path;

    list.removeFromTop (14);

    auto exportRow = list.removeFromTop (36);
    exportButton.setBounds (exportRow.removeFromLeft (exportButton.getTextWidth() + 36).withSizeKeepingCentre (
        exportButton.getTextWidth() + 36, 34));
    exportRow.removeFromLeft (14);
    percentArea = exportRow.removeFromRight (88);
    exportRow.removeFromRight (14);
    progressArea = exportRow.withSizeKeepingCentre (exportRow.getWidth(), 6);

    list.removeFromTop (10);
    statusArea = list.removeFromTop (34);
}

void ExportView::paint (juce::Graphics& g)
{
    g.fillAll (colours::workspace);

    g.setColour (colours::text);
    g.setFont (sansFont (13.0f, Weight::semibold));
    g.drawText ("Instrument exportieren", titleArea, juce::Justification::centredLeft, false);

    // Kennzahlen
    const auto* zone = ctx.model.getSelectedZone();
    const juce::String labels[] = { "Zonen", "Samples", "Spuren in Zone", "Größe"_u };
    const juce::String values[] = { juce::String ((int) ctx.model.zones.size()),
                                    juce::String ((int) ctx.model.samples.size()),
                                    juce::String (zone != nullptr ? (int) zone->tracks.size() : 0),
                                    sizeText (getSampleBytes()) };

    for (size_t i = 0; i < cardAreas.size(); ++i)
    {
        const auto card = cardAreas[i];
        g.setColour (colours::surface);
        g.fillRect (card);
        g.setColour (colours::line);
        g.drawRect (card, 1);

        auto inner = card.reduced (12);
        draw::capsLabel (g, labels[i], inner.removeFromTop (13));
        inner.removeFromTop (6);
        g.setColour (colours::text);
        g.setFont (sansFont (19.0f, Weight::semibold));
        g.drawText (values[i], inner, juce::Justification::topLeft, false);
    }

    // Zielformate
    const auto sampleBytes = getSampleBytes();

    for (size_t i = 0; i < targetAreas.size(); ++i)
    {
        const auto& row = targetRows[i];
        const auto bounds = targetAreas[i];
        const bool selected = row.available && row.flag != nullptr && *row.flag;

        g.setColour (selected ? colours::accentSoft
                              : ((int) i == hoverRow && row.available ? colours::white : colours::surface));
        g.fillRect (bounds);
        g.setColour (selected ? colours::accent : colours::line);
        g.drawRect (bounds, 1);

        auto inner = bounds.reduced (12, 0);
        auto box = inner.removeFromLeft (boxSize).withSizeKeepingCentre (boxSize, boxSize);
        inner.removeFromLeft (11);

        g.setColour (selected ? colours::accent : colours::white);
        g.fillRect (box);
        g.setColour (selected ? colours::accent : (row.available ? colours::lineStrong : colours::line));
        g.drawRect (box, 1);

        if (selected)
            StudioLookAndFeel::drawTick (g, box.toFloat().reduced (2.0f), colours::white);

        const auto size = inner.removeFromRight (90);
        g.setColour (row.available ? colours::textSecondary : colours::textTertiary);
        g.setFont (monoFont (10.5f));

        juce::String sizeLabel ("—"_u);

        if (row.available)
        {
            // Jedes Ziel traegt die Samples und die Projektdatei, das Plugin und die
            // App zusaetzlich ihr eigenes Programm
            juce::int64 rowBytes = sampleBytes + 64 * 1024;

            if (i == 0)
                rowBytes += pluginTemplateBytes();
            else if (i == 1)
                rowBytes += standaloneTemplate.getSize();

            sizeLabel = sizeText (rowBytes);
        }

        g.drawText (sizeLabel, size, juce::Justification::centredRight, false);

        auto text = inner.reduced (0, 6);
        g.setColour (row.available ? colours::text : colours::textSecondary);
        g.setFont (sansFont (12.5f, Weight::medium));
        g.drawText (row.name, text.removeFromTop (16), juce::Justification::centredLeft, true);

        text.removeFromTop (2);
        g.setColour (colours::textTertiary);
        g.setFont (monoFont (10.0f));
        juce::String note = row.note;

        if (! row.available && row.unavailableNote.isNotEmpty())
            note += " · "_u + row.unavailableNote;

        g.drawText (note, text, juce::Justification::centredLeft, true);
    }

    // Option: Samples mitkopieren
    {
        auto inner = optionArea.reduced (12, 0);
        auto box = inner.removeFromLeft (boxSize).withSizeKeepingCentre (boxSize, boxSize);
        inner.removeFromLeft (11);

        g.setColour (copySamples ? colours::accent : colours::white);
        g.fillRect (box);
        g.setColour (copySamples ? colours::accent : colours::lineStrong);
        g.drawRect (box, 1);

        if (copySamples)
            StudioLookAndFeel::drawTick (g, box.toFloat().reduced (2.0f), colours::white);

        g.setColour (colours::text);
        g.setFont (sansFont (12.0f));
        g.drawText ("Samples in den Zielordner kopieren", inner, juce::Justification::centredLeft, true);
    }

    // Zielordner
    g.setColour (colours::surface);
    g.fillRect (pathArea);
    g.setColour (colours::line);
    g.drawRect (pathArea, 1);
    g.setColour (colours::textSecondary);
    g.setFont (monoFont (11.0f));
    g.drawText (targetFolder.getFullPathName(), pathArea.reduced (10, 0), juce::Justification::centredLeft, true);

    // Fortschritt
    g.setColour (colours::sliderTrack);
    g.fillRect (progressArea);
    g.setColour (colours::accent);
    g.fillRect (progressArea.withWidth (juce::roundToInt (displayedProgress * (double) progressArea.getWidth())));

    g.setColour (colours::textSecondary);
    g.setFont (monoFont (11.0f));
    g.drawText (exporter.isExporting() ? juce::String (juce::roundToInt (displayedProgress * 100.0)) + " %"
                                       : (displayedProgress >= 1.0 ? juce::String ("fertig") : juce::String()),
                percentArea, juce::Justification::centredRight, false);

    if (statusText.isNotEmpty())
    {
        g.setColour (colours::textTertiary);
        g.setFont (monoFont (10.0f));
        g.drawFittedText (statusText, statusArea, juce::Justification::topLeft, 2);
    }
    else if (usesExternalPlugins())
    {
        // Fremde Plugins stecken nicht im Bundle - auf einem anderen Rechner fehlen sie
        g.setColour (colours::warning);
        g.setFont (monoFont (10.0f));
        g.drawFittedText ("Achtung: fremde Plugins werden nicht mitgeliefert. „Zone bouncen“ im "
                          "Editor rechnet sie fest ins Instrument."_u,
                          statusArea, juce::Justification::topLeft, 2);
    }
}

bool ExportView::usesExternalPlugins() const
{
    for (const auto& zone : ctx.model.zones)
        for (const auto& track : zone.tracks)
            for (const auto& effect : track.effects)
                if (effect.type == EffectType::external && effect.enabled)
                    return true;

    return false;
}

//==============================================================================
void ExportView::mouseMove (const juce::MouseEvent& e)
{
    int row = -1;

    for (size_t i = 0; i < targetAreas.size(); ++i)
        if (targetAreas[i].contains (e.getPosition()))
            row = (int) i;

    if (row != hoverRow)
    {
        hoverRow = row;
        setMouseCursor (row >= 0 && targetRows[(size_t) row].available ? juce::MouseCursor::PointingHandCursor
                                                                       : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ExportView::mouseExit (const juce::MouseEvent&)
{
    hoverRow = -1;
    repaint();
}

void ExportView::mouseDown (const juce::MouseEvent& e)
{
    for (size_t i = 0; i < targetAreas.size(); ++i)
    {
        if (! targetAreas[i].contains (e.getPosition()))
            continue;

        const auto& row = targetRows[i];

        if (! row.available)
        {
            ctx.toast (row.name + " braucht eine mitgelieferte Plugin-Vorlage – folgt"_u);
            return;
        }

        if (row.flag != nullptr)
        {
            ctx.step ("Exportziel umgeschaltet"_u);
            *row.flag = ! *row.flag;
            ctx.model.notifyChanged();
        }

        return;
    }

    if (optionArea.contains (e.getPosition()))
    {
        copySamples = ! copySamples;
        repaint();
    }
}

void ExportView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (targetFolder == juce::File())
        targetFolder = getDefaultFolder();

    repaint();
}

void ExportView::timerCallback()
{
    const double progress = exporter.getProgress();

    if (std::abs (progress - displayedProgress) > 0.005)
    {
        displayedProgress = progress;
        repaint (progressArea.getUnion (percentArea));
    }
}
} // namespace sis
