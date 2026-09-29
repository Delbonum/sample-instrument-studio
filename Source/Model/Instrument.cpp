#include "Instrument.h"

#include <algorithm>
#include <set>

namespace sis
{
namespace
{
    namespace id
    {
        // Knoten
        const juce::Identifier instrument { "Instrument" };
        const juce::Identifier zone       { "Zone" };
        const juce::Identifier track      { "Track" };
        const juce::Identifier effect     { "Effect" };
        const juce::Identifier parameter  { "Parameter" };
        const juce::Identifier sample     { "Sample" };
        const juce::Identifier envelope   { "Envelope" };
        const juce::Identifier macros     { "Macros" };
        const juce::Identifier targets    { "Targets" };

        // Eigenschaften
        const juce::Identifier name           { "name" };
        const juce::Identifier uid            { "id" };
        const juce::Identifier selectedZone   { "selectedZone" };
        const juce::Identifier selectedSample { "selectedSample" };
        const juce::Identifier selectedTrack  { "selectedTrack" };
        const juce::Identifier lowNote        { "lowNote" };
        const juce::Identifier highNote       { "highNote" };
        const juce::Identifier lowVelocity    { "lowVelocity" };
        const juce::Identifier highVelocity   { "highVelocity" };
        const juce::Identifier rootNote       { "rootNote" };
        const juce::Identifier kind           { "kind" };
        const juce::Identifier drumPart       { "drumPart" };
        const juce::Identifier colour         { "colour" };
        const juce::Identifier softColour     { "softColour" };
        const juce::Identifier clip           { "clip" };
        const juce::Identifier shape          { "shape" };
        const juce::Identifier offset         { "offset" };
        const juce::Identifier natural        { "natural" };
        const juce::Identifier stretch        { "stretch" };
        const juce::Identifier trimStart      { "trimStart" };
        const juce::Identifier trimEnd        { "trimEnd" };
        const juce::Identifier fadeIn         { "fadeIn" };
        const juce::Identifier fadeOut        { "fadeOut" };
        const juce::Identifier gain           { "gain" };
        const juce::Identifier pan            { "pan" };
        const juce::Identifier pitch          { "pitch" };
        const juce::Identifier cents          { "cents" };
        const juce::Identifier reverse        { "reverse" };
        const juce::Identifier loop           { "loop" };
        const juce::Identifier loopStart      { "loopStart" };
        const juce::Identifier loopCrossfade  { "loopCrossfade" };
        const juce::Identifier kitPieces      { "kitPieces" };
        const juce::Identifier algorithm      { "algorithm" };
        const juce::Identifier mute           { "mute" };
        const juce::Identifier solo           { "solo" };
        const juce::Identifier enabled        { "enabled" };
        const juce::Identifier external       { "external" };
        const juce::Identifier label          { "label" };
        const juce::Identifier value          { "value" };
        const juce::Identifier path           { "path" };
        const juce::Identifier length         { "length" };
        const juce::Identifier bitDepth       { "bitDepth" };
        const juce::Identifier attack         { "attack" };
        const juce::Identifier decay          { "decay" };
        const juce::Identifier sustain        { "sustain" };
        const juce::Identifier release        { "release" };
        const juce::Identifier vst3           { "vst3" };
        const juce::Identifier audioUnit      { "audioUnit" };
        const juce::Identifier standalone     { "standalone" };
        const juce::Identifier project        { "project" };
    }

    template <typename Enum>
    Enum readEnum (const juce::ValueTree& tree, const juce::Identifier& key, Enum fallback, int numValues)
    {
        const int v = (int) tree.getProperty (key, (int) fallback);
        return v >= 0 && v < numValues ? (Enum) v : fallback;
    }

    juce::Colour readColour (const juce::ValueTree& tree, const juce::Identifier& key, juce::Colour fallback)
    {
        const auto text = tree.getProperty (key).toString();
        return text.isEmpty() ? fallback : juce::Colour::fromString (text);
    }

    std::vector<float> computePeaks (juce::AudioFormatReader& reader)
    {
        const int bins = SampleFile::peakResolution;
        std::vector<float> peaks ((size_t) bins, 0.0f);

        const auto total = reader.lengthInSamples;
        const int channels = juce::jmax (1, (int) reader.numChannels);
        std::vector<juce::Range<float>> levels ((size_t) channels);
        float maxPeak = 0.0f;

        for (int i = 0; i < bins; ++i)
        {
            const juce::int64 start = total * i / bins;
            const juce::int64 end = total * (i + 1) / bins;

            if (end <= start)
                continue;

            reader.readMaxLevels (start, end - start, levels.data(), channels);

            float peak = 0.0f;
            for (const auto& l : levels)
                peak = juce::jmax (peak, std::abs (l.getStart()), std::abs (l.getEnd()));

            peaks[(size_t) i] = peak;
            maxPeak = juce::jmax (maxPeak, peak);
        }

        if (maxPeak > 0.0f)
            for (auto& p : peaks)
                p /= maxPeak;

        return peaks;
    }

    Effect makeEffect (const juce::String& name, bool external, std::vector<EffectParameter> params)
    {
        Effect e;
        e.name = name;
        e.external = external;
        e.parameters = std::move (params);
        return e;
    }

    Track makeTrack (const juce::String& name, const juce::String& clip, int paletteIndex,
                     WaveShape shape, double naturalSeconds)
    {
        Track t;
        const auto& c = trackPalette()[(size_t) paletteIndex];
        t.name = name;
        t.clip = clip;
        t.colour = c.main;
        t.softColour = c.soft;
        t.shape = shape;
        t.natural = naturalSeconds / InstrumentModel::timelineSeconds;
        t.fadeIn = 0.04;
        t.fadeOut = 0.12;
        t.gain = 0.6f;
        return t;
    }

