#include "StudioHandover.h"
#include "Model/Text.h"

namespace sis
{
namespace handover
{
namespace
{
    const char* const executableKey = "studioExecutable";
}

juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = JucePlugin_Name;
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = JucePlugin_Name;
    return options;
}

void rememberStudioExecutable()
{
    const auto self = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    if (! self.existsAsFile())
        return;

    juce::ApplicationProperties properties;
    properties.setStorageParameters (settingsOptions());

    if (auto* settings = properties.getUserSettings())
    {
        settings->setValue (executableKey, self.getFullPathName());
        settings->saveIfNeeded();
    }
}

juce::File findStudioExecutable()
{
    juce::ApplicationProperties properties;
    properties.setStorageParameters (settingsOptions());

    if (auto* settings = properties.getUserSettings())
    {
        const juce::File file (settings->getValue (executableKey));

        if (file.existsAsFile())
            return file;
    }

    return {};
}

juce::File transferFile()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
               .getChildFile (JucePlugin_Name)
               .getChildFile ("Übergabe aus dem Plugin.sisp"_u);
}
} // namespace handover
} // namespace sis
