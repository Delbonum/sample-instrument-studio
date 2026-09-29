#pragma once

#include <JuceHeader.h>

#include "../Shell/StudioContext.h"

namespace sis
{
/** Symbole der drei Werkzeuge, als Pfade in einem 20 × 20-Feld – dieselben in der
    Werkzeugleiste und im Mauszeiger. */
namespace toolIcons
{
    juce::Path arrow();
    juce::Path eraser();
    juce::Path scissors();

    /** Mauszeiger je Werkzeug. Die Schere trägt einen senkrechten Strich, und genau auf
        ihm liegt der Klickpunkt: dort wird geschnitten. */
    juce::MouseCursor cursorFor (EditTool);
}

/** Werkzeugleiste am Mauszeiger, wie in Cubase: ein gehaltener Rechtsklick in der
    Zeitleiste öffnet sie, Loslassen über einem Symbol wählt das Werkzeug. Wer nur kurz
    klickt, behält sie offen und wählt mit einem Linksklick; ein Klick daneben oder Esc
    schließt sie. */
class ToolPalette final : public juce::Component
{
public:
    ToolPalette();
    ~ToolPalette() override;

    std::function<void (EditTool)> onChoose;
    std::function<void()> onDismiss;

    /** Werkzeug unter einem Punkt (in Koordinaten der Leiste), sonst -1. */
    int itemAt (juce::Point<int>) const;

    void setHover (int item);
    void setCurrent (EditTool);

    /** Bleibt offen, bis gewählt oder daneben geklickt wird. */
    void makeSticky();

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static constexpr int itemSize = 34;
    static constexpr int labelHeight = 14;
    static constexpr int pad = 5;

    static juce::Rectangle<int> idealSize() { return { 0, 0, 3 * itemSize + 2 * pad, itemSize + labelHeight + 2 * pad }; }

private:
    juce::Rectangle<int> itemBounds (int item) const;

    struct OutsideClick final : juce::MouseListener
    {
        std::function<void (const juce::MouseEvent&)> callback;
        void mouseDown (const juce::MouseEvent& e) override { if (callback) callback (e); }
    };

    OutsideClick outside;
    bool sticky = false;
    int hover = -1;
    EditTool current = EditTool::select;
};
} // namespace sis
