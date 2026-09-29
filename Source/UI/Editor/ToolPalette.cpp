#include "ToolPalette.h"
#include "../Widgets.h"

namespace sis
{
namespace
{
    const std::array<std::pair<EditTool, const char*>, 3> items { {
        { EditTool::select, "Auswahl" },
        { EditTool::erase,  "Löschen" },
        { EditTool::split,  "Schere" },
    } };

    juce::Path iconFor (EditTool tool)
    {
        switch (tool)
        {
            case EditTool::erase: return toolIcons::eraser();
            case EditTool::split: return toolIcons::scissors();
            case EditTool::select: break;
        }

        return toolIcons::arrow();
    }

    /** Symbol in ein Feld setzen: Pfade sind für 20 × 20 gezeichnet. */
    void drawIcon (juce::Graphics& g, const juce::Path& icon, juce::Rectangle<float> area, juce::Colour colour, bool filled)
    {
        const auto transform = juce::AffineTransform::scale (area.getWidth() / 20.0f, area.getHeight() / 20.0f)
                                   .translated (area.getX(), area.getY());
        auto path = icon;
        path.applyTransform (transform);

        if (filled)
        {
            g.setColour (colours::white);
            g.fillPath (path);
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
namespace toolIcons
{
    juce::Path arrow()
    {
        juce::Path p;
        p.startNewSubPath (4.0f, 2.0f);
        p.lineTo (4.0f, 16.5f);
        p.lineTo (7.6f, 13.2f);
        p.lineTo (10.2f, 18.5f);
        p.lineTo (12.6f, 17.4f);
        p.lineTo (10.0f, 12.2f);
        p.lineTo (15.0f, 12.2f);
        p.closeSubPath();
        return p;
    }

    juce::Path eraser()
    {
        // Schräg liegender Radiergummi mit abgesetzter Spitze
        juce::Path p;
        p.addRoundedRectangle (-7.5f, -3.5f, 15.0f, 7.0f, 1.2f);
        p.startNewSubPath (-2.0f, -3.5f);
        p.lineTo (-2.0f, 3.5f);
        p.applyTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::pi / 4.0f).translated (10.0f, 10.0f));
        p.startNewSubPath (3.0f, 18.0f);
        p.lineTo (17.0f, 18.0f);
        return p;
    }

    juce::Path scissors()
    {
        juce::Path p;
        p.addEllipse (2.5f, 12.5f, 5.5f, 5.5f);
        p.addEllipse (12.0f, 12.5f, 5.5f, 5.5f);
        p.startNewSubPath (7.0f, 13.2f);
        p.lineTo (14.5f, 2.0f);
        p.startNewSubPath (13.0f, 13.2f);
        p.lineTo (5.5f, 2.0f);
        return p;
    }

    juce::MouseCursor cursorFor (EditTool tool)
    {
        if (tool == EditTool::select)
            return juce::MouseCursor::NormalCursor;

        /* Doppelt so fein gezeichnet wie angezeigt, damit der Zeiger bei 200 % Skalierung
           nicht verschwimmt. */
        constexpr float scale = 2.0f;
        const int size = 28;
        juce::Image image (juce::Image::ARGB, (int) (size * scale), (int) (size * scale), true);
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));

        if (tool == EditTool::erase)
        {
            drawIcon (g, eraser(), { 4.0f, 2.0f, 22.0f, 22.0f }, colours::text, true);
            // Klickpunkt: die Spitze unten links
            return juce::MouseCursor (juce::ScaledImage (image, scale), { 7, 21 });
        }

        // Schere: ein senkrechter Strich über die ganze Höhe, die Schere daneben
        constexpr int cutX = 5;
        g.setColour (colours::white);
        g.fillRect (juce::Rectangle<float> ((float) cutX - 1.0f, 0.0f, 3.0f, (float) size));
        g.setColour (colours::text);
        g.fillRect (juce::Rectangle<float> ((float) cutX, 0.0f, 1.0f, (float) size));
        drawIcon (g, scissors(), { 8.0f, 6.0f, 18.0f, 18.0f }, colours::text, false);
        return juce::MouseCursor (juce::ScaledImage (image, scale), { cutX, size / 2 });
    }
}

//==============================================================================
ToolPalette::ToolPalette()
{
    setSize (idealSize().getWidth(), idealSize().getHeight());
    setAlwaysOnTop (true);

    outside.callback = [this] (const juce::MouseEvent& e)
    {
        if (e.eventComponent == this || isParentOf (e.eventComponent))
            return;

        // Nicht mitten in der Mausbehandlung abbauen
        juce::Component::SafePointer<ToolPalette> safe (this);
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr && safe->onDismiss != nullptr)
                safe->onDismiss();
        });
    };
}

ToolPalette::~ToolPalette()
{
    if (sticky)
        juce::Desktop::getInstance().removeGlobalMouseListener (&outside);
}

juce::Rectangle<int> ToolPalette::itemBounds (int item) const
{
    return { pad + item * itemSize, pad, itemSize, itemSize + labelHeight };
}

int ToolPalette::itemAt (juce::Point<int> p) const
{
    for (int i = 0; i < (int) items.size(); ++i)
        if (itemBounds (i).contains (p))
            return i;

    return -1;
}

void ToolPalette::setHover (int item)
{
    if (hover != item)
    {
        hover = item;
        repaint();
    }
}

void ToolPalette::setCurrent (EditTool tool)
{
    current = tool;
    repaint();
}

void ToolPalette::makeSticky()
{
    if (! sticky)
    {
        sticky = true;
        juce::Desktop::getInstance().addGlobalMouseListener (&outside);
    }
}

void ToolPalette::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (juce::Colours::black.withAlpha (0.08f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 5.0f);
    g.setColour (colours::surface);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (colours::lineStrong);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    for (int i = 0; i < (int) items.size(); ++i)
    {
        const auto [tool, name] = items[(size_t) i];
        const auto area = itemBounds (i).toFloat().reduced (2.0f);
        const bool isCurrent = tool == current;

        if (i == hover || isCurrent)
        {
            g.setColour (i == hover ? colours::accentSoftHover : colours::accentSoft);
            g.fillRoundedRectangle (area, 4.0f);
            g.setColour (i == hover ? colours::accent : colours::accentSoft.darker (0.1f));
            g.drawRoundedRectangle (area, 4.0f, 1.0f);
        }

        auto icon = area.withHeight ((float) itemSize - 4.0f).reduced (6.0f);
        drawIcon (g, iconFor (tool), icon, isCurrent ? colours::accentDark : colours::text, tool == EditTool::select);

        g.setColour (colours::textSecondary);
        g.setFont (monoFont (9.0f));
        g.drawText (juce::String::fromUTF8 (name), area.withTrimmedTop ((float) itemSize - 4.0f).toNearestInt(),
                    juce::Justification::centredTop, false);
    }
}

void ToolPalette::mouseMove (const juce::MouseEvent& e)
{
    setHover (itemAt (e.getPosition()));
}

void ToolPalette::mouseExit (const juce::MouseEvent&)
{
    setHover (-1);
}

void ToolPalette::mouseUp (const juce::MouseEvent& e)
{
    const int item = itemAt (e.getPosition());

    if (item < 0)
        return;

    const auto tool = items[(size_t) item].first;
    juce::Component::SafePointer<ToolPalette> safe (this);
    juce::MessageManager::callAsync ([safe, tool]
    {
        if (safe != nullptr && safe->onChoose != nullptr)
            safe->onChoose (tool);
    });
}
} // namespace sis
