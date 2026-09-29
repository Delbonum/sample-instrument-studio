#pragma once

#include <JuceHeader.h>
#include <array>
#include <optional>
#include <vector>

#include "DemoWaveform.h"
#include "DrumKit.h"
#include "Text.h"

namespace sis
{
enum class LoopMode { oneShot, sustainLoop, pingPong };
enum class StretchAlgorithm { transientPreserving, smooth, monophonic, granular };

juce::String toDisplayString (LoopMode);
juce::String toDisplayString (StretchAlgorithm);

struct EffectParameter
{
    juce::String label;
    float value = 0.5f;
};

/** Welche Art Effekt. Umgesetzt sind Equalizer, Kompressor und Hall. */
// Neue Arten **hinten** anhängen: der Zahlenwert steht so in den Projektdateien.
enum class EffectType { generic, equalizer, compressor, reverb, external,
                        channelFilter, saturation, transients, chorus, bitCrusher,
                        overdrive, distortion, flanger, vibrato, phaser, tremolo,
                        delay, noiseGate, expander, limiter, deEsser,
                        envelopeFilter, autoWah, channelStrip };

/** Effekt in der Kette einer Spur – intern oder externes Plugin (VST3 / AU). */
struct Effect
{
    juce::String name;
    EffectType type = EffectType::generic;
    bool enabled = true;
    bool external = false;
    std::vector<EffectParameter> parameters;

    /** Nur bei fremden Plugins: Kennung aus der Plugin-Liste und gespeicherter Zustand. */
    juce::String pluginIdentifier;
    juce::String pluginState;

    juce::String kindLabel() const;

    /** Vier Bänder mit fester Frequenz; die Parameter 0 … 1 steuern die Anhebung ±12 dB. */
    static Effect makeEqualizer();

    /** Schwelle, Verhältnis, Ansprechen, Loslassen und Ausgleich. */
    static Effect makeCompressor();

    /** Größe, Dämpfung, Breite und Hallanteil. */
    static Effect makeReverb();

    /** Fremdes Plugin, das über die Plugin-Liste geladen wird. */
    static Effect makeExternal (const juce::String& name, const juce::String& identifier);

    /** Hoch- und Tiefpass, um eine Spur im Mix freizustellen. */
    static Effect makeChannelFilter();

    /** Weiche Sättigung über eine Tangens-Kennlinie. */
    static Effect makeSaturation();

    /** Anschlag und Ausklingen getrennt formen. */
    static Effect makeTransients();

    /** Chorus über eine modulierte Verzögerung. */
    static Effect makeChorus();

    /** Gröbere Auflösung und gröbere Abtastung. */
    static Effect makeBitCrusher();

    /** Weiches, unsymmetrisches Anzerren. */
    static Effect makeOverdrive();

    /** Hart übersteuern, bis zum glatten Abschneiden. */
    static Effect makeDistortion();

    /** Wandernder Kamm aus Auslöschungen, mit Rückkopplung. */
    static Effect makeFlanger();

    /** Schwankende Tonhöhe, ohne Trockenanteil. */
    static Effect makeVibrato();

    /** Wandernde Kerben aus einer Allpass-Kette. */
    static Effect makePhaser();

    /** Schwankender Pegel, bei voller Breite ein Auto-Panorama. */
    static Effect makeTremolo();

    /** Echo mit Rückkopplung, Dämpfung und Ping-Pong. */
    static Effect makeDelay();

    /** Schließt unterhalb einer Schwelle, mit Haltezeit. */
    static Effect makeNoiseGate();

    /** Macht Leises leiser, stufenlos nach Verhältnis. */
    static Effect makeExpander();

    /** Fängt Spitzen ab, ohne die Decke zu überschreiten. */
    static Effect makeLimiter();

    /** Nimmt Schärfe heraus, ohne den ganzen Ton zu ducken. */
    static Effect makeDeEsser();

    /** Filter, das dem Anschlag folgt: lauter heißt offener. */
    static Effect makeEnvelopeFilter();

    /** Filter, das gleichmäßig zwischen zwei Frequenzen wandert. */
    static Effect makeAutoWah();

    /** Gate, Klangregelung und Kompressor in einer Karte. */
    static Effect makeChannelStrip();

