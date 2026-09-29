#include "PresetLibrary.h"
#include "Text.h"
#include "XmlFile.h"

namespace sis
{
namespace
{
    const juce::Identifier presetsTag { "SisPresets" };
    const juce::Identifier presetTag  { "Preset" };
    const juce::Identifier parameterTag { "Parameter" };

    const juce::Identifier typeAttribute { "type" };
    const juce::Identifier nameAttribute { "name" };
    const juce::Identifier labelAttribute { "label" };
    const juce::Identifier valueAttribute { "value" };

    constexpr int highestEffectType = 23;   // muss zu EffectType passen
}

PresetLibrary::PresetLibrary (const juce::File& fileToUse)
    : file (fileToUse != juce::File() ? fileToUse : getDefaultFile())
{
    reload();
}

juce::File PresetLibrary::getDefaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Sample Instrument Studio")
               .getChildFile ("presets.xml");
}

void PresetLibrary::reload()
{
    presets.clear();

    if (! file.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName (presetsTag.toString()))
        return;

    for (auto* node : xml->getChildWithTagNameIterator (presetTag.toString()))
    {
        const int type = node->getIntAttribute (typeAttribute.toString(), -1);

        if (type < 0 || type > highestEffectType)
            continue;

        Preset preset;
        preset.type = (EffectType) type;
        preset.name = node->getStringAttribute (nameAttribute.toString());

        if (preset.name.isEmpty())
            continue;

        for (auto* parameter : node->getChildWithTagNameIterator (parameterTag.toString()))
            preset.parameters.push_back ({ parameter->getStringAttribute (labelAttribute.toString()),
                                           (float) parameter->getDoubleAttribute (valueAttribute.toString()) });

        presets.push_back (std::move (preset));
    }
}

void PresetLibrary::write() const
{
    juce::XmlElement root (presetsTag.toString());

    for (const auto& preset : presets)
    {
        auto* node = root.createNewChildElement (presetTag.toString());
        node->setAttribute (typeAttribute.toString(), (int) preset.type);
        node->setAttribute (nameAttribute.toString(), preset.name);

        for (const auto& parameter : preset.parameters)
        {
            auto* child = node->createNewChildElement (parameterTag.toString());
            child->setAttribute (labelAttribute.toString(), parameter.label);
            child->setAttribute (valueAttribute.toString(), parameter.value);
        }
    }

    // Unmittelbar schreiben, nicht über eine Zwischendatei - siehe XmlFile.h
    writeXmlDirectly (root, file);
}

const PresetLibrary::Preset* PresetLibrary::find (EffectType type, const juce::String& name) const
{
    for (const auto& preset : presets)
        if (preset.type == type && preset.name == name)
            return &preset;

    return nullptr;
}

juce::StringArray PresetLibrary::getNames (EffectType type) const
{
    juce::StringArray names;

    for (const auto& preset : presets)
        if (preset.type == type)
            names.add (preset.name);

    names.sort (true);
    return names;
}

bool PresetLibrary::apply (const juce::String& name, Effect& effect) const
{
    const auto* preset = find (effect.type, name);

    if (preset == nullptr)
        return false;

    // Über die Beschriftung zuordnen; was nicht vorkommt, bleibt stehen
    for (const auto& stored : preset->parameters)
        for (auto& parameter : effect.parameters)
            if (parameter.label == stored.label)
                parameter.value = juce::jlimit (0.0f, 1.0f, stored.value);

    return true;
}

bool PresetLibrary::applyFactoryDefaults (Effect& effect)
{
    // Die Voreinstellungen stehen in den Fabriken - keine zweite Liste daneben
    for (const auto& original : Effect::builtIn())
    {
        if (original.type != effect.type)
            continue;

        effect.parameters = original.parameters;
        return true;
    }

    return false;
}

void PresetLibrary::save (const juce::String& name, const Effect& effect)
{
    const auto trimmed = name.trim();

    if (trimmed.isEmpty() || effect.parameters.empty())
        return;

    for (auto& preset : presets)
    {
        if (preset.type == effect.type && preset.name == trimmed)
        {
            preset.parameters = effect.parameters;
            write();
            return;
        }
    }

    presets.push_back ({ effect.type, trimmed, effect.parameters });
    write();
}

void PresetLibrary::remove (EffectType type, const juce::String& name)
{
    const auto before = presets.size();

    presets.erase (std::remove_if (presets.begin(), presets.end(),
                                   [type, &name] (const Preset& preset)
                                   {
                                       return preset.type == type && preset.name == name;
                                   }),
                   presets.end());

    if (presets.size() != before)
        write();
}
} // namespace sis
