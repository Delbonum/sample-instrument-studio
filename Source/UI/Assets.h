#pragma once

#include <JuceHeader.h>

namespace sis
{
enum class Weight { regular, medium, semibold };

/** Eingebettete IBM-Plex-Schriften. Wird über SharedResourcePointer geteilt;
    das LookAndFeel hält eine Referenz, solange ein Fenster offen ist. */
class FontLibrary
{
public:
    FontLibrary();

    juce::Typeface::Ptr getSans (Weight) const;
    juce::Typeface::Ptr getMono (Weight) const;

private:
    juce::Typeface::Ptr sansRegular, sansMedium, sansSemibold, monoRegular, monoMedium;
};

/** IBM Plex Sans, Größe in CSS-Pixeln (em-Größe). */
juce::Font sansFont (float size, Weight = Weight::regular);

/** Das Handbuch (docs/Handbuch.html), eingebettet wie Icon und Schriften – damit F1 in jeder
    Installation und jedem Export funktioniert. `{{VERSION}}` steht darin für die Version. */
juce::String manualHtml();

/** IBM Plex Mono, Größe in CSS-Pixeln. */
juce::Font monoFont (float size, Weight = Weight::regular);

/** Versale Abschnittstitel: Mono, letter-spacing 0.09em. Text selbst in Großbuchstaben übergeben. */
juce::Font capsFont (float size = 10.0f, float tracking = 0.09f);

float textWidth (const juce::Font&, const juce::String&);

/** Programm-Icon aus assets/sis_icon.png. */
juce::Image appIcon();
} // namespace sis