    /** Alle eingebauten Effekte in der Reihenfolge des Menüs. */
    static std::vector<Effect> builtIn();
};

struct SampleFile;

/** Ein Stück Sample auf der Zeitachse einer Spur.

    Zeitwerte (offset, natural, Länge) sind Anteile der 8-Sekunden-Achse, trim ist ein
    Anteil des Samples, die Fades Anteile der Clip-Länge.

    Eine Spur kann beliebig viele Clips tragen. Wo sie sich überlappen, klingt nur der
    obere – der, der in `Track::clips` weiter hinten steht, also zuletzt gesetzt wurde.
    Nur in seinen Fades scheint der untere durch: so entsteht ein Crossfade (Taste X). */
struct Clip
{
    juce::String sample;    // Name des Samples in `InstrumentModel::samples`
    WaveShape shape = WaveShape::air;

    double offset = 0.0;
    double natural = 0.3;
    double stretch = 1.0;
    double trimStart = 0.0;
    double trimEnd = 1.0;
    double fadeIn = 0.0;
    double fadeOut = 0.0;

    /** Kennung zur Laufzeit, für Auswahl und Ziehen. Steht nicht in der Datei: nach dem
        Laden (und nach Rückgängig) bekommt jeder Clip eine neue. */
    juce::uint32 uid = nextUid();

    double length() const noexcept { return natural * stretch * (trimEnd - trimStart); }
    double end() const noexcept    { return offset + length(); }

    /** Ob ein Sample dahinter steht. „leer“ war früher der Platzhalter einer leeren Spur
        und steht noch in alten Projektdateien. */
    bool hasSample() const { return sample.isNotEmpty() && sample != "leer"; }

    /** Neuer Clip aus einem Sample, ungeschnitten und ohne Fades. */
    static Clip fromSample (const SampleFile&, double offset);

    static juce::uint32 nextUid() noexcept;
};

/** Eine Spur (Layer) innerhalb einer Zone: Pegel, Tonhöhe, Loop-Verhalten und
    Effektkette gelten für alle ihre Clips. */
struct Track
{
    juce::String name { "Spur" };
    juce::Colour colour { 0xff6e6b66 };
    juce::Colour softColour { 0xffefeeea };

    std::vector<Clip> clips;   // Reihenfolge = Stapel: der letzte liegt oben

    float gain = 0.5f;
    float pan = 0.0f;       // -1 … +1
    int pitch = 0;          // Halbtöne, -24 … +24
    float cents = 0.5f;     // 0 … 1, 0.5 = ±0 ct
    bool reverse = false;
    LoopMode loop = LoopMode::oneShot;

    /** Nur bei Sustain-Loop und Vor/Rückwärts, beides als Anteil des **gespielten**
        Ausschnitts eines Clips (nach dem Zuschnitt): ab wo geloopt wird (davor liegt der
        Anschlag, der nur einmal klingt) und wie lang an der Nahtstelle übergeblendet wird.
        Ohne Überblendung ist die Naht hörbar, sobald Anfang und Ende der Schleife nicht
        zufällig zusammenpassen. Eine Schleife klingt bis zum nächsten Clip der Spur. */
    double loopStart = 0.0;
    double loopCrossfade = 0.0;

    StretchAlgorithm algorithm = StretchAlgorithm::transientPreserving;
    bool mute = false;
    bool solo = false;

    std::vector<Effect> effects;

    bool hasClips() const;

    /** Clip mit dieser Kennung, sonst nullptr. */
    Clip* findClip (juce::uint32 uid);
    const Clip* findClip (juce::uint32 uid) const;

    /** Wo der letzte Clip endet (0 ohne Clips). */
    double end() const;

    /** Legt ein Sample als neuen, obersten Clip auf die Spur. Heißt die Spur noch wie
        vergeben („Spur 2“), bekommt sie den Namen des Samples. */
    Clip& addSample (const SampleFile&, double offset);
};

/** Tastatur-Zone: Tastenbereich × Velocity-Bereich, mit eigenen Spuren. */
struct Zone
{
    juce::String id;
    juce::String name;
    int lowNote = 60;
    int highNote = 71;
    int lowVelocity = 0;
    int highVelocity = 127;
    int rootNote = 64;
    juce::Colour colour { 0xff6e6b66 };

    /** Bei einem Drumset: welches Teil des Kits diese Zone ist (`DrumKit.h`).
        Leer bei einem Tonhöhen-Instrument. */
    juce::String drumPart;

    std::vector<Track> tracks;
    int selectedTrack = 0;

    bool containsNote (int note) const noexcept           { return note >= lowNote && note <= highNote; }
    bool containsVelocity (int velocity) const noexcept   { return velocity >= lowVelocity && velocity <= highVelocity; }

    /** Anzahl verschiedener Sample-Dateien, die die Spuren der Zone verwenden. */
    int numSamples() const;

    Track* getSelectedTrack();
    const Track* getSelectedTrack() const;

