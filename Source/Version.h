#pragma once

#include <JuceHeader.h>

namespace sis
{
/** Programmversion und Urheber.

    Gepflegt wird die Version an **genau einer Stelle**: `project(... VERSION x.y.z)` in
    `CMakeLists.txt`. JUCE macht daraus `JucePlugin_VersionString`, das hier nur
    weitergereicht wird. Zwei Stellen könnten auseinanderlaufen, eine nicht.

    Beim Ändern mitziehen:
      - **x** – großer Umbau oder Bruch mit älteren Projektdateien
      - **y** – neue Funktionen
      - **z** – Fehlerbehebungen

    Der Verlauf steht im README unter „Versionen“. */
inline juce::String versionString()
{
    return JucePlugin_VersionString;
}

/** Wer das Programm gemacht hat – erscheint in den Credits. */
inline juce::String developerName()
{
    return "WiskundeKnobbel (Philippe Nix)";
}
} // namespace sis