    SampleFile demoSample (const juce::String& name, double seconds, int bits, WaveShape shape, int seed)
    {
        SampleFile s;
        s.name = name;
        s.lengthSeconds = seconds;
        s.bitDepth = bits;
        s.shape = shape;
        s.peaks = demoWaveform (seed, SampleFile::peakResolution, shape);
        return s;
    }
} // namespace

//==============================================================================
juce::String toDisplayString (LoopMode mode)
{
    switch (mode)
    {
        case LoopMode::oneShot:     return "One-Shot";
        case LoopMode::sustainLoop: return "Sustain-Loop";
        case LoopMode::pingPong:    return "Vor/Rückwärts"_u;
    }
    return {};
}

juce::String toDisplayString (StretchAlgorithm algorithm)
{
    switch (algorithm)
    {
        case StretchAlgorithm::transientPreserving: return "Transienten-treu";
        case StretchAlgorithm::smooth:              return "Glatt (Pad)";
        case StretchAlgorithm::monophonic:          return "Monophon";
        case StretchAlgorithm::granular:            return "Korn / Granular";
    }
    return {};
}

const std::array<TrackColour, 5>& trackPalette()
{
    static const std::array<TrackColour, 5> palette {{
        { juce::Colour (0xff7a6cf0), juce::Colour (0xffeeebfe) },
        { juce::Colour (0xff2e9e96), juce::Colour (0xffe2f2f0) },
        { juce::Colour (0xffc0862e), juce::Colour (0xfffaf1de) },
        { juce::Colour (0xffb4553f), juce::Colour (0xfff6e4df) },
        { juce::Colour (0xff6e6b66), juce::Colour (0xffefeeea) },
    }};
    return palette;
}

Effect Effect::makeEqualizer()
{
    Effect effect;
    effect.name = "EQ 4-Band";
    effect.type = EffectType::equalizer;
    effect.parameters = { { "TIEFEN", 0.5f }, { "TIEFMITTEN", 0.5f },
                          { "HOCHMITTEN", 0.5f }, { "HÖHEN"_u, 0.5f } };
    return effect;
}

Effect Effect::makeCompressor()
{
    Effect effect;
    effect.name = "Kompressor";
    effect.type = EffectType::compressor;
    effect.parameters = { { "SCHWELLE", 0.55f }, { "VERHÄLTNIS"_u, 0.35f },
                          { "ATTACK", 0.25f }, { "RELEASE", 0.35f }, { "AUSGLEICH", 0.2f } };
    return effect;
}

Effect Effect::makeReverb()
{
    Effect effect;
    effect.name = "Hall (Plate)";
    effect.type = EffectType::reverb;
    effect.parameters = { { "GRÖSSE"_u, 0.55f }, { "DÄMPFUNG"_u, 0.45f },
                          { "BREITE", 0.8f }, { "ANTEIL", 0.25f } };
    return effect;
}

Effect Effect::makeExternal (const juce::String& name, const juce::String& identifier)
{
    Effect effect;
    effect.name = name;
    effect.type = EffectType::external;
    effect.external = true;
    effect.pluginIdentifier = identifier;
    return effect;
}

Effect Effect::makeChannelFilter()
{
    Effect effect;
    effect.name = "Kanalfilter";
    effect.type = EffectType::channelFilter;
    effect.parameters = { { "HOCHPASS", 0.0f }, { "TIEFPASS", 1.0f }, { "GÜTE"_u, 0.2f } };
    return effect;
}

Effect Effect::makeSaturation()
{
    Effect effect;
    effect.name = "Sättigung"_u;
    effect.type = EffectType::saturation;
    effect.parameters = { { "SÄTTIGUNG"_u, 0.35f }, { "MENGE", 1.0f }, { "AUSGANG", 0.5f } };
    return effect;
}

Effect Effect::makeTransients()
{
    Effect effect;
    effect.name = "Transienten";
    effect.type = EffectType::transients;
    effect.parameters = { { "ANSCHLAG", 0.5f }, { "AUSKLANG", 0.5f } };
    return effect;
}

Effect Effect::makeChorus()
{
    Effect effect;
    effect.name = "Chorus";
    effect.type = EffectType::chorus;
    effect.parameters = { { "TIEFE", 0.45f }, { "TEMPO", 0.25f },
                          { "BREITE", 0.8f }, { "MENGE", 0.4f } };
    return effect;
}

Effect Effect::makeBitCrusher()
{
    Effect effect;
    effect.name = "Bit-Crusher";
    effect.type = EffectType::bitCrusher;
    effect.parameters = { { "BITS", 0.55f }, { "RATE", 0.0f }, { "MENGE", 0.6f } };
    return effect;
}

Effect Effect::makeOverdrive()
{
    Effect effect;
    effect.name = "Overdrive";
    effect.type = EffectType::overdrive;
    effect.parameters = { { "STÄRKE"_u, 0.4f }, { "CHARAKTER", 0.5f }, { "KLANG", 0.7f },
                          { "MENGE", 1.0f }, { "AUSGANG", 0.5f } };
    return effect;
}

Effect Effect::makeDistortion()
{
    Effect effect;
    effect.name = "Distortion";
    effect.type = EffectType::distortion;
    effect.parameters = { { "STÄRKE"_u, 0.5f }, { "KANTE", 0.6f }, { "KLANG", 0.5f },
                          { "MENGE", 1.0f }, { "AUSGANG", 0.4f } };
    return effect;
}

Effect Effect::makeFlanger()
{
    Effect effect;
    effect.name = "Flanger";
    effect.type = EffectType::flanger;
    effect.parameters = { { "TIEFE", 0.6f }, { "TEMPO", 0.18f },
                          { "RÜCKKOPPLUNG"_u, 0.5f }, { "MENGE", 0.5f } };
    return effect;
}

Effect Effect::makeVibrato()
{
    Effect effect;
    effect.name = "Vibrato";
    effect.type = EffectType::vibrato;

    // Kein Mischregler: mit Trockenanteil wäre es ein Chorus
    effect.parameters = { { "TIEFE", 0.3f }, { "TEMPO", 0.45f } };
    return effect;
}

Effect Effect::makePhaser()
{
    Effect effect;
    effect.name = "Phaser";
    effect.type = EffectType::phaser;
    effect.parameters = { { "TIEFE", 0.75f }, { "TEMPO", 0.2f },
                          { "RÜCKKOPPLUNG"_u, 0.45f }, { "MENGE", 0.5f } };
    return effect;
}

Effect Effect::makeTremolo()
{
    Effect effect;
    effect.name = "Tremolo";
    effect.type = EffectType::tremolo;
    effect.parameters = { { "TIEFE", 0.6f }, { "TEMPO", 0.35f },
                          { "FORM", 0.0f }, { "BREITE", 0.0f } };
    return effect;
}

Effect Effect::makeDelay()
{
    Effect effect;
    effect.name = "Delay";
    effect.type = EffectType::delay;
    effect.parameters = { { "ZEIT", 0.35f }, { "RÜCKKOPPLUNG"_u, 0.4f },
                          { "DÄMPFUNG"_u, 0.4f }, { "PING-PONG", 0.0f }, { "MENGE", 0.3f } };
    return effect;
}

Effect Effect::makeNoiseGate()
{
    Effect effect;
    effect.name = "Noise Gate";
    effect.type = EffectType::noiseGate;
    effect.parameters = { { "SCHWELLE", 0.3f }, { "ATTACK", 0.1f }, { "HALTEN", 0.3f },
                          { "RELEASE", 0.3f }, { "TIEFE", 0.8f } };
    return effect;
}

Effect Effect::makeExpander()
{
    Effect effect;
    effect.name = "Expander";
    effect.type = EffectType::expander;
    effect.parameters = { { "SCHWELLE", 0.4f }, { "VERHÄLTNIS"_u, 0.3f },
                          { "ATTACK", 0.2f }, { "RELEASE", 0.35f } };
    return effect;
}

Effect Effect::makeLimiter()
{
    Effect effect;
    effect.name = "Limiter";
    effect.type = EffectType::limiter;
    effect.parameters = { { "STÄRKE"_u, 0.0f }, { "DECKE", 0.95f }, { "RELEASE", 0.25f } };
    return effect;
}

Effect Effect::makeDeEsser()
{
    Effect effect;
    effect.name = "De-Esser";
    effect.type = EffectType::deEsser;
    effect.parameters = { { "FREQUENZ", 0.5f }, { "SCHWELLE", 0.45f },
                          { "STÄRKE"_u, 0.5f }, { "RELEASE", 0.3f } };
    return effect;
}

Effect Effect::makeEnvelopeFilter()
{
    Effect effect;
    effect.name = "Envelope Filter";
    effect.type = EffectType::envelopeFilter;
    effect.parameters = { { "GRUNDTON", 0.25f }, { "ANSCHLAG", 0.6f },
                          { "RESONANZ", 0.5f }, { "ATTACK", 0.2f },
                          { "RELEASE", 0.35f }, { "MENGE", 0.8f } };
    return effect;
}

Effect Effect::makeAutoWah()
{
    Effect effect;
    effect.name = "Auto-Wah";
    effect.type = EffectType::autoWah;

    // VON und BIS spannen das Intervall auf, in dem das Filter gleichmäßig läuft
    effect.parameters = { { "VON", 0.2f }, { "BIS", 0.7f }, { "TEMPO", 0.25f },
                          { "RESONANZ", 0.55f }, { "MENGE", 0.8f } };
    return effect;
}

Effect Effect::makeChannelStrip()
{
    Effect effect;
    effect.name = "Kanal-Streifen";
    effect.type = EffectType::channelStrip;

    // Sechs Regler statt fünfzehn - das ist der Punkt des Streifens
    effect.parameters = { { "GATE", 0.0f }, { "TIEFEN", 0.5f }, { "MITTEN", 0.5f },
                          { "HÖHEN"_u, 0.5f }, { "KOMPRESSION", 0.0f }, { "AUSGANG", 0.5f } };
    return effect;
}

std::vector<Effect> Effect::builtIn()
{
    // Reihenfolge des Menüs: erst formen, dann verzerren, dann modulieren,
    // dann Raum, dann Dynamik
    // Der Streifen steht vorn: er ist für die meisten Spuren der schnellste Weg
    return { makeChannelStrip(),
             makeChannelFilter(), makeSaturation(), makeOverdrive(), makeDistortion(),
             makeTransients(), makeEqualizer(),
             makeEnvelopeFilter(), makeAutoWah(),
             makeChorus(), makeFlanger(), makePhaser(), makeVibrato(), makeTremolo(),
             makeDelay(), makeReverb(),
             makeNoiseGate(), makeExpander(), makeCompressor(), makeLimiter(), makeDeEsser(),
             makeBitCrusher() };
}

juce::String macroName (int index)
{
    static const char* const names[] = { "Filter-Farbe", "Attack-Anteil",
                                         "Luft / Obertöne", "Hall-Anteil" };

    return juce::isPositiveAndBelow (index, (int) juce::numElementsInArray (names))
               ? juce::String::fromUTF8 (names[index])
               : juce::String();
}

std::optional<EffectFamily> familyOf (EffectType type) noexcept
{
    /* Bewusst ohne `default`: kommt eine Art hinzu und wird hier vergessen, warnt der
       Übersetzer, statt dass sie still aus dem Menü verschwindet. */
    switch (type)
    {
        case EffectType::channelFilter:
        case EffectType::equalizer:
        case EffectType::envelopeFilter:
        case EffectType::autoWah:
            return EffectFamily::filter;

        case EffectType::saturation:
        case EffectType::overdrive:
        case EffectType::distortion:
        case EffectType::bitCrusher:
            return EffectFamily::drive;

        case EffectType::chorus:
        case EffectType::flanger:
        case EffectType::phaser:
        case EffectType::vibrato:
        case EffectType::tremolo:
            return EffectFamily::modulation;

        case EffectType::delay:
        case EffectType::reverb:
            return EffectFamily::time;

        case EffectType::channelStrip:
        case EffectType::transients:
        case EffectType::noiseGate:
        case EffectType::expander:
        case EffectType::compressor:
        case EffectType::limiter:
        case EffectType::deEsser:
            return EffectFamily::dynamics;

        case EffectType::generic:
        case EffectType::external:
            break;
    }

    return std::nullopt;
}

juce::String toDisplayString (EffectFamily family)
{
    switch (family)
    {
        case EffectFamily::filter:     return "Filter & Klangregelung"_u;
        case EffectFamily::drive:      return "Verzerrung"_u;
        case EffectFamily::modulation: return "Modulation"_u;
        case EffectFamily::time:       return "Zeit & Raum"_u;
        case EffectFamily::dynamics:   return "Dynamik"_u;
    }

    return {};
}

const std::vector<EffectFamily>& effectFamilyOrder()
{
    static const std::vector<EffectFamily> order { EffectFamily::filter, EffectFamily::drive,
                                                   EffectFamily::modulation, EffectFamily::time,
                                                   EffectFamily::dynamics };
    return order;
}

juce::String Effect::kindLabel() const
{
    return external ? "VST3 · extern"_u : juce::String ("Intern");
}

//==============================================================================
int Zone::numSamples() const
{
    std::set<juce::String> clips;
    for (const auto& t : tracks)
        if (t.clip.isNotEmpty() && t.clip != "leer")
            clips.insert (t.clip);
    return (int) clips.size();
}

double Zone::contentEnd() const
{
    double end = 0.0;

    for (const auto& t : tracks)
        if (t.hasClip())
            end = juce::jmax (end, t.offset + t.clipLength());

    return end;
}

void Track::placeSample (const SampleFile& sample, double newOffset)
{
    /* Der Name folgt dem Sample, solange ihn niemand selbst vergeben hat – eine Spur,
       die „Spur 2“ oder wie ihr altes Sample heißt, soll nicht falsch beschriftet bleiben. */
    const auto baseName = [] (const juce::String& fileName)
    {
        return fileName.containsChar ('.') ? fileName.upToLastOccurrenceOf (".", false, false) : fileName;
    };

    const bool generatedName = name.isEmpty() || name == "Spur"
                            || (name.startsWith ("Spur ") && name.substring (5).containsOnly ("0123456789"))
                            || (hasClip() && name == baseName (clip));

    if (generatedName)
        name = baseName (sample.name);

    clip = sample.name;
    shape = sample.shape;
    natural = juce::jmax (0.01, sample.lengthSeconds / InstrumentModel::timelineSeconds);
    offset = juce::jmax (0.0, newOffset);
    stretch = 1.0;
    trimStart = 0.0;
    trimEnd = 1.0;
    fadeIn = 0.0;
    fadeOut = 0.0;
}

Track* Zone::getSelectedTrack()
{
    return juce::isPositiveAndBelow (selectedTrack, (int) tracks.size()) ? &tracks[(size_t) selectedTrack] : nullptr;
}

const Track* Zone::getSelectedTrack() const
{
    return const_cast<Zone*> (this)->getSelectedTrack();
}

//==============================================================================
juce::String SampleFile::metaText() const
{
    return juce::String (lengthSeconds, 2) + " s · "_u + juce::String (bitDepth) + " Bit";
}

std::optional<SampleFile> SampleFile::fromAudioFile (const juce::File& f, juce::AudioFormatManager& formats)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));

    if (reader == nullptr || reader->sampleRate <= 0.0)
        return std::nullopt;

    SampleFile s;
    s.name = f.getFileName();
    s.file = f;
    s.lengthSeconds = (double) reader->lengthInSamples / reader->sampleRate;
    s.bitDepth = (int) reader->bitsPerSample;
    s.shape = WaveShape::sustain;
    s.peaks = computePeaks (*reader);
    return s;
}

