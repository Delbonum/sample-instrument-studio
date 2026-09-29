#include "DrumKit.h"

namespace sis
{
const std::vector<DrumPart>& drumParts()
{
    static const std::vector<DrumPart> parts {
        { "kick",         "Kick",            36, "Kick" },
        { "snare",        "Snare",           38, "Fell" },
        { "snareRim",     "Snare Rim-Shot",  40, "Rim" },
        { "sidestick",    "Snare Rim-Click", 37, "Klick" },   // Kennung aus der Zeit, als er „Sidestick“ hieß
        { "snareFlam",    "Snare Flam",      31, "Flam" },
        { "clap",         "Clap",            39, "Clap" },
        { "hihatClosed",  "HiHat zu",        42, "zu" },
        { "hihatOpen",    "HiHat offen",     46, "offen" },
        { "hihatPedal",   "HiHat Fuß",       44, "Fuß" },
        { "tomHigh",      "Tom hoch",        48, "Tom" },
        { "tomMid",       "Tom mittel",      47, "Tom" },
        { "tomLow",       "Tom tief",        45, "Tom" },
        { "tomFloorHigh", "Standtom hoch",   43, "Tom" },
        { "tomFloor",     "Standtom",        41, "Tom" },
        { "crash",        "Crash",           49, "Crash" },
        { "crash2",       "Crash 2",         57, "Crash" },
        { "ride",         "Ride",            51, "Fläche" },
        { "rideBell",     "Ride-Glocke",     53, "Glocke" },
        { "china",        "China",           52, "China" },
        { "splash",       "Splash",          55, "Splash" },
        { "cowbell",      "Cowbell",         56, "Cowbell" },
        { "tambourine",   "Tamburin",        54, "Tamburin" },
    };

    return parts;
}

const DrumPart* findDrumPart (const juce::String& id)
{
    for (const auto& part : drumParts())
        if (id == part.id)
            return &part;

    return nullptr;
}

const std::vector<DrumPiece>& drumPieces()
{
    using K = DrumPieceKind;

    static const std::vector<DrumPiece> pieces {
        { "kick",         "Kick",          K::kick,       { "kick" },                                      true  },
        { "snare",        "Snare",         K::snare,      { "snare", "snareRim", "sidestick", "snareFlam" }, true  },
        { "hihat",        "HiHat",         K::hiHat,      { "hihatClosed", "hihatOpen", "hihatPedal" },    true  },

        { "tomHigh",      "Tom hoch",      K::rackTom,    { "tomHigh" },                                   true  },
        { "tomMid",       "Tom mittel",    K::rackTom,    { "tomMid" },                                    false },
        { "tomLow",       "Tom tief",      K::rackTom,    { "tomLow" },                                    true  },
        { "tomFloorHigh", "Standtom hoch", K::floorTom,   { "tomFloorHigh" },                              false },
        { "tomFloor",     "Standtom",      K::floorTom,   { "tomFloor" },                                  true  },

        { "crash",        "Crash",         K::cymbal,     { "crash" },                                     true  },
        { "crash2",       "Crash 2",       K::cymbal,     { "crash2" },                                    false },
        { "ride",         "Ride",          K::cymbal,     { "ride", "rideBell" },                          true  },
        { "china",        "China",         K::cymbal,     { "china" },                                     false },
        { "splash",       "Splash",        K::cymbal,     { "splash" },                                    false },

        { "clap",         "Clap",          K::percussion, { "clap" },                                      false },
        { "cowbell",      "Cowbell",       K::percussion, { "cowbell" },                                   false },
        { "tambourine",   "Tamburin",      K::percussion, { "tambourine" },                                false },
    };

    return pieces;
}

const DrumPiece* findDrumPiece (const juce::String& id)
{
    for (const auto& piece : drumPieces())
        if (id == piece.id)
            return &piece;

    return nullptr;
}

const DrumPiece* findPieceForPart (const juce::String& partId)
{
    for (const auto& piece : drumPieces())
        for (const auto* articulation : piece.articulations)
            if (partId == articulation)
                return &piece;

    return nullptr;
}

juce::StringArray standardDrumPieces()
{
    juce::StringArray ids;

    for (const auto& piece : drumPieces())
        if (piece.standard)
            ids.add (piece.id);

    return ids;
}
} // namespace sis
