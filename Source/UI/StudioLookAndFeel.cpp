#include "StudioLookAndFeel.h"
#include "Theme.h"

namespace sis
{
namespace
{
    constexpr int menuMinWidth = 262;
    constexpr int menuItemHeight = 22;       // 5 px + Zeile + 5 px
    constexpr int menuSeparatorHeight = 7;   // 3 px + 1 px + 3 px
    constexpr int menuPadX = 8;
    constexpr int menuTickColumn = 12;
}

StudioLookAndFeel::StudioLookAndFeel()
{
    if (auto sans = fonts->getSans (Weight::regular))
        setDefaultSansSerifTypeface (sans);

    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::DocumentWindow::textColourId, colours::windowText);

    setColour (juce::PopupMenu::backgroundColourId, colours::white);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accentSoftHover);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);

    setColour (juce::TextEditor::backgroundColourId, colours::surface);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::outlineColourId, colours::line);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    setColour (juce::TextEditor::highlightColourId, juce::Colour (0xffdad5fc));
    setColour (juce::TextEditor::highlightedTextColourId, colours::text);
    setColour (juce::CaretComponent::caretColourId, colours::text);

    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xffcfcbc3));
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);

    setColour (juce::AlertWindow::backgroundColourId, colours::surface);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::lineStrong);
    setColour (juce::TextButton::buttonColourId, colours::surface);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::ListBox::backgroundColourId, colours::white);
    setColour (juce::ListBox::outlineColourId, colours::lineStrongAlt);
    setColour (juce::ListBox::textColourId, colours::text);
    setColour (juce::ToggleButton::textColourId, colours::text);
    setColour (juce::ToggleButton::tickColourId, colours::accentHover);
    setColour (juce::ToggleButton::tickDisabledColourId, colours::lineStrong);
    setColour (juce::ComboBox::backgroundColourId, colours::white);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::lineStrongAlt);
}

juce::Typeface::Ptr StudioLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    if (font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        if (auto tf = fonts->getSans (font.isBold() ? Weight::semibold : Weight::regular))
            return tf;

    if (font.getTypefaceName() == juce::Font::getDefaultMonospacedFontName())
        if (auto tf = fonts->getMono (font.isBold() ? Weight::medium : Weight::regular))
            return tf;

    return LookAndFeel_V4::getTypefaceForFont (font);
}

//==============================================================================
void StudioLookAndFeel::drawMenuBarBackground (juce::Graphics& g, int width, int height, bool,
                                               juce::MenuBarComponent&)
{
    g.fillAll (colours::bars);
    g.setColour (colours::line);
    g.fillRect (0, height - 1, width, 1);
}

void StudioLookAndFeel::drawMenuBarItem (juce::Graphics& g, int width, int height, int, const juce::String& itemText,
                                         bool, bool isMenuOpen, bool, juce::MenuBarComponent&)
{
    if (isMenuOpen)
    {
        g.setColour (colours::surface);
        g.fillRect (0, 0, width, height - 1);
    }

    g.setColour (isMenuOpen ? colours::text : colours::windowText);
    g.setFont (sansFont (12.0f));
    g.drawText (itemText, 0, 0, width, height - 1, juce::Justification::centred, false);
}

juce::Font StudioLookAndFeel::getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&)
{
    return sansFont (12.0f);
}

int StudioLookAndFeel::getMenuBarItemWidth (juce::MenuBarComponent&, int, const juce::String& itemText)
{
    return juce::roundToInt (textWidth (sansFont (12.0f), itemText)) + 20;   // padding 0 10px
}

//==============================================================================
void StudioLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (colours::white);
    g.setColour (colours::lineStrong);
    g.drawRect (0, 0, width, height, 1);
}

void StudioLookAndFeel::drawTick (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    const auto r = area.withSizeKeepingCentre (8.0f, 6.0f);
    juce::Path tick;
    tick.startNewSubPath (r.getX(), r.getCentreY());
    tick.lineTo (r.getX() + r.getWidth() * 0.36f, r.getBottom());
    tick.lineTo (r.getRight(), r.getY());
    g.setColour (colour);
    g.strokePath (tick, juce::PathStrokeType (1.3f));
}

void StudioLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                           bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                           const juce::String& text, const juce::String& shortcutKeyText,
                                           const juce::Drawable*, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (colours::divider);
        g.fillRect (area.getX(), area.getCentreY(), area.getWidth(), 1);
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (colours::accentSoftHover);
        g.fillRect (area);
    }

    auto r = area.reduced (menuPadX, 0);
    const auto tickArea = r.removeFromLeft (menuTickColumn);
    r.removeFromLeft (8);

    if (isTicked)
        drawTick (g, tickArea.toFloat(), colours::accentHover);

    const float alpha = isActive ? 1.0f : 0.45f;

    if (shortcutKeyText.isNotEmpty())
    {
        const auto keyFont = monoFont (10.0f);
        const int keyWidth = juce::roundToInt (textWidth (keyFont, shortcutKeyText)) + 2;
        g.setFont (keyFont);
        g.setColour (colours::textTertiary.withMultipliedAlpha (alpha));
        g.drawText (shortcutKeyText, r.removeFromRight (keyWidth), juce::Justification::centredRight, false);
        r.removeFromRight (8);
    }

    if (hasSubMenu)
    {
        auto arrow = r.removeFromRight (10).toFloat().withSizeKeepingCentre (4.0f, 7.0f);
        juce::Path p;
        p.addTriangle (arrow.getTopLeft(), arrow.getBottomLeft(), { arrow.getRight(), arrow.getCentreY() });
        g.setColour (colours::textTertiary);
        g.fillPath (p);
    }

    g.setFont (sansFont (12.0f));
    g.setColour ((textColour != nullptr ? *textColour : colours::text).withMultipliedAlpha (alpha));
    g.drawText (text, r, juce::Justification::centredLeft, true);
}

juce::Font StudioLookAndFeel::getPopupMenuFont()
{
    return sansFont (12.0f);
}

void StudioLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int,
                                                   int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 50;
        idealHeight = menuSeparatorHeight;
        return;
    }

    idealHeight = menuItemHeight;
    const int contentWidth = juce::roundToInt (textWidth (sansFont (12.0f), text))
                             + menuPadX * 2 + menuTickColumn + 8 + 16;
    idealWidth = juce::jmax (menuMinWidth - 2 * getPopupMenuBorderSize(), contentWidth);
}

//==============================================================================
void StudioLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    g.setColour (colours::white);
    g.fillRect (0, 0, width, height);
    g.setColour (box.hasKeyboardFocus (false) ? colours::accent : colours::lineStrongAlt);
    g.drawRect (0, 0, width, height, 1);

    // Pfeil nach unten
    juce::Path arrow;
    const float cx = (float) width - 12.0f, cy = (float) height * 0.5f;
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (colours::textSecondary);
    g.fillPath (arrow);
}

juce::Font StudioLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return sansFont (11.5f);
}

void StudioLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 24, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
void StudioLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRect (0, 0, width, height);
}

void StudioLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    const bool focused = editor.hasKeyboardFocus (true) && ! editor.isReadOnly();
    g.setColour (editor.findColour (focused ? juce::TextEditor::focusedOutlineColourId
                                            : juce::TextEditor::outlineColourId));
    g.drawRect (0, 0, width, height, 1);
}

void StudioLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int width, int height,
                                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                       bool isMouseOver, bool isMouseDown)
{
    juce::Rectangle<int> thumb;

    if (isScrollbarVertical)
        thumb = { x, thumbStartPosition, width, thumbSize };
    else
        thumb = { thumbStartPosition, y, thumbSize, height };

    auto colour = bar.findColour (juce::ScrollBar::thumbColourId);
    if (isMouseOver || isMouseDown)
        colour = colour.darker (0.1f);

    // border: 3px transparent; background-clip: content-box
    g.setColour (colour);
    g.fillRoundedRectangle (thumb.toFloat().reduced (3.0f), 2.0f);
}
} // namespace sis
