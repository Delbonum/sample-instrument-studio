#include "PluginIdentityPatch.h"
#include "../Model/Text.h"

namespace sis::identity
{
namespace
{
    /** Hersteller- und Plugin-Code, wie sie in den VST3-Kennungen stehen (JucePlugin_*Code). */
    const juce::String manufacturerHex ("576B6B62");   // "Wkkb"
    const juce::String defaultCodeHex ("53697374");    // "Sist"
    const juce::String defaultName ("Sample Instrument Studio");

    juce::String toHex (const juce::String& fourCharacters)
    {
        juce::String hex;

        for (int i = 0; i < 4; ++i)
            hex += juce::String::toHexString ((int) (juce::juce_wchar) fourCharacters[i])
                       .paddedLeft ('0', 2)
                       .toUpperCase();

        return hex;
    }

    /** Die Stelle im Binary, an der ein vollständiger Block steht (Marke + 4 Zeichen + '|'). */
    std::vector<size_t> findBlocks (const juce::MemoryBlock& data)
    {
        std::vector<size_t> offsets;
        const auto* bytes = static_cast<const char*> (data.getData());
        const size_t size = data.getSize();
        const juce::String markerText (marker);
        const size_t markerLength = (size_t) markerText.length();

        if (size < markerLength + 6)
            return offsets;

        for (size_t i = 0; i + markerLength + 5 < size; ++i)
        {
            if (std::memcmp (bytes + i, markerText.toRawUTF8(), markerLength) != 0)
                continue;

            // Ein echter Datenblock hat hinter der Marke vier Zeichen und einen senkrechten Strich
            if (bytes[i + markerLength + 4] == '|')
                offsets.push_back (i);
        }

        return offsets;
    }

    juce::File findModule (const juce::File& bundle)
    {
        const auto binaryFolder = bundle.getChildFile ("Contents").getChildFile ("x86_64-win");

        if (binaryFolder.isDirectory())
        {
            const auto modules = binaryFolder.findChildFiles (juce::File::findFiles, false, "*.vst3");

            if (! modules.isEmpty())
                return modules.getFirst();
        }

        return {};
    }
} // namespace

juce::String makePluginCode (const juce::String& instrumentName)
{
    const auto hash = (juce::uint64) instrumentName.trim().toLowerCase().hashCode64();

    const char* const initials = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char* const rest = "abcdefghijklmnopqrstuvwxyz0123456789";

    juce::String code;
    code << initials[(hash >> 3) % 26]
         << rest[(hash >> 11) % 36]
         << rest[(hash >> 23) % 36]
         << rest[(hash >> 37) % 36];

    return code;
}

bool patchBinary (const juce::File& binary, const juce::String& instrumentName,
                  const juce::String& pluginCode, juce::String& error)
{
    if (pluginCode.length() != 4)
    {
        error = "Plugin-Code muss vier Zeichen haben"_u;
        return false;
    }

    const auto name = instrumentName.trim();
    const auto newBlock = juce::String (marker) + pluginCode + "|" + name;

    if (newBlock.getNumBytesAsUTF8() >= blockSize)
    {
        error = "Name ist zu lang für die Plugin-Kennung"_u;
        return false;
    }

    if (! binary.existsAsFile())
    {
        error = "Programmdatei nicht gefunden: "_u + binary.getFileName();
        return false;
    }

    auto module = binary;
    module.setReadOnly (false);

    juce::MemoryBlock data;

    if (! module.loadFileAsData (data))
    {
        error = "Programmdatei konnte nicht gelesen werden"_u;
        return false;
    }

    const auto offsets = findBlocks (data);

    if (offsets.empty())
    {
        error = "Kennungsblock im Plugin nicht gefunden"_u;
        return false;
    }

    // Nur die gefundenen Blöcke überschreiben - die Datei bleibt sonst unangetastet.
    // (Ein vollständiges Neuschreiben scheitert unter Windows gern am Virenscanner.)
    std::vector<char> replacement ((size_t) blockSize, 0);
    std::memcpy (replacement.data(), newBlock.toRawUTF8(), (size_t) newBlock.getNumBytesAsUTF8());

    juce::FileOutputStream out (module);

    if (! out.openedOk())
    {
        error = "Plugin-Datei konnte nicht geöffnet werden"_u;
        return false;
    }

    for (const auto offset : offsets)
    {
        if (! out.setPosition ((juce::int64) offset) || ! out.write (replacement.data(), replacement.size()))
        {
            error = "Kennung konnte nicht geschrieben werden"_u;
            return false;
        }
    }

    out.flush();

    if (out.getStatus().failed())
    {
        error = out.getStatus().getErrorMessage();
        return false;
    }

    return true;
}

bool patchBundle (const juce::File& bundle, const juce::String& instrumentName,
                  const juce::String& pluginCode, juce::String& error)
{
    const auto name = instrumentName.trim();

    auto module = findModule (bundle);

    if (module == juce::File())
    {
        error = "Plugin-Datei im Bundle nicht gefunden"_u;
        return false;
    }

    if (! patchBinary (module, name, pluginCode, error))
        return false;

    // Das Modul soll wie das Bundle heißen
    const auto desiredName = bundle.getFileNameWithoutExtension() + ".vst3";

    if (module.getFileName() != desiredName)
        module.moveFileTo (module.getSiblingFile (desiredName));

    // --- moduleinfo.json nachziehen ---
    const auto manifest = bundle.getChildFile ("Contents").getChildFile ("Resources")
                                .getChildFile ("moduleinfo.json");

    if (manifest.existsAsFile())
    {
        manifest.setReadOnly (false);
        auto text = manifest.loadFileAsString();

        // In den Kennungen steht der Plugin-Code direkt hinter dem Herstellercode
        text = text.replace (manufacturerHex + defaultCodeHex, manufacturerHex + toHex (pluginCode));
        text = text.replace ("\"" + defaultName + "\"", "\"" + name + "\"");

        // Direkt in die Datei schreiben statt über eine Zwischendatei: `replaceWithText`
        // legt eine temporäre Datei an und benennt sie um, und genau daran scheitert
        // unter Windows immer wieder der Virenscanner.
        juce::FileOutputStream manifestOut (manifest);

        if (! manifestOut.openedOk()
            || ! manifestOut.setPosition (0)
            || ! manifestOut.truncate().wasOk()
            || ! manifestOut.writeText (text, false, false, nullptr))
        {
            error = "moduleinfo.json konnte nicht geschrieben werden"_u;
            return false;
        }

        manifestOut.flush();

        if (manifestOut.getStatus().failed())
        {
            error = "moduleinfo.json: "_u + manifestOut.getStatus().getErrorMessage();
            return false;
        }
    }

    return true;
}
} // namespace sis::identity
