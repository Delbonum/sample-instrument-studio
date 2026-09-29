#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sis
{
enum class WaveShape { sustain, attack, air, tail };

/** Platzhalter-Hüllkurve für Beispieldaten ohne echte Audiodatei.
    Entspricht wave() im HTML-Prototyp; Werte 0.04 … 0.96. */
inline std::vector<float> demoWaveform (int seed, int count, WaveShape shape)
{
    std::int64_t state = (std::int64_t) seed * 9301 + 49297;
    auto random = [&state]
    {
        state = (state * 9301 + 49297) % 233280;
        return (double) state / 233280.0;
    };

    constexpr double pi = 3.14159265358979323846;
    std::vector<float> out;
    out.reserve ((size_t) std::max (0, count));

    for (int i = 0; i < count; ++i)
    {
        const double t = i / (double) std::max (1, count - 1);
        double e = 0.0;

        switch (shape)
        {
            case WaveShape::attack:  e = std::pow (1.0 - t, 3.2); break;
            case WaveShape::air:     e = std::sin (std::min (1.0, t * 1.25) * pi) * 0.9; break;
            case WaveShape::tail:    e = std::pow (1.0 - t, 1.1) * (t < 0.04 ? t / 0.04 : 1.0); break;
            case WaveShape::sustain: e = (t < 0.05 ? t / 0.05 : 1.0) * (1.0 - 0.45 * t); break;
        }

        out.push_back ((float) (std::max (4.0, e * (0.42 + 0.58 * random()) * 96.0) / 100.0));
    }

    return out;
}
} // namespace sis
