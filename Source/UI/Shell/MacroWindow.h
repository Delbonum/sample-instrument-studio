#pragma once

#include "StudioContext.h"
#include "../Theme.h"
#include "../Widgets.h"

namespace sis
{
/** Übersicht der Makro-Zuweisungen.

    Zugewiesen wird nicht hier, sondern mit der zweiten Maustaste auf dem Regler selbst –
    dort weiß man, was man zuweist. Dieses Fenster zeigt, was zugewiesen *ist*, und löst
    es auf Klick wieder. */
class MacroWindow final : public juce::Component,
                          private juce::ChangeListener
{
public:
    static void show (StudioContext&, juce::Component* parent);

    explicit MacroWindow (StudioContext&);
    ~MacroWindow() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    /** Beschreibt, worauf ein Makro zeigt – „EQ 4-Band · TIEFEN“. */
    juce::String describe (const MacroTarget&) const;

    StudioContext& ctx;
    FlatButton closeButton { "Schließen"_u };
    std::array<juce::Rectangle<int>, InstrumentModel::numMacros> rowAreas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroWindow)
};
} // namespace sis
