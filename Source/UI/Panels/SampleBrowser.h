#pragma once

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"

namespace sis
{
/** Liste der Samples mit Mini-Wellenform, gefiltert über das Suchfeld. */
class SampleList final : public juce::Component
{
public:
    static constexpr int rowHeight = 42;
    static constexpr int padX = 6;

    explicit SampleList (InstrumentModel&);

    /** Doppelklick auf eine Zeile: Index in model.samples. */
    std::function<void (int)> onSampleChosen;

    void setFilter (const juce::String&);
    void refresh();
    int getContentHeight() const;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int rowAt (juce::Point<int>) const;
    bool dragging = false;

    InstrumentModel& model;
    juce::String filter;
    std::vector<int> visible;   // Indizes in model.samples
    int hoverRow = -1;
};

/** Linke Spalte in Mapping und Export: Kopf, Suche, Liste, Ablagefläche. */
class SampleBrowser final : public juce::Component,
                            public juce::FileDragAndDropTarget,
                            private juce::ChangeListener
{
public:
    explicit SampleBrowser (StudioContext&);
    ~SampleBrowser() override;

    /** In der Editor-Ansicht steht der Browser rechts – dann gehört die Trennlinie nach links. */
    void setBorderOnLeft (bool);

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void layoutList();
    void assignSampleToZone (int sampleIndex);

    StudioContext& ctx;
    juce::TextEditor search;
    SampleList list;
    juce::Viewport viewport;
    juce::Rectangle<int> dropArea;
    bool dragOver = false;
    bool borderOnLeft = false;
};
} // namespace sis
