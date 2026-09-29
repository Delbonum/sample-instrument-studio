#pragma once

#include <JuceHeader.h>

namespace sis
{
/** Ein Klang des Schlagzeugs – eine Zone, eine Taste.

    Die Notennummern folgen der **General-MIDI-Belegung**. Das ist keine Kosmetik: wer
    ein fertiges Drum-Pattern aus seiner DAW auf das Instrument spielt, erwartet die
    Bassdrum auf 36 und die Snare auf 38. Eine eigene Belegung wäre nur dann besser, wenn
    es gar keine verbreitete gäbe. (Der Flam hat in General MIDI keine Note; er liegt
    unterhalb der Belegung auf 31.)

    Die Kennung (`id`) steht in der Projektdatei und darf sich **nicht** ändern; Name und
    Note dürfen. Die Note ist die Voreinstellung — im Instrument hängt sie an der Zone
    und ist dort auch verschiebbar. */
struct DrumPart
{
    const char* id;
    const char* name;
    int note;              // General MIDI
    const char* shortName; // auf dem Umschalter eines Teils mit mehreren Spielweisen
};

/** Alle Klänge, die ein Kit haben kann. */
const std::vector<DrumPart>& drumParts();

/** Klang zu einer Kennung, sonst nullptr. */
const DrumPart* findDrumPart (const juce::String& id);

//==============================================================================
enum class DrumPieceKind { kick, snare, hiHat, rackTom, floorTom, cymbal, percussion };

/** Ein Teil, wie es im Kit steht: eine Trommel oder ein Becken.

    Ein Teil kann mehrere **Spielweisen** haben – die HiHat zu, offen und getreten, die
    Snare mit Rim-Shot, Rim-Click und Flam, das Ride auf der Fläche und auf der Glocke.
    Jede Spielweise ist ein eigener `DrumPart` mit eigener Zone und eigener Note (anders
    bekäme eine DAW sie nicht auseinander); gezeichnet wird das Teil trotzdem nur einmal,
    mit einem Umschalter. Ein Schlagzeug mit drei HiHats sähe nicht mehr aus wie eines.

    Auch diese Kennung steht in der Projektdatei (welche Teile aufgebaut sind). */
struct DrumPiece
{
    const char* id;
    const char* name;
    DrumPieceKind kind;
    std::vector<const char*> articulations;   // Kennungen aus drumParts(), die erste ist die Grundform
    bool standard;                            // gehört zu einem neuen Kit
};

/** Alle Teile, in der Reihenfolge des Menüs „Teil hinzufügen“. */
const std::vector<DrumPiece>& drumPieces();

/** Teil zu einer Kennung, sonst nullptr. */
const DrumPiece* findDrumPiece (const juce::String& id);

/** Das Teil, zu dem ein Klang gehört (die Snare zum Rim-Shot), sonst nullptr. */
const DrumPiece* findPieceForPart (const juce::String& partId);

/** Die Teile eines neuen Kits: Kick, Snare, HiHat, drei Toms, Crash und Ride. */
juce::StringArray standardDrumPieces();

/** Mehr Toms passen nicht sinnvoll um eine Bassdrum. */
constexpr int maxDrumToms = 5;

inline bool isTom (DrumPieceKind kind) noexcept
{
    return kind == DrumPieceKind::rackTom || kind == DrumPieceKind::floorTom;
}
} // namespace sis
