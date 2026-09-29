#pragma once

#include <JuceHeader.h>

#include "RenderPlan.h"
#include "SampleCache.h"

namespace sis
{
/** Auftrag für einen Bounce: eine Zone samt Effektketten in einen Stereopuffer rendern.

    Läuft schneller als in Echtzeit auf dem Message-Thread. Fremde Plugins dürfen dabei
    **nicht** die Instanzen des laufenden Instruments sein — die gehören dem Audio-Thread.
    `hostedPlugins` muss also eigene Instanzen liefern. */
struct BounceRequest
{
    const InstrumentModel* model = nullptr;
    const SampleCache* cache = nullptr;
    juce::String zoneId;

    double sampleRate = 48000.0;
    int blockSize = 512;

    /** Nachklang nach dem Loslassen, damit Hall und fremde Plugins ausklingen können. */
    double tailSeconds = 2.0;

    HostedPluginProvider hostedPlugins;
};

struct BounceResult
{
    bool succeeded = false;
    juce::String message;

    juce::AudioBuffer<float> audio;   // zwei Kanäle
    double sampleRate = 48000.0;

    double getLengthSeconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) audio.getNumSamples() / sampleRate : 0.0;
    }
};

/** Rendert die Zone so, wie sie auf ihrem Grundton klingt — mit allen Effekten,
    aber ohne die Hüllkurve des Instruments und ohne den Master-Pegel: beide wirken
    beim Spielen weiterhin und dürfen kein zweites Mal im Sample stecken. */
BounceResult renderZone (const BounceRequest&);

/** Schreibt das Ergebnis als 24-Bit-WAV. */
bool writeBounceToFile (const BounceResult&, const juce::File&, juce::String& error);
} // namespace sis
