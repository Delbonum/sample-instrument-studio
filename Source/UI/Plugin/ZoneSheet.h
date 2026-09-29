#pragma once

#include <JuceHeader.h>

#include "../Editor/SampleEditor.h"
#include "../Shell/StudioContext.h"
#include "../Widgets.h"

namespace sis
{
/** Zonen-Dialog der Plugin-Ansicht (Doppelklick auf eine Zone).

    Legt sich über das ganze Plugin-Fenster und zeigt mittig eine Karte: Spuren der Zone
    und darunter den **gleichen** Sample-Editor wie im Standalone – nicht eine zweite,
    abgespeckte Fassung. Das ist der Grund für den `StudioContext` in der Plugin-Ansicht:
    `SampleEditor` arbeitet gegen ihn, und ein zweiter Editor müsste jede Änderung am
    ersten nachziehen. */
class ZoneSheet final : public juce::Component, private juce::ChangeListener
{
public:
    explicit ZoneSheet (StudioContext&);
    ~ZoneSheet() override;

    /** Knopf „Im Studio bearbeiten“ in der Fußzeile. */
    std::function<void()> onOpenStudio;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    /** Zeile einer Spur zeichnen; `index` zählt innerhalb der gewählten Zone. */
    void paintTrackRow (juce::Graphics&, const Track&, int index, juce::Rectangle<int>) const;

    void close();

    static constexpr int cardMaxWidth = 720;
    static constexpr int rowHeight = 34;
    static constexpr int editorHeight = 154;   // 34 Kopf + 96 Welle + 2 × 12 Rand

    StudioContext& ctx;
    FlatButton closeButton { "×"_u }, studioButton { "Im Studio bearbeiten"_u };
    SampleEditor editor;

    juce::Rectangle<int> card, headerArea, footerArea;
    std::vector<juce::Rectangle<int>> rowAreas, muteAreas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZoneSheet)
};
} // namespace sis
