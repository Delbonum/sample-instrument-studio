#pragma once

#include "RenderPlan.h"
#include "SamplerEngine.h"

namespace sis
{
/** Eine Effektkette auf einen Stereo-Block anwenden.

    Das ist der Kern, den sich der Sampler und die einzelnen Effekt-Plugins **teilen**.
    Dass ein „SIS Chorus“ in einer fremden DAW genauso klingt wie der Chorus in der
    Effektkette des Studios, ist damit kein Versprechen, sondern dieselbe Funktion.

    Audio-Thread: rechnet nur, legt nichts an. Was gerechnet werden musste — Koeffizienten
    vor allem — steckt schon im `TrackEffectsPlan`. */
void applyEffectChain (const TrackEffectsPlan&, BusProcessors&,
                       float* const* channels, int numSamples);
} // namespace sis
