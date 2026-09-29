#pragma once

#include <JuceHeader.h>

#include "../../Export/InstrumentExporter.h"
#include "../Shell/StudioContext.h"
#include "../Widgets.h"

namespace sis
{
/** Export-Ansicht: Kennzahlen, Zielformate, Zielordner, Fortschritt.

    Wirklich geschrieben werden Projektdatei (.sisp) und die verwendeten Samples.
    Die Plugin-Formate stehen in der Liste, sind aber noch nicht wählbar – dafür
    fehlt eine mitgelieferte Plugin-Vorlage. */
class ExportView final : public juce::Component,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    explicit ExportView (StudioContext&);
    ~ExportView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    /** Zielformate in der Liste: VST3, Standalone, Projektdatei.
        Audio Unit fehlt bewusst - siehe ExportView.cpp. */
    static constexpr size_t numTargets = 3;

    struct TargetRow
    {
        juce::String name, note;
        bool available = false;
        bool* flag = nullptr;          // zeigt auf das Feld in ExportTargets
        juce::String unavailableNote;  // warum es (noch) nicht geht
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void chooseFolder();
    void startExport();
    juce::Array<juce::File> collectUsedSamples() const;
    juce::int64 getSampleBytes() const;

    /** Ob im Instrument fremde Plugins stecken - die wandern nicht mit ins Bundle. */
    bool usesExternalPlugins() const;
    juce::int64 pluginTemplateBytes() const;
    juce::File getDefaultFolder() const;
    juce::String sizeText (juce::int64 bytes) const;

    StudioContext& ctx;
    InstrumentExporter exporter;

    FlatButton folderButton { "Ordner …"_u }, exportButton { "Exportieren" };
    juce::File targetFolder;
    juce::File pluginTemplate;
    juce::File standaloneTemplate;
    bool copySamples = true;
    juce::String statusText;
    double displayedProgress = 0.0;

    std::array<TargetRow, numTargets> targetRows;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> titleArea, cardsArea, optionArea, pathArea, progressArea, percentArea, statusArea;
    std::array<juce::Rectangle<int>, 4> cardAreas;
    std::array<juce::Rectangle<int>, numTargets> targetAreas;
    int hoverRow = -1;
};
} // namespace sis
