#pragma once

#include "../Theme.h"
#include "../Widgets.h"

namespace sis
{
/** Kleines Fenster mit Entwickler, Version und einer Zeile darüber, was das Programm tut.

    Früher gab es daneben noch einen Eintrag „Über …“ mit demselben Zweck; beides ist
    hier zusammengeführt. Die Version kommt aus `Source/Version.h`, nicht aus einer
    Zeichenkette an dieser Stelle. */
class CreditsWindow final : public juce::Component
{
public:
    /** Öffnet das Fenster über `parent`. */
    static void show (juce::Component* parent);

    CreditsWindow();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void close();

    FlatButton closeButton { "Schließen"_u };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CreditsWindow)
};
} // namespace sis
