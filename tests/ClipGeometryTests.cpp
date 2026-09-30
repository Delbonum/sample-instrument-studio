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

    // Raster mit eigener Weite: 10 ms auf der 8-Sekunden-Achse
    {
        const double grid = 0.01 / 8.0;
        expectNear ("Feines Raster rastet auf 10 ms", sis::geometry::snapTime (0.123456, true, grid) * 8.0, 0.99, 1.0e-9);
        expectNear ("Ohne Raster bleibt der Wert", sis::geometry::snapTime (0.123456, false, grid), 0.123456);
        const auto moved = sis::geometry::moveClip (makeClip(), 0.37, 0.05, true, 1.0, grid);
        expectNear ("Verschieben rastet auf das feine Raster", moved.offset * 8.0, 2.56, 1.0e-9);
    }

    // Schere: zwei Stücke, die zusammen den alten Clip ergeben
    {
        auto clip = makeClip();          // 0,10 … 0,60
        clip.trimStart = 0.2;
        clip.trimEnd = 0.8;              // Länge 0,30 → 0,10 … 0,40
        clip.fadeIn = 0.1;
        clip.fadeOut = 0.2;

        ClipState left, right;
        expect ("Der Schnitt gelingt", sis::geometry::splitClip (clip, 0.25, left, right));
        expectNear ("Links endet am Schnitt", left.end(), 0.25);
        expectNear ("Rechts beginnt am Schnitt", right.offset, 0.25);
        expectNear ("Rechts endet wie vorher", right.end(), clip.end());
        expectNear ("Das Sample läuft über den Schnitt weiter", left.trimEnd, right.trimStart);
        expectNear ("Mitten im Zuschnitt geschnitten", left.trimEnd, 0.5);
        expectNear ("Der Fade-in bleibt in Sekunden", left.fadeIn * left.length(), clip.fadeIn * clip.length());
        expectNear ("Der Fade-out bleibt in Sekunden", right.fadeOut * right.length(), clip.fadeOut * clip.length());
        expectNear ("Innen keine Fades", left.fadeOut + right.fadeIn, 0.0);

        expect ("Am Rand wird nicht geschnitten", ! sis::geometry::splitClip (clip, clip.offset + 0.0005, left, right));
        expect ("Daneben auch nicht", ! sis::geometry::splitClip (clip, 0.9, left, right));
    }

    // Crossfade über die ganze Überschneidung
    {
        auto a = makeClip();             // 0,10 … 0,60
        auto b = makeClip();
        b.offset = 0.40;                 // 0,40 … 0,90 – überschneidet 0,40 … 0,60

        expect ("Crossfade gelingt", sis::geometry::crossfade (b, a));   // Reihenfolge egal
        expectNear ("Der frühere blendet über die Überschneidung aus", a.fadeOut * a.length(), 0.20);
        expectNear ("Der spätere blendet über sie ein", b.fadeIn * b.length(), 0.20);

        auto inner = makeClip();
        inner.offset = 0.2;
        inner.natural = 0.1;             // ganz in a
        auto outer = makeClip();
        expect ("Ein Clip ganz im anderen: kein Crossfade", ! sis::geometry::crossfade (outer, inner));

        auto later = makeClip();
        later.offset = 0.8;
        auto early = makeClip();
        expect ("Ohne Überschneidung: kein Crossfade", ! sis::geometry::crossfade (early, later));
    }

    // Überdeckung: der obere Clip verdeckt den unteren – außer in seinen Fades
    {
        using sis::geometry::Span;
        const std::vector<Span> spans { { 0.0, 1.0, 0.0, 0.0 },     // unten
                                        { 0.4, 0.6, 0.0, 0.0 } };   // oben, mittendrin

        const auto lower = sis::geometry::audibleSegments (spans, 0, 1.0);
        expect ("Der untere zerfällt in zwei Stücke", lower.size() == 2);
        if (lower.size() == 2)
        {
            expectNear ("Erstes Stück bis zum oberen", lower[0].second, 0.4);
            expectNear ("Zweites Stück ab seinem Ende", lower[1].first, 0.6);
        }

        const auto upper = sis::geometry::audibleSegments (spans, 1, 0.6);
        expect ("Der obere klingt ganz", upper.size() == 1 && upper[0].first == 0.4 && upper[0].second == 0.6);

        // Mit Fades scheint der untere in ihnen durch
        const std::vector<Span> faded { { 0.0, 1.0, 0.0, 0.0 }, { 0.4, 0.6, 0.05, 0.05 } };
        const auto through = sis::geometry::audibleSegments (faded, 0, 1.0);
        expect ("Im Fade klingt der untere mit", through.size() == 2 && std::abs (through[0].second - 0.45) < 1.0e-9
                                                    && std::abs (through[1].first - 0.55) < 1.0e-9);

        // Schleife: bis zum nächsten Clip der Spur
        expectNear ("Schleife endet am nächsten Clip", sis::geometry::loopUntil (spans, 0), 0.4);
        expect ("Ohne Nachfolger läuft sie weiter", std::isinf (sis::geometry::loopUntil (spans, 1)));
    }

    std::printf ("%d Prüfungen, %d Fehler\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
