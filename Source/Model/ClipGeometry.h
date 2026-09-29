#pragma once

#include <algorithm>
#include <cmath>

/** Geometrie der Clips im Editor – bewusst ohne JUCE und ohne Oberfläche,
    damit die Regeln aus dem README einzeln geprüft werden können.

    Alle Zeitwerte sind Anteile der 8-Sekunden-Achse, trim und fade Anteile
    des Samples bzw. der Clip-Länge. `limit` ist das Ende der Achse: 1 für die
    8 Sekunden, mehr, wenn der Editor weiter nach rechts reicht. */
namespace sis::geometry
{
inline constexpr double minClipLength = 0.02;
inline constexpr double minStretch = 0.25;
inline constexpr double maxStretch = 4.0;
inline constexpr double gridDivision = 16.0;   // Raster = 1/16 der Achse = 0,5 s

struct ClipState
{
    double offset = 0.0;      // Beginn auf der Zeitachse
    double natural = 0.3;     // Länge des ganzen Samples bei Stretch 1
    double stretch = 1.0;
    double trimStart = 0.0;
    double trimEnd = 1.0;
    double fadeIn = 0.0;
    double fadeOut = 0.0;

    double length() const { return natural * stretch * (trimEnd - trimStart); }
    double end() const { return offset + length(); }
};

inline double snapTime (double value, bool snapToGrid)
{
    return snapToGrid ? std::round (value * gridDivision) / gridDivision : value;
}

/** Körper ziehen: verschiebt den Clip, ohne ihn zu verändern. */
inline ClipState moveClip (ClipState clip, double pointerTime, double grabOffset, bool snapToGrid,
                           double limit = 1.0)
{
    const double length = std::max (minClipLength, clip.length());
    clip.offset = std::clamp (snapTime (pointerTime - grabOffset, snapToGrid), 0.0, std::max (0.0, limit - length));
    return clip;
}

/** Rechte Kante ziehen: schneidet zu oder streckt; der Clip-Anfang bleibt stehen. */
inline ClipState dragRightEdge (ClipState clip, double pointerTime, bool snapToGrid, bool stretching,
                                double limit = 1.0)
{
    const double newLength = std::clamp (snapTime (pointerTime, snapToGrid) - clip.offset,
                                         minClipLength, std::max (minClipLength, limit - clip.offset));

    if (stretching)
    {
        const double base = clip.natural * (clip.trimEnd - clip.trimStart);
        if (base > 0.0)
            clip.stretch = std::clamp (newLength / base, minStretch, maxStretch);
    }
    else
    {
        const double span = newLength / std::max (1.0e-6, clip.natural * clip.stretch);
        clip.trimEnd = std::clamp (clip.trimStart + span, clip.trimStart + minClipLength, 1.0);
    }

    return clip;
}

/** Linke Kante ziehen: das rechte Clip-Ende bleibt stehen, `offset` wandert mit. */
inline ClipState dragLeftEdge (ClipState clip, double pointerTime, bool snapToGrid, bool stretching)
{
    const double end = clip.end();
    const double newStart = std::clamp (snapTime (pointerTime, snapToGrid), 0.0, end - minClipLength);
    const double newLength = end - newStart;

    if (stretching)
    {
        const double base = clip.natural * (clip.trimEnd - clip.trimStart);
        clip.offset = newStart;
        if (base > 0.0)
            clip.stretch = std::clamp (newLength / base, minStretch, maxStretch);
    }
    else
    {
        const double span = newLength / std::max (1.0e-6, clip.natural * clip.stretch);
        clip.trimStart = std::clamp (clip.trimEnd - span, 0.0, clip.trimEnd - minClipLength);
        clip.offset = end - clip.natural * clip.stretch * (clip.trimEnd - clip.trimStart);
    }

    return clip;
}

/** Obere Ecken ziehen: Fade-in (links) bzw. Fade-out (rechts), 0 … 100 % der Clip-Länge. */
inline ClipState dragFadeIn (ClipState clip, double pointerTime)
{
    const double length = std::max (minClipLength, clip.length());
    clip.fadeIn = std::clamp ((pointerTime - clip.offset) / length, 0.0, 1.0 - clip.fadeOut);
    return clip;
}

inline ClipState dragFadeOut (ClipState clip, double pointerTime)
{
    const double length = std::max (minClipLength, clip.length());
    clip.fadeOut = std::clamp ((clip.offset + length - pointerTime) / length, 0.0, 1.0 - clip.fadeIn);
    return clip;
}
} // namespace sis::geometry