    /** Wo der letzte Clip endet, in Anteilen der Zeitachse (0, wenn es keinen gibt). */
    double contentEnd() const;

    /** Die Spur, auf der ein Clip liegt, sonst -1. */
    int trackOfClip (juce::uint32 uid) const;
};

/** Name eines Makros, wie er in beiden Ansichten steht. */
juce::String macroName (int index);

/** Familien für das Menü „Effekt hinzufügen“ – bei zweiundzwanzig Einträgen wird eine
    flache Liste unübersichtlich. */
enum class EffectFamily { filter, drive, modulation, time, dynamics };

/** Zu welcher Familie eine Effektart gehört. Leer für Arten, die nicht im Menü stehen
    (der Platzhalter und fremde Plugins). */
std::optional<EffectFamily> familyOf (EffectType) noexcept;

/** Überschrift der Familie im Menü. */
juce::String toDisplayString (EffectFamily);

/** Reihenfolge der Untermenüs. */
const std::vector<EffectFamily>& effectFamilyOrder();

/** Eintrag im Sample-Browser. */
struct SampleFile
{
    static constexpr int peakResolution = 120;

    juce::String name;
    juce::File file;              // leer bei Beispieldaten
    double lengthSeconds = 0.0;
    int bitDepth = 24;
    WaveShape shape = WaveShape::sustain;
    std::vector<float> peaks;     // 0 … 1, peakResolution Werte

    juce::String metaText() const;

    static std::optional<SampleFile> fromAudioFile (const juce::File&, juce::AudioFormatManager&);
};

/** Ziel einer Makro-Zuweisung: ein Regler eines Effekts einer Spur.

    Gespeichert wird über die Zonen-Kennung und Nummern, nicht über Zeiger – das
    Instrument wird beim Laden neu aufgebaut, Zeiger überlebten das nicht. */
struct MacroTarget
{
    juce::String zoneId;
    int trackIndex = 0;
    int effectIndex = 0;
    int parameterIndex = 0;

    bool operator== (const MacroTarget& other) const noexcept
    {
        return zoneId == other.zoneId && trackIndex == other.trackIndex
            && effectIndex == other.effectIndex && parameterIndex == other.parameterIndex;
    }
};

/** ADSR, jeweils normiert 0 … 1 (Anzeige: A×2000 ms, D×3000 ms, S als dB, R×5000 ms). */
struct Envelope
{
    float attack = 0.06f;
    float decay = 0.3f;
    float sustain = 0.72f;
    float release = 0.44f;
};

struct ExportTargets
{
    bool vst3 = true;
    bool audioUnit = true;
    bool standalone = false;
    bool project = true;
};

/** Welche Art Instrument gebaut wird.

    Der Unterschied ist nicht bloß die Ansicht: ein **Drumset** spielt jedes Sample in
    seiner eigenen Tonhöhe, egal welche Taste es auslöst. Ein Tonhöhen-Instrument
    verstimmt das Sample um den Abstand zum Grundton. Genau das trennt ein Schlagzeug von
    einem gesampelten Klavier, und deshalb hängt es am Instrument und nicht an der Ansicht.

    Die Zahlen stehen in Projektdateien – **nur hinten anhängen**. */
enum class InstrumentKind { melodic, drumKit };

/** „Tonhöhen-Instrument“ / „Drumset“. */
juce::String toDisplayString (InstrumentKind);

/** Das Instrument – alles, was in einer .sisp-Datei steht.
    Wird ausschließlich auf dem Message-Thread verändert; Änderungen
    werden über ChangeBroadcaster an die Oberfläche gemeldet. */
class InstrumentModel : public juce::ChangeBroadcaster
{
public:
    static constexpr int lowestNote = 24;   // C1
    static constexpr int highestNote = 95;  // B6
    static constexpr int numNotes = highestNote - lowestNote + 1;
    static constexpr int numMacros = 4;
    static constexpr double timelineSeconds = 8.0;

    InstrumentModel();

    juce::String name;
    juce::File projectFile;
    InstrumentKind kind = InstrumentKind::melodic;

    std::vector<Zone> zones;
    juce::String selectedZoneId;

    std::vector<SampleFile> samples;
    int selectedSample = 0;

    Envelope envelope;
    std::array<float, numMacros> macros { 0.62f, 0.35f, 0.5f, 0.18f };
    std::array<std::vector<MacroTarget>, numMacros> macroTargets;
    ExportTargets targets;

    Zone* getSelectedZone();
    const Zone* getSelectedZone() const;
    Zone* findZone (const juce::String& id);
    const SampleFile* findSample (const juce::String& fileName) const;

