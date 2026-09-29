#pragma once

#include <JuceHeader.h>
#include <array>

#include "Compressor.h"
#include "Equalizer.h"
#include "Gate.h"

namespace sis::dsp
{
/** Einstellungen des Kanal-Streifens.

    Die Filter-Koeffizienten kommen fertig von außen – wie beim Equalizer entstehen sie
    im Renderplan und nicht im Audio-Thread. */
struct ChannelStripSettings
{
    bool gateActive = false;
    GateSettings gate;

    std::array<Equalizer::Coefficients::Ptr, Equalizer::numBands> equalizer;
    std::array<bool, Equalizer::numBands> bandAudible {};

    bool compressorActive = false;
    CompressorSettings compressor;

    float outputGain = 1.0f;
};

/** Kanal-Streifen: Gate, Klangregelung und Kompressor in einer Karte.

    Hier steckt **keine neue Signalverarbeitung**. Der Streifen setzt die vorhandenen
    Bausteine zusammen; sein Beitrag ist die **Reihenfolge** und dass man sie mit sechs
    statt fünfzehn Reglern bedient.

    Die Reihenfolge ist die eigentliche Entscheidung und folgt dem Mischpult:

    1. **Gate** zuerst – was weg soll, soll weg, bevor irgendetwas es anhebt. Hinter einem
       Kompressor sitzend müsste es gegen dessen Anhebung der leisen Stellen anarbeiten.
    2. **Klangregelung** danach – der Kompressor soll auf das hören, was man behalten will.
       Wer die Tiefen wegnimmt, will nicht, dass sie die Regelung noch steuern.
    3. **Kompressor** zuletzt, weil er das Ergebnis der beiden Schritte festhält.
    4. **Ausgang** als reine Verstärkung hinterher.

    Wer eine andere Reihenfolge braucht, hängt die Einzeleffekte in die Kette – dafür
    sind sie da. Der Streifen ist der schnelle Weg, nicht der einzige. */
class ChannelStrip
{
public:
    void prepare (double sampleRate, int numChannels, int maximumBlockSize);
    void reset();

    void setSettings (const ChannelStripSettings&) noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    ChannelStripSettings settings;

    Gate gate;
    Equalizer equalizer;
    Compressor compressor;
};
} // namespace sis::dsp
