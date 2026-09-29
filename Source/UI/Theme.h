#pragma once

#include <JuceHeader.h>

/** Design-Tokens aus der HTML-Referenz (README, Abschnitt "Design-Tokens").
    Alle Maße in CSS-Pixeln bei 1440 × 900. */
namespace sis
{
namespace colours
{
    inline const juce::Colour background      { 0xffe3e1dc };
    inline const juce::Colour windowBar       { 0xffdedbd5 };
    inline const juce::Colour windowBarLine   { 0xffc4c0b8 };
    inline const juce::Colour windowText      { 0xff3a3833 };
    inline const juce::Colour windowHover     { 0xffcfccc6 };

    inline const juce::Colour bars            { 0xffedebe7 };   // Menü- und Werkzeugleiste
    inline const juce::Colour panel           { 0xfff4f3f0 };   // Spalten, Statusleiste
    inline const juce::Colour workspace       { 0xffe9e7e3 };   // Arbeitsfläche Mitte
    inline const juce::Colour surface         { 0xfffbfaf8 };   // Flächen, Felder, Karten
    inline const juce::Colour lowerZone       { 0xfff1efeb };   // Sample-Editor unten
    inline const juce::Colour white           { 0xffffffff };

    inline const juce::Colour lineFine        { 0xffe1ded8 };
    inline const juce::Colour line            { 0xffd6d3cd };
    inline const juce::Colour lineStrong      { 0xffbfbbb3 };
    inline const juce::Colour lineStrongAlt   { 0xffc4c0b8 };
    inline const juce::Colour divider         { 0xffe4e1db };

    inline const juce::Colour text            { 0xff191817 };
    inline const juce::Colour textSecondary   { 0xff56534e };
    inline const juce::Colour textTertiary    { 0xff6e6b66 };

    inline const juce::Colour accent          { 0xff7a6cf0 };
    inline const juce::Colour accentHover     { 0xff5546ce };
    inline const juce::Colour accentDark      { 0xff3b2fa8 };
    inline const juce::Colour accentSoft      { 0xffeeebfe };
    inline const juce::Colour accentSoftHover { 0xfff1effd };

    inline const juce::Colour warning         { 0xffb4553f };   // Warnung / Mute
    inline const juce::Colour sliderTrack     { 0xffdcd9d3 };
    inline const juce::Colour rowHover        { 0xffe9e7e2 };
    inline const juce::Colour miniWave        { 0xffbfbbb3 };

    inline const juce::Colour zoneIdle        { 0xfff3f1ed };
    inline const juce::Colour keyWhite        { 0xfffdfdfc };
    inline const juce::Colour keyLine         { 0xffdcd9d3 };
    inline const juce::Colour keyBlack        { 0xff26241f };
    inline const juce::Colour keyBlackInZone  { 0xff4a4380 };

    inline const juce::Colour waveInactive    { 0xffc9c6bf };
    inline const juce::Colour waveOutside     { 0xffe4e1db };

    inline const juce::Colour toast           { 0xff191817 };
    inline const juce::Colour toastText       { 0xfff4f3f0 };
} // namespace colours

namespace metrics
{
    constexpr int titleBar = 34;
    constexpr int menuBar = 26;
    constexpr int toolBar = 46;
    constexpr int statusBar = 30;
    constexpr int leftPanel = 264;
    constexpr int rightPanel = 286;
    constexpr int panelHeader = 34;
    constexpr int trackHeader = 140;
    constexpr int trackHeight = 78;
    constexpr int clipHeader = 15;
    constexpr int lowerZone = 186;
    constexpr int keyboard = 92;
    constexpr int pluginKeyboard = 72;
    constexpr int pluginZoneStrip = 50;
    constexpr float blackKeyRatio = 56.0f / 92.0f;   // Höhe schwarze Taste / Klaviatur
    constexpr float blackKeyWidth = 0.62f;           // Anteil einer weißen Taste
    constexpr int keyHighlightMs = 340;
    constexpr int toastMs = 2200;
} // namespace metrics
} // namespace sis