//==============================================================================
InstrumentModel::InstrumentModel()
{
    loadDemo();
}

void InstrumentModel::applyMacro (int macro)
{
    if (! juce::isPositiveAndBelow (macro, numMacros))
        return;

    const float value = juce::jlimit (0.0f, 1.0f, macros[(size_t) macro]);

    for (const auto& target : macroTargets[(size_t) macro])
    {
        auto* zone = findZone (target.zoneId);

        if (zone == nullptr || ! juce::isPositiveAndBelow (target.trackIndex, (int) zone->tracks.size()))
            continue;

        auto& effects = zone->tracks[(size_t) target.trackIndex].effects;

        if (! juce::isPositiveAndBelow (target.effectIndex, (int) effects.size()))
            continue;

        auto& parameters = effects[(size_t) target.effectIndex].parameters;

        if (juce::isPositiveAndBelow (target.parameterIndex, (int) parameters.size()))
            parameters[(size_t) target.parameterIndex].value = value;
    }
}

int InstrumentModel::macroFor (const MacroTarget& target) const
{
    for (int macro = 0; macro < numMacros; ++macro)
        for (const auto& assigned : macroTargets[(size_t) macro])
            if (assigned == target)
                return macro;

    return -1;
}

void InstrumentModel::assignMacro (int macro, const MacroTarget& target)
{
    // Erst überall lösen: ein Regler gehört zu höchstens einem Makro
    for (auto& targets : macroTargets)
        targets.erase (std::remove (targets.begin(), targets.end(), target), targets.end());

    if (juce::isPositiveAndBelow (macro, numMacros))
    {
        macroTargets[(size_t) macro].push_back (target);
        applyMacro (macro);
    }
}

