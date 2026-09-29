// Prüft die Clip-Gesten aus dem README ohne Oberfläche.
// Bauen und starten:  cmake --build build/ninja --target SisClipGeometryTests && ./SisClipGeometryTests

#include "../Source/Model/ClipGeometry.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
int failures = 0;
int checks = 0;

void expectNear (const std::string& what, double actual, double expected, double tolerance = 1.0e-9)
{
    ++checks;
    if (std::abs (actual - expected) > tolerance)
    {
        std::printf ("FEHLER  %s: %.6f erwartet, %.6f erhalten\n", what.c_str(), expected, actual);
        ++failures;
    }
}

void expect (const std::string& what, bool condition)
{
    ++checks;
    if (! condition)
    {
        std::printf ("FEHLER  %s\n", what.c_str());
        ++failures;
    }
}

using sis::geometry::ClipState;

ClipState makeClip()
{
    ClipState c;
    c.offset = 0.10;
    c.natural = 0.50;     // 4 s auf der 8-s-Achse
    c.stretch = 1.0;
    c.trimStart = 0.0;
    c.trimEnd = 1.0;
    c.fadeIn = 0.0;
    c.fadeOut = 0.0;
    return c;
}
} // namespace

int main()
{
    // Verschieben ohne Raster
    {
        const auto moved = sis::geometry::moveClip (makeClip(), 0.37, 0.05, false);
        expectNear ("Verschieben folgt dem Zeiger", moved.offset, 0.32);
        expectNear ("Verschieben ändert die Länge nicht", moved.length(), makeClip().length());
    }

    // Verschieben mit Raster rastet auf 1/16 (0,5 s) ein
    {
        const auto moved = sis::geometry::moveClip (makeClip(), 0.37, 0.05, true);
        expectNear ("Raster rastet auf 1/16 ein", moved.offset, 0.3125);
    }

    // Clip bleibt in der Zeitachse
    {
        const auto moved = sis::geometry::moveClip (makeClip(), 1.4, 0.0, false);
        expect ("Clip endet spätestens bei 1.0", moved.end() <= 1.0 + 1.0e-9);
        const auto back = sis::geometry::moveClip (makeClip(), -0.5, 0.0, false);
        expectNear ("Clip beginnt frühestens bei 0", back.offset, 0.0);
    }

    // Rechte Kante: Zuschneiden verkürzt das Sample, Tempo bleibt
    {
        auto clip = makeClip();
        const auto trimmed = sis::geometry::dragRightEdge (clip, 0.35, false, false);
        expectNear ("Zuschneiden lässt den Anfang stehen", trimmed.offset, clip.offset);
        expectNear ("Zuschneiden lässt das Tempo stehen", trimmed.stretch, 1.0);
        expectNear ("Neues Clip-Ende liegt am Zeiger", trimmed.end(), 0.35);
        expectNear ("trimEnd entspricht der neuen Länge", trimmed.trimEnd, 0.5);
    }

    // Rechte Kante mit Shift: Stretchen behält den Ausschnitt
    {
        auto clip = makeClip();
        const auto stretched = sis::geometry::dragRightEdge (clip, 0.35, false, true);
        expectNear ("Stretchen lässt den Ausschnitt stehen", stretched.trimEnd, 1.0);
        expectNear ("Stretch-Faktor passt zur neuen Länge", stretched.stretch, 0.5);
        expectNear ("Clip-Ende liegt am Zeiger", stretched.end(), 0.35);
    }

    // Stretch bleibt in den Grenzen 0,25× … 4×
    {
        const auto tiny = sis::geometry::dragRightEdge (makeClip(), 0.11, false, true);
        expect ("Stretch nicht unter 0,25", tiny.stretch >= sis::geometry::minStretch - 1.0e-9);
        const auto huge = sis::geometry::dragRightEdge (makeClip(), 1.0, false, true);
        expect ("Stretch nicht über 4", huge.stretch <= sis::geometry::maxStretch + 1.0e-9);
    }

    // Linke Kante: das rechte Ende bleibt stehen
    {
        auto clip = makeClip();
        const double end = clip.end();
        const auto trimmed = sis::geometry::dragLeftEdge (clip, 0.25, false, false);
        expectNear ("Rechtes Ende bleibt beim Zuschneiden stehen", trimmed.end(), end);
        expectNear ("Clip beginnt am Zeiger", trimmed.offset, 0.25);
        expectNear ("trimStart wandert mit", trimmed.trimStart, 0.30);

        const auto stretched = sis::geometry::dragLeftEdge (clip, 0.25, false, true);
        expectNear ("Rechtes Ende bleibt auch beim Strecken stehen", stretched.end(), end);
        expectNear ("Ausschnitt bleibt beim Strecken", stretched.trimStart, 0.0);
        expectNear ("Stretch-Faktor passt", stretched.stretch, 0.70);
    }

    // Mindestlänge
    {
        const auto collapsed = sis::geometry::dragRightEdge (makeClip(), 0.10, false, false);
        expect ("Clip behält eine Mindestlänge", collapsed.length() >= sis::geometry::minClipLength - 1.0e-9);
    }

    // Fades: Anteil der Clip-Länge, zusammen höchstens 100 %
    {
        auto clip = makeClip();
        const auto faded = sis::geometry::dragFadeIn (clip, clip.offset + clip.length() * 0.25);
        expectNear ("Fade-in ist ein Viertel der Clip-Länge", faded.fadeIn, 0.25);

        auto both = sis::geometry::dragFadeOut (faded, faded.offset);
        expectNear ("Fade-out füllt höchstens den Rest", both.fadeOut, 0.75);
        expect ("Fades überschneiden sich nicht", both.fadeIn + both.fadeOut <= 1.0 + 1.0e-9);

        const auto negative = sis::geometry::dragFadeIn (clip, clip.offset - 0.2);
        expectNear ("Fade-in wird nicht negativ", negative.fadeIn, 0.0);
    }

    std::printf ("%d Prüfungen, %d Fehler\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
