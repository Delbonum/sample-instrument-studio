#pragma once

#include <JuceHeader.h>

namespace sis
{
/** UTF-8-Literal für Oberflächentexte mit Umlauten und Sonderzeichen (·, –, …).
    juce::String (const char*) akzeptiert nur ASCII. */
inline juce::String operator""_u (const char* text, size_t length)
{
    return juce::String::fromUTF8 (text, (int) length);
}

/** Großbuchstaben, auch für Umlaute. juce::String::toUpperCase lässt sie unter Windows
    klein („TONHöHE“): die Laufzeitbibliothek kennt im Standard-Locale nur ASCII. */
inline juce::String upperCase (const juce::String& text)
{
    return text.toUpperCase()
               .replace (juce::String::fromUTF8 ("ä"), juce::String::fromUTF8 ("Ä"))
               .replace (juce::String::fromUTF8 ("ö"), juce::String::fromUTF8 ("Ö"))
               .replace (juce::String::fromUTF8 ("ü"), juce::String::fromUTF8 ("Ü"));
}

/** Notenname wie im Prototyp: 36 → "C2", 54 → "F#3". */
inline juce::String noteName (int midiNote)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((midiNote % 12) + 12) % 12]) + juce::String (midiNote / 12 - 1);
}

/** Pegel als Text: "−1.7 dB", bei Stille "-∞ dB". */
inline juce::String gainToText (float gain)
{
    if (gain <= 0.001f)
        return "-∞ dB"_u;

    return juce::String (juce::Decibels::gainToDecibels (gain), 1) + " dB";
}
} // namespace sis