Zone* InstrumentModel::getSelectedZone()
{
    return findZone (selectedZoneId);
}

const Zone* InstrumentModel::getSelectedZone() const
{
    return const_cast<InstrumentModel*> (this)->getSelectedZone();
}

Zone* InstrumentModel::findZone (const juce::String& zoneId)
{
    for (auto& z : zones)
        if (z.id == zoneId)
            return &z;
    return nullptr;
}

const SampleFile* InstrumentModel::findSample (const juce::String& fileName) const
{
    for (const auto& s : samples)
        if (s.name == fileName)
            return &s;
    return nullptr;
}

std::vector<float> InstrumentModel::waveformFor (const Track& track, int trackIndex) const
{
    if (const auto* sample = findSample (track.clip))
        if (! sample->peaks.empty())
            return sample->peaks;

    return demoWaveform (trackIndex + 11, SampleFile::peakResolution, track.shape);
}

const Zone* InstrumentModel::findZoneForNote (int note, int velocity) const
{
    for (const auto& z : zones)
        if (z.containsNote (note) && (velocity < 0 || z.containsVelocity (velocity)))
            return &z;
    return nullptr;
}

void InstrumentModel::selectZone (const juce::String& zoneId)
{
    if (selectedZoneId != zoneId && findZone (zoneId) != nullptr)
    {
        selectedZoneId = zoneId;
        notifyChanged();
    }
}

Zone& InstrumentModel::addZone()
{
    Zone z;
    z.id = juce::Uuid().toString();
    z.name = "Neue Zone";
    z.lowNote = 60;
    z.highNote = 71;
    z.rootNote = 64;
    z.colour = trackPalette()[4].main;
    zones.push_back (z);
    selectedZoneId = z.id;
    notifyChanged();
    return zones.back();
}

Zone* InstrumentModel::findZoneForDrumPart (const juce::String& partId)
{
    if (partId.isEmpty())
        return nullptr;

    for (auto& z : zones)
        if (z.drumPart == partId)
            return &z;

    return nullptr;
}

const Zone* InstrumentModel::findZoneForDrumPart (const juce::String& partId) const
{
    return const_cast<InstrumentModel*> (this)->findZoneForDrumPart (partId);
}

Zone& InstrumentModel::addDrumZone (const DrumPart& part)
{
    if (const auto* piece = findPieceForPart (part.id))
        if (! hasKitPiece (piece->id))
            kitPieces.add (piece->id);

    if (auto* existing = findZoneForDrumPart (part.id))
    {
        selectZone (existing->id);
        return *existing;
    }

    /* Ein Kit-Teil sitzt auf **einer** Taste, und diese Taste ist zugleich sein Grundton.
       Beides auseinanderzuziehen hätte keinen Sinn: gespielt wird das Sample ohnehin in
       seiner eigenen Tonhöhe. */
    Zone z;
    z.id = juce::Uuid().toString();
    z.name = juce::String::fromUTF8 (part.name);
    z.drumPart = part.id;
    z.lowNote = z.highNote = z.rootNote = part.note;
    z.colour = trackPalette()[zones.size() % trackPalette().size()].main;

    zones.push_back (z);
    selectedZoneId = z.id;
    notifyChanged();
    return zones.back();
}

void InstrumentModel::addKitPiece (const DrumPiece& piece)
{
    if (hasKitPiece (piece.id))
        return;

    kitPieces.add (piece.id);
    notifyChanged();
}