    /** Wellenform eines Clips: echte Spitzenwerte, sonst Platzhalter aus der Form. */
    std::vector<float> waveformFor (const Clip&, int seed) const;

    /** Erste Zone, die Note (und, wenn velocity >= 0, Velocity) enthält. */
    const Zone* findZoneForNote (int note, int velocity = -1) const;

    /** Legt den Wert eines Makros auf alle zugewiesenen Regler. */
    void applyMacro (int macro);

    /** Welches Makro diesen Regler steuert, sonst -1. */
    int macroFor (const MacroTarget&) const;

    /** Weist zu; `macro < 0` löst eine vorhandene Zuweisung. Ein Regler gehört zu
        höchstens einem Makro. */
    void assignMacro (int macro, const MacroTarget&);

    void selectZone (const juce::String& id);
    Zone& addZone();

    /** Zone eines Kit-Teils, sonst nullptr. */
    Zone* findZoneForDrumPart (const juce::String& partId);
    const Zone* findZoneForDrumPart (const juce::String& partId) const;

    /** Legt die Zone eines Kit-Klangs an: eine einzige Taste, Grundton = diese Taste.
        Gibt es sie schon, wird sie nur gewählt. Das Teil kommt dabei ins Kit, falls es
        noch fehlt. */
    Zone& addDrumZone (const DrumPart&);

    /** Welche Teile im Kit aufgebaut sind (Kennungen aus `drumPieces()`). Ein Teil ohne
        Zone wird blass gezeichnet; ein Teil, das hier fehlt, gar nicht. */
    juce::StringArray kitPieces = standardDrumPieces();

    bool hasKitPiece (const juce::String& pieceId) const { return kitPieces.contains (pieceId); }

    /** Stellt ein Teil ins Kit (ohne Zone – die entsteht beim ersten Klick oder Sample). */
    void addKitPiece (const DrumPiece&);

    /** Nimmt ein Teil aus dem Kit, samt den Zonen aller seiner Spielweisen. */
    void removeKitPiece (const DrumPiece&);

    /** Wie viele Toms gerade aufgebaut sind. */
    int numKitToms() const;

    /** Löscht eine Zone. Makro-Zuweisungen auf sie werden gelöst. */
    void removeZone (const juce::String& zoneId);

    /** Löscht eine Spur. Makro-Zuweisungen zeigen über die Nummer der Spur – die auf
        die gelöschte Spur fallen weg, die dahinter rücken nach. */
    void removeTrack (Zone&, int trackIndex);

    /** Legt ein Sample als Spur in die Zone. Hat die Zone eine Spur ohne Clips, bekommt die
        das Sample, statt dass eine weitere dazukommt. Gibt die Nummer der Spur zurück. */
    int addSampleTrack (Zone&, const SampleFile&);

    /** Neue Zone um eine Taste herum: so breit, wie Platz ist (höchstens eine Oktave),
        und im Velocity-Bereich, den dort noch keine Zone belegt. Grundton ist die Taste. */
    Zone& addZoneAt (int note, int velocity);

    /** Setzt die Art um. Beim Wechsel zum Drumset behalten vorhandene Zonen ihre Tasten –
        geworfen wird nichts weg. */
    void setKind (InstrumentKind);

    /** Liest Länge, Bittiefe und Wellenform-Spitzen. Gibt die Anzahl neuer Einträge zurück. */
    int importSamples (const juce::Array<juce::File>&, juce::AudioFormatManager&);

    static bool isSupportedAudioFile (const juce::String& path);

    void loadDemo();
    void clear();

    juce::ValueTree toValueTree() const;

    /** `baseFolder` (der Ordner der Projektdatei) löst relative Sample-Pfade auf. */
    bool fromValueTree (const juce::ValueTree&, juce::AudioFormatManager&,
                        const juce::File& baseFolder = juce::File());

    void notifyChanged() { sendChangeMessage(); }
};

/** Schreibt die Sample-Pfade eines Instrument-Baums relativ zu `folder`,
    sofern die Datei dort liegt – damit ein exportierter Ordner umziehen kann. */
void makeSamplePathsRelative (juce::ValueTree instrument, const juce::File& folder);

/** Kopie eines Instrument-Baums ohne alles, was nur die Auswahl betrifft.
    Damit lassen sich zwei Stände vergleichen, ohne dass bloßes Anklicken als
    Änderung zählt (siehe UndoHistory). */
juce::ValueTree withoutSelection (const juce::ValueTree& instrument);

struct TrackColour
{
    juce::Colour main, soft;
};

/** Spurfarben aus den Design-Tokens. */
const std::array<TrackColour, 5>& trackPalette();

} // namespace sis
