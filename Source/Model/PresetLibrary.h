#pragma once

#include <JuceHeader.h>

#include "Instrument.h"

namespace sis
{
/** Gespeicherte Reglerstellungen je Effektart.

    Ein Preset hält **nur die Reglerwerte**, nicht den Namen des Effekts und nicht, ob er
    eingeschaltet ist. Beides gehört zur einzelnen Spur, nicht zum Klang.

    Zugeordnet werden die Werte über die **Beschriftung** des Reglers, nicht über seine
    Nummer: so überlebt ein Preset, wenn bei einem Effekt später ein Regler dazwischen
    kommt. Was nicht zugeordnet werden kann, bleibt einfach stehen.

    Am Effekt selbst wird **nicht vermerkt**, welches Preset geladen wurde. Sobald man
    einen Regler anfasst, wäre der Name ohnehin falsch, und ein Name, der lügt, ist
    schlechter als keiner.

    Ausschließlich vom Message-Thread benutzen. */
class PresetLibrary
{
public:
    /** Ohne Pfad wird die Datei neben den Programmeinstellungen benutzt.
        Die Angabe dient den Tests, damit sie nicht in die echten Presets schreiben. */
    explicit PresetLibrary (const juce::File& fileToUse = {});

    /** Namen der gespeicherten Presets einer Effektart, alphabetisch. */
    juce::StringArray getNames (EffectType) const;

    /** Legt die Werte eines Presets auf den Effekt. Gibt false zurück, wenn es das
        Preset nicht gibt. */
    bool apply (const juce::String& name, Effect&) const;

    /** Setzt den Effekt auf die Werte zurück, mit denen er eingefügt wird. */
    static bool applyFactoryDefaults (Effect&);

    /** Sichert die aktuellen Werte. Ein vorhandener Name wird überschrieben. */
    void save (const juce::String& name, const Effect&);

    void remove (EffectType, const juce::String& name);

    /** Wo die Presets liegen. */
    static juce::File getDefaultFile();

    /** Für Tests: erneut von der Platte lesen. */
    void reload();

private:
    struct Preset
    {
        EffectType type = EffectType::generic;
        juce::String name;
        std::vector<EffectParameter> parameters;
    };

    void write() const;
    const Preset* find (EffectType, const juce::String& name) const;

    juce::File file;
    std::vector<Preset> presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetLibrary)
};
} // namespace sis