void InstrumentModel::removeKitPiece (const DrumPiece& piece)
{
    kitPieces.removeString (piece.id);

    for (const auto* articulation : piece.articulations)
        while (const auto* zone = findZoneForDrumPart (articulation))
            removeZone (zone->id);

    notifyChanged();
}

int InstrumentModel::numKitToms() const
{
    int toms = 0;

    for (const auto& pieceId : kitPieces)
        if (const auto* piece = findDrumPiece (pieceId))
            if (isTom (piece->kind))
                ++toms;

    return toms;
}

void InstrumentModel::removeZone (const juce::String& zoneToRemove)
{
    const auto zoneId = zoneToRemove;   // Kopie: der Verweis darf die Kennung der Zone selbst sein, die gleich verschwindet
    const auto found = std::find_if (zones.begin(), zones.end(),
                                     [&zoneId] (const Zone& z) { return z.id == zoneId; });

    if (found == zones.end())
        return;

    zones.erase (found);

    for (auto& assigned : macroTargets)
        assigned.erase (std::remove_if (assigned.begin(), assigned.end(),
                                        [&zoneId] (const MacroTarget& t) { return t.zoneId == zoneId; }),
                        assigned.end());

    if (selectedZoneId == zoneId)
        selectedZoneId = zones.empty() ? juce::String() : zones.front().id;

    notifyChanged();
}

void InstrumentModel::removeTrack (Zone& zone, int trackIndex)
{
    if (! juce::isPositiveAndBelow (trackIndex, (int) zone.tracks.size()))
        return;

    zone.tracks.erase (zone.tracks.begin() + trackIndex);

    for (auto& assigned : macroTargets)
    {
        assigned.erase (std::remove_if (assigned.begin(), assigned.end(),
                                        [&] (const MacroTarget& t)
                                        {
                                            return t.zoneId == zone.id && t.trackIndex == trackIndex;
                                        }),
                        assigned.end());

        for (auto& t : assigned)
            if (t.zoneId == zone.id && t.trackIndex > trackIndex)
                --t.trackIndex;
    }

    if (zone.selectedTrack >= trackIndex)
        zone.selectedTrack = juce::jmax (0, zone.selectedTrack - 1);

    notifyChanged();
}

int InstrumentModel::addSampleTrack (Zone& zone, const SampleFile& sample)
{
    auto target = std::find_if (zone.tracks.begin(), zone.tracks.end(),
                                [] (const Track& t) { return ! t.hasClip(); });

    if (target == zone.tracks.end())
    {
        const auto& palette = trackPalette()[zone.tracks.size() % trackPalette().size()];

        Track track;
        track.name.clear();
        track.colour = palette.main;
        track.softColour = palette.soft;
        track.gain = 0.8f;
        zone.tracks.push_back (std::move (track));
        target = zone.tracks.end() - 1;
    }

    target->placeSample (sample, 0.0);
    zone.selectedTrack = (int) (target - zone.tracks.begin());
    notifyChanged();
    return zone.selectedTrack;
}

Zone& InstrumentModel::addZoneAt (int note, int velocity)
{
    note = juce::jlimit (lowestNote, highestNote, note);
    velocity = juce::jlimit (0, 127, velocity);

    // Velocity: der freie Bereich um den Anschlag, den an dieser Taste keine Zone belegt
    int lowVelocity = 0, highVelocity = 127;

    for (const auto& z : zones)
    {
        if (! z.containsNote (note))
            continue;

        if (z.highVelocity < velocity)
            lowVelocity = juce::jmax (lowVelocity, z.highVelocity + 1);
        else if (z.lowVelocity > velocity)
            highVelocity = juce::jmin (highVelocity, z.lowVelocity - 1);
        else
            lowVelocity = highVelocity = velocity;   // schon belegt – dann nur diese eine Stufe
    }

    const auto isFree = [&] (int low, int high)
    {
        for (const auto& z : zones)
            if (z.lowNote <= high && z.highNote >= low
                && z.lowVelocity <= highVelocity && z.highVelocity >= lowVelocity)
                return false;

        return true;
    };

    // Tasten: abwechselnd nach oben und unten wachsen, solange Platz ist
    int low = note, high = note;
    bool grew = true;

    while (grew && high - low + 1 < 12)
    {
        grew = false;

        if (high < highestNote && isFree (low, high + 1))
        {
            ++high;
            grew = true;
        }

        if (high - low + 1 < 12 && low > lowestNote && isFree (low - 1, high))
        {
            --low;
            grew = true;
        }
    }

    Zone z;
    z.id = juce::Uuid().toString();
    z.name = "Neue Zone";
    z.lowNote = low;
    z.highNote = high;
    z.rootNote = note;
    z.lowVelocity = lowVelocity;
    z.highVelocity = juce::jmax (lowVelocity, highVelocity);
    z.colour = trackPalette()[zones.size() % trackPalette().size()].main;
    zones.push_back (z);
    selectedZoneId = z.id;
    notifyChanged();
    return zones.back();
}

void InstrumentModel::setKind (InstrumentKind newKind)
{
    if (kind == newKind)
        return;

    kind = newKind;
    notifyChanged();
}

juce::String toDisplayString (InstrumentKind kind)
{
    return kind == InstrumentKind::drumKit ? "Drumset" : "Tonhöhen-Instrument"_u;
}

