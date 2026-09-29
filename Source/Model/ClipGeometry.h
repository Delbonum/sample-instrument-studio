#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

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

//==============================================================================
/** Kürzestes Stück, das ein Schnitt übrig lassen darf (1/500 der Achse = 16 ms). */
inline constexpr double minSplitPiece = 0.002;

/** Schere: teilt den Clip bei `time` in zwei. Das Sample läuft über die Schnittstelle
    ungestört weiter – links endet der Zuschnitt dort, wo er rechts beginnt. Die Fades
    bleiben in Sekunden erhalten, der Fade-in beim linken, der Fade-out beim rechten Stück.
    Gibt false zurück, wenn `time` nicht deutlich im Clip liegt. */
inline bool splitClip (const ClipState& clip, double time, ClipState& left, ClipState& right)
{
    const double length = clip.length();

    if (length <= 0.0 || time - clip.offset < minSplitPiece || clip.end() - time < minSplitPiece)
        return false;

    const double share = (time - clip.offset) / length;
    const double cut = clip.trimStart + share * (clip.trimEnd - clip.trimStart);
    const double fadeInLength = clip.fadeIn * length;
    const double fadeOutLength = clip.fadeOut * length;

    left = clip;
    right = clip;

    left.trimEnd = cut;
    left.fadeIn = std::min (1.0, fadeInLength / left.length());
    left.fadeOut = 0.0;

    right.trimStart = cut;
    right.offset = time;
    right.fadeIn = 0.0;
    right.fadeOut = std::min (1.0, fadeOutLength / right.length());
    return true;
}

/** Taste X: Crossfade über die ganze Überschneidung. Der früher beginnende Clip blendet
    aus, der spätere ein – gleich welcher oben liegt, denn der obere lässt in seinem Fade
    den unteren durchscheinen (siehe `audibleSegments`). Liegt ein Clip ganz im anderen
    oder überschneiden sie sich gar nicht, gibt es nichts zu überblenden: false. */
inline bool crossfade (ClipState& a, ClipState& b)
{
    auto& first = a.offset <= b.offset ? a : b;
    auto& second = &first == &a ? b : a;

    const double overlap = first.end() - second.offset;

    if (overlap <= 0.0 || second.end() <= first.end())
        return false;

    first.fadeOut = std::min (1.0, overlap / first.length());
    first.fadeIn = std::min (first.fadeIn, 1.0 - first.fadeOut);
    second.fadeIn = std::min (1.0, overlap / second.length());
    second.fadeOut = std::min (second.fadeOut, 1.0 - second.fadeIn);
    return true;
}

/** Ein Clip auf der Achse, so wie ihn die Überdeckung braucht: Anfang, Ende und die
    Längen seiner Fades (nicht Anteile). */
struct Span
{
    double start = 0.0, end = 0.0;
    double fadeIn = 0.0, fadeOut = 0.0;
};

using Segments = std::vector<std::pair<double, double>>;

/** Wo Clip `index` zu hören ist: von seinem Anfang bis `until`, abzüglich dessen, was die
    Clips darüber (weiter hinten in `spans`) verdecken. Ein Clip verdeckt nur zwischen
    seinen Fades – in einem Fade scheint der untere durch, und genau das ist ein Crossfade.
    `until` ist bei einem One-Shot das Clip-Ende, bei einer Schleife der nächste Clip. */
inline Segments audibleSegments (const std::vector<Span>& spans, std::size_t index, double until)
{
    Segments result;

    if (index >= spans.size() || until <= spans[index].start)
        return result;

    result.push_back ({ spans[index].start, until });

    for (std::size_t upper = index + 1; upper < spans.size(); ++upper)
    {
        const double hiddenFrom = spans[upper].start + spans[upper].fadeIn;
        const double hiddenTo = spans[upper].end - spans[upper].fadeOut;

        if (hiddenTo <= hiddenFrom)
            continue;

        Segments remaining;

        for (const auto& [from, to] : result)
        {
            if (hiddenTo <= from || hiddenFrom >= to)
            {
                remaining.push_back ({ from, to });
                continue;
            }

            if (hiddenFrom > from)
                remaining.push_back ({ from, hiddenFrom });

            if (hiddenTo < to)
                remaining.push_back ({ hiddenTo, to });
        }

        result = std::move (remaining);
    }

    result.erase (std::remove_if (result.begin(), result.end(),
                                  [] (const std::pair<double, double>& s) { return s.second - s.first < 1.0e-9; }),
                  result.end());
    return result;
}

/** Bis wohin eine Schleife klingt: bis zum Anfang des nächsten Clips der Spur, sonst
    unbegrenzt. */
inline double loopUntil (const std::vector<Span>& spans, std::size_t index)
{
    double next = std::numeric_limits<double>::infinity();

    for (std::size_t other = 0; other < spans.size(); ++other)
        if (other != index && spans[other].end > spans[other].start && spans[other].start > spans[index].start)
            next = std::min (next, spans[other].start);

    return next;
}
} // namespace sis::geometry