bool InstrumentModel::isSupportedAudioFile (const juce::String& path)
{
    return juce::File (path).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

int InstrumentModel::importSamples (const juce::Array<juce::File>& files, juce::AudioFormatManager& formats)
{
    int added = 0;

    for (const auto& f : files)
    {
        if (! f.existsAsFile() || ! isSupportedAudioFile (f.getFullPathName()))
            continue;

        const bool known = std::any_of (samples.begin(), samples.end(),
                                        [&f] (const SampleFile& s) { return s.file == f; });
        if (known)
            continue;

        if (auto s = SampleFile::fromAudioFile (f, formats))
        {
            samples.push_back (std::move (*s));
            ++added;
        }
    }

    if (added > 0)
    {
        selectedSample = (int) samples.size() - 1;
        notifyChanged();
    }

    return added;
}

//==============================================================================
void InstrumentModel::clear()
{
    name = "Neues Instrument";
    projectFile = juce::File();
    kind = InstrumentKind::melodic;
    kitPieces = standardDrumPieces();
    zones.clear();
    selectedZoneId.clear();
    samples.clear();
    selectedSample = 0;
    envelope = {};
    macros = { 0.5f, 0.5f, 0.5f, 0.5f };

    for (auto& targets : macroTargets)
        targets.clear();
    targets = {};
    notifyChanged();
}

void InstrumentModel::loadDemo()
{
    name = "Nocturne Hybrid Bass";
    kitPieces = standardDrumPieces();
    projectFile = juce::File();

    samples = {
        demoSample ("sub_bow_C1_sustain.wav", 4.21, 24, WaveShape::sustain, 3),
        demoSample ("sub_bow_C1_soft.wav",    3.90, 24, WaveShape::sustain, 4),
        demoSample ("attack_klick_A.wav",     0.31, 24, WaveShape::attack,  5),
        demoSample ("attack_klick_B.wav",     0.28, 24, WaveShape::attack,  6),
        demoSample ("luft_obertoene.wav",     2.60, 32, WaveShape::air,     7),
        demoSample ("metall_tail_lang.wav",   6.02, 24, WaveShape::tail,    8),
        demoSample ("noise_atmo_loop.wav",    8.00, 24, WaveShape::air,     9),
    };
    selectedSample = 0;

    // Zone "Sub Sustain" mit den drei Spuren aus dem Prototyp
    Zone sub;
    sub.id = "z1"; sub.name = "Sub Sustain";
    sub.lowNote = 24; sub.highNote = 47; sub.rootNote = 36;
    sub.colour = trackPalette()[0].main;
    {
        Track t = makeTrack ("Sub Sustain", "sub_bow_C1_sustain.wav", 0, WaveShape::sustain, 0.53 * timelineSeconds);
        t.offset = 0.0; t.gain = 0.82f; t.pan = 0.0f; t.pitch = -2; t.loop = LoopMode::sustainLoop;
        t.fadeIn = 0.03; t.fadeOut = 0.18;
        t.effects = { makeEffect ("Kanalfilter", false, { { "CUTOFF", 0.62f }, { "RESO", 0.28f } }),
                      makeEffect ("Sättigung"_u, false, { { "DRIVE", 0.41f }, { "MIX", 0.7f } }) };
        sub.tracks.push_back (t);
    }
    {
        Track t = makeTrack ("Attack Klick", "attack_klick_A.wav", 2, WaveShape::attack, 0.09 * timelineSeconds);
        t.offset = 0.01; t.gain = 0.54f; t.pan = -0.18f; t.pitch = 0; t.loop = LoopMode::oneShot;
        t.fadeIn = 0.0; t.fadeOut = 0.22;
        t.effects = { makeEffect ("Transienten", false, { { "ATTACK", 0.66f } }) };
        sub.tracks.push_back (t);
    }
    {
        Track t = makeTrack ("Luft / Obertöne"_u, "luft_obertoene.wav", 1, WaveShape::air, 0.33 * timelineSeconds);
        t.offset = 0.06; t.stretch = 1.28; t.gain = 0.36f; t.pan = 0.22f; t.pitch = 12;
        t.loop = LoopMode::sustainLoop; t.algorithm = StretchAlgorithm::smooth;
        t.fadeIn = 0.14; t.fadeOut = 0.2;
        t.effects = { makeEffect ("ValhallaVintage", true, { { "GRÖSSE"_u, 0.55f }, { "MIX", 0.24f } }) };
        sub.tracks.push_back (t);
    }

    Zone mid;
    mid.id = "z2"; mid.name = "Hybrid Mid";
    mid.lowNote = 48; mid.highNote = 66; mid.highVelocity = 86; mid.rootNote = 55;
    mid.colour = trackPalette()[1].main;
    mid.tracks = { makeTrack ("Mid Körper"_u, "sub_bow_C1_soft.wav", 1, WaveShape::sustain, 3.90),
                   makeTrack ("Attack B", "attack_klick_B.wav", 2, WaveShape::attack, 0.28),
                   makeTrack ("Luft", "luft_obertoene.wav", 0, WaveShape::air, 2.60),
                   makeTrack ("Atmo", "noise_atmo_loop.wav", 4, WaveShape::air, 8.00) };

    Zone hard;
    hard.id = "z3"; hard.name = "Hybrid Mid · hart"_u;
    hard.lowNote = 48; hard.highNote = 66; hard.lowVelocity = 87; hard.rootNote = 55;
    hard.colour = trackPalette()[1].main;
    hard.tracks = { makeTrack ("Mid hart", "sub_bow_C1_sustain.wav", 1, WaveShape::sustain, 4.21),
                    makeTrack ("Attack A", "attack_klick_A.wav", 2, WaveShape::attack, 0.31) };

    Zone air;
    air.id = "z4"; air.name = "Air Top";
    air.lowNote = 67; air.highNote = 83; air.rootNote = 72;
    air.colour = trackPalette()[2].main;
    air.tracks = { makeTrack ("Luft", "luft_obertoene.wav", 1, WaveShape::air, 2.60),
                   makeTrack ("Atmo", "noise_atmo_loop.wav", 4, WaveShape::air, 8.00),
                   makeTrack ("Attack B", "attack_klick_B.wav", 2, WaveShape::attack, 0.28) };

    Zone tail;
    tail.id = "z5"; tail.name = "FX Tail";
    tail.lowNote = 84; tail.highNote = 95; tail.rootNote = 88;
    tail.colour = trackPalette()[3].main;
    tail.tracks = { makeTrack ("Metall-Tail", "metall_tail_lang.wav", 3, WaveShape::tail, 6.02) };

    zones = { sub, mid, hard, air, tail };
    selectedZoneId = "z1";

    envelope = {};
    macros = { 0.62f, 0.35f, 0.5f, 0.18f };
    targets = {};
    notifyChanged();
}

//==============================================================================
juce::ValueTree InstrumentModel::toValueTree() const
{
    juce::ValueTree root (id::instrument);
    root.setProperty (id::name, name, nullptr);
    root.setProperty (id::kind, (int) kind, nullptr);
    root.setProperty (id::kitPieces, kitPieces.joinIntoString (","), nullptr);
    root.setProperty (id::selectedZone, selectedZoneId, nullptr);
    root.setProperty (id::selectedSample, selectedSample, nullptr);

    for (const auto& s : samples)
    {
        juce::ValueTree node (id::sample);
        node.setProperty (id::name, s.name, nullptr);
        node.setProperty (id::path, s.file.getFullPathName(), nullptr);
        node.setProperty (id::length, s.lengthSeconds, nullptr);
        node.setProperty (id::bitDepth, s.bitDepth, nullptr);
        node.setProperty (id::shape, (int) s.shape, nullptr);
        root.appendChild (node, nullptr);
    }

    for (const auto& z : zones)
    {
        juce::ValueTree zn (id::zone);
        zn.setProperty (id::uid, z.id, nullptr);
        zn.setProperty (id::name, z.name, nullptr);
        zn.setProperty (id::lowNote, z.lowNote, nullptr);
        zn.setProperty (id::highNote, z.highNote, nullptr);
        zn.setProperty (id::lowVelocity, z.lowVelocity, nullptr);
        zn.setProperty (id::highVelocity, z.highVelocity, nullptr);
        zn.setProperty (id::rootNote, z.rootNote, nullptr);
        zn.setProperty (id::drumPart, z.drumPart, nullptr);
        zn.setProperty (id::colour, z.colour.toString(), nullptr);
        zn.setProperty (id::selectedTrack, z.selectedTrack, nullptr);

        for (const auto& t : z.tracks)
        {
            juce::ValueTree tn (id::track);
            tn.setProperty (id::name, t.name, nullptr);
            tn.setProperty (id::clip, t.clip, nullptr);
            tn.setProperty (id::colour, t.colour.toString(), nullptr);
            tn.setProperty (id::softColour, t.softColour.toString(), nullptr);
            tn.setProperty (id::shape, (int) t.shape, nullptr);
            tn.setProperty (id::offset, t.offset, nullptr);
            tn.setProperty (id::natural, t.natural, nullptr);
            tn.setProperty (id::stretch, t.stretch, nullptr);
            tn.setProperty (id::trimStart, t.trimStart, nullptr);
            tn.setProperty (id::trimEnd, t.trimEnd, nullptr);
            tn.setProperty (id::fadeIn, t.fadeIn, nullptr);
            tn.setProperty (id::fadeOut, t.fadeOut, nullptr);
            tn.setProperty (id::gain, t.gain, nullptr);
            tn.setProperty (id::pan, t.pan, nullptr);
            tn.setProperty (id::pitch, t.pitch, nullptr);
            tn.setProperty (id::cents, t.cents, nullptr);
            tn.setProperty (id::reverse, t.reverse, nullptr);
            tn.setProperty (id::loop, (int) t.loop, nullptr);
            tn.setProperty (id::loopStart, t.loopStart, nullptr);
            tn.setProperty (id::loopCrossfade, t.loopCrossfade, nullptr);
            tn.setProperty (id::algorithm, (int) t.algorithm, nullptr);
            tn.setProperty (id::mute, t.mute, nullptr);
            tn.setProperty (id::solo, t.solo, nullptr);

            for (const auto& e : t.effects)
            {
                juce::ValueTree en (id::effect);
                en.setProperty (id::name, e.name, nullptr);
                en.setProperty (id::shape, (int) e.type, nullptr);
                en.setProperty (id::enabled, e.enabled, nullptr);
                en.setProperty (id::external, e.external, nullptr);
                en.setProperty (id::clip, e.pluginIdentifier, nullptr);
                en.setProperty (id::path, e.pluginState, nullptr);

                for (const auto& p : e.parameters)
                {
                    juce::ValueTree pn (id::parameter);
                    pn.setProperty (id::label, p.label, nullptr);
                    pn.setProperty (id::value, p.value, nullptr);
                    en.appendChild (pn, nullptr);
                }
                tn.appendChild (en, nullptr);
            }
            zn.appendChild (tn, nullptr);
        }
        root.appendChild (zn, nullptr);
    }

    juce::ValueTree env (id::envelope);
    env.setProperty (id::attack, envelope.attack, nullptr);
    env.setProperty (id::decay, envelope.decay, nullptr);
    env.setProperty (id::sustain, envelope.sustain, nullptr);
    env.setProperty (id::release, envelope.release, nullptr);
    root.appendChild (env, nullptr);

    juce::ValueTree mac (id::macros);

    for (int macro = 0; macro < numMacros; ++macro)
    {
        for (const auto& target : macroTargets[(size_t) macro])
        {
            juce::ValueTree node ("Target");
            node.setProperty ("macro", macro, nullptr);
            node.setProperty ("zone", target.zoneId, nullptr);
            node.setProperty ("track", target.trackIndex, nullptr);
            node.setProperty ("effect", target.effectIndex, nullptr);
            node.setProperty ("parameter", target.parameterIndex, nullptr);
            mac.appendChild (node, nullptr);
        }
    }
    for (size_t i = 0; i < macros.size(); ++i)
        mac.setProperty ("m" + juce::String ((int) i + 1), macros[i], nullptr);
    root.appendChild (mac, nullptr);

    juce::ValueTree tg (id::targets);
    tg.setProperty (id::vst3, targets.vst3, nullptr);
    tg.setProperty (id::audioUnit, targets.audioUnit, nullptr);
    tg.setProperty (id::standalone, targets.standalone, nullptr);
    tg.setProperty (id::project, targets.project, nullptr);
    root.appendChild (tg, nullptr);

    return root;
}

void makeSamplePathsRelative (juce::ValueTree instrument, const juce::File& folder)
{
    if (! instrument.hasType (id::instrument) || folder == juce::File())
        return;

    for (auto node : instrument)
    {
        if (! node.hasType (id::sample))
            continue;

        const juce::File file (node.getProperty (id::path).toString());

        if (file != juce::File() && file.isAChildOf (folder))
            node.setProperty (id::path, file.getRelativePathFrom (folder), nullptr);
    }
}

juce::ValueTree withoutSelection (const juce::ValueTree& instrument)
{
    auto copy = instrument.createCopy();
    copy.removeProperty (id::selectedZone, nullptr);
    copy.removeProperty (id::selectedSample, nullptr);

    for (auto node : copy)
        if (node.hasType (id::zone))
            node.removeProperty (id::selectedTrack, nullptr);

    return copy;
}

bool InstrumentModel::fromValueTree (const juce::ValueTree& root, juce::AudioFormatManager& formats,
                                     const juce::File& baseFolder)
{
    if (! root.hasType (id::instrument))
        return false;

    name = root.getProperty (id::name, "Instrument").toString();
    kind = readEnum (root, id::kind, InstrumentKind::melodic, 2);
    zones.clear();
    samples.clear();

    for (auto& targets : macroTargets)
        targets.clear();

    int sampleSeed = 3;
    for (const auto& node : root)
    {
        if (node.hasType (id::sample))
        {
            const auto storedPath = node.getProperty (id::path).toString();
            juce::File f;

            if (storedPath.isNotEmpty())
                f = juce::File::isAbsolutePath (storedPath)
                        ? juce::File (storedPath)
                        : (baseFolder != juce::File() ? baseFolder.getChildFile (storedPath) : juce::File());
            const auto shape = readEnum (node, id::shape, WaveShape::sustain, 4);

            if (auto real = f.existsAsFile() ? SampleFile::fromAudioFile (f, formats) : std::nullopt)
            {
                samples.push_back (std::move (*real));
            }
            else
            {
                // Beispiel-Eintrag oder fehlende Datei: Metadaten behalten, Platzhalter-Wellenform
                SampleFile s = demoSample (node.getProperty (id::name).toString(),
                                           (double) node.getProperty (id::length, 0.0),
                                           (int) node.getProperty (id::bitDepth, 24), shape, sampleSeed);
                s.file = f;
                samples.push_back (std::move (s));
            }
            ++sampleSeed;
        }
        else if (node.hasType (id::zone))
        {
            Zone z;
            z.id = node.getProperty (id::uid, juce::Uuid().toString()).toString();
            z.name = node.getProperty (id::name).toString();
            z.lowNote = juce::jlimit (0, 127, (int) node.getProperty (id::lowNote, 60));
            z.highNote = juce::jlimit (z.lowNote, 127, (int) node.getProperty (id::highNote, 71));
            z.lowVelocity = juce::jlimit (0, 127, (int) node.getProperty (id::lowVelocity, 0));
            z.highVelocity = juce::jlimit (z.lowVelocity, 127, (int) node.getProperty (id::highVelocity, 127));
            z.rootNote = juce::jlimit (0, 127, (int) node.getProperty (id::rootNote, 60));
            z.drumPart = node.getProperty (id::drumPart).toString();
            z.colour = readColour (node, id::colour, trackPalette()[4].main);
            z.selectedTrack = (int) node.getProperty (id::selectedTrack, 0);

            for (const auto& tn : node)
            {
                if (! tn.hasType (id::track))
                    continue;

                Track t;
                t.name = tn.getProperty (id::name, "Spur").toString();
                t.clip = tn.getProperty (id::clip, "leer").toString();
                t.colour = readColour (tn, id::colour, t.colour);
                t.softColour = readColour (tn, id::softColour, t.softColour);
                t.shape = readEnum (tn, id::shape, WaveShape::air, 4);
                t.offset = tn.getProperty (id::offset, t.offset);
                t.natural = tn.getProperty (id::natural, t.natural);
                t.stretch = juce::jlimit (0.25, 4.0, (double) tn.getProperty (id::stretch, t.stretch));
                t.trimStart = tn.getProperty (id::trimStart, t.trimStart);
                t.trimEnd = tn.getProperty (id::trimEnd, t.trimEnd);
                t.fadeIn = tn.getProperty (id::fadeIn, t.fadeIn);
                t.fadeOut = tn.getProperty (id::fadeOut, t.fadeOut);
                t.gain = tn.getProperty (id::gain, t.gain);
                t.pan = tn.getProperty (id::pan, t.pan);
                t.pitch = juce::jlimit (-24, 24, (int) tn.getProperty (id::pitch, t.pitch));
                t.cents = tn.getProperty (id::cents, t.cents);
                t.reverse = tn.getProperty (id::reverse, t.reverse);
                t.loop = readEnum (tn, id::loop, LoopMode::oneShot, 3);
                t.loopStart = juce::jlimit (0.0, 0.9, (double) tn.getProperty (id::loopStart, 0.0));
                t.loopCrossfade = juce::jlimit (0.0, 0.5, (double) tn.getProperty (id::loopCrossfade, 0.0));
                t.algorithm = readEnum (tn, id::algorithm, StretchAlgorithm::transientPreserving, 4);
                t.mute = tn.getProperty (id::mute, false);
                t.solo = tn.getProperty (id::solo, false);

                for (const auto& en : tn)
                {
                    if (! en.hasType (id::effect))
                        continue;

                    Effect e;
                    e.name = en.getProperty (id::name).toString();
                    e.type = readEnum (en, id::shape, EffectType::generic, 24);
                    e.enabled = en.getProperty (id::enabled, true);
                    e.external = en.getProperty (id::external, false);
                    e.pluginIdentifier = en.getProperty (id::clip).toString();
                    e.pluginState = en.getProperty (id::path).toString();

                    for (const auto& pn : en)
                        if (pn.hasType (id::parameter))
                            e.parameters.push_back ({ pn.getProperty (id::label).toString(),
                                                      (float) pn.getProperty (id::value, 0.5f) });

                    t.effects.push_back (std::move (e));
                }

                z.tracks.push_back (std::move (t));
            }

            zones.push_back (std::move (z));
        }
        else if (node.hasType (id::envelope))
        {
            envelope.attack = node.getProperty (id::attack, envelope.attack);
            envelope.decay = node.getProperty (id::decay, envelope.decay);
            envelope.sustain = node.getProperty (id::sustain, envelope.sustain);
            envelope.release = node.getProperty (id::release, envelope.release);
        }
        else if (node.hasType (id::macros))
        {
            for (size_t i = 0; i < macros.size(); ++i)
                macros[i] = node.getProperty ("m" + juce::String ((int) i + 1), macros[i]);

            for (const auto& target : node)
            {
                if (! target.hasType (juce::Identifier ("Target")))
                    continue;

                const int macro = target.getProperty ("macro", -1);

                if (! juce::isPositiveAndBelow (macro, numMacros))
                    continue;

                MacroTarget assigned;
                assigned.zoneId = target.getProperty ("zone").toString();
                assigned.trackIndex = target.getProperty ("track", 0);
                assigned.effectIndex = target.getProperty ("effect", 0);
                assigned.parameterIndex = target.getProperty ("parameter", 0);

                macroTargets[(size_t) macro].push_back (assigned);
            }
        }
        else if (node.hasType (id::targets))
        {
            targets.vst3 = node.getProperty (id::vst3, targets.vst3);
            targets.audioUnit = node.getProperty (id::audioUnit, targets.audioUnit);
            targets.standalone = node.getProperty (id::standalone, targets.standalone);
            targets.project = node.getProperty (id::project, targets.project);
        }
    }

    /* Welche Teile im Kit stehen. Ältere Dateien kennen die Angabe nicht – dann das
       Standard-Kit. In beiden Fällen kommt jedes Teil dazu, das schon eine Zone hat:
       sonst verschwände etwa die Clap eines alten Projekts aus der Zeichnung. */
    kitPieces.clear();

    if (root.hasProperty (id::kitPieces))
        kitPieces.addTokens (root.getProperty (id::kitPieces).toString(), ",", {});
    else
        kitPieces = standardDrumPieces();

    kitPieces.removeEmptyStrings();

    for (const auto& z : zones)
        if (const auto* piece = findPieceForPart (z.drumPart))
            kitPieces.addIfNotAlreadyThere (piece->id);

    selectedZoneId = root.getProperty (id::selectedZone).toString();
    if (findZone (selectedZoneId) == nullptr)
        selectedZoneId = zones.empty() ? juce::String() : zones.front().id;

    selectedSample = juce::jlimit (0, juce::jmax (0, (int) samples.size() - 1),
                                   (int) root.getProperty (id::selectedSample, 0));

    notifyChanged();
    return true;
}

} // namespace sis
