#include "PluginLibrary.h"
#include "../Model/Text.h"
#include "../Model/XmlFile.h"

namespace sis
{

//==============================================================================
/** Durchsucht die Plugin-Ordner in einem eigenen Thread. */
class PluginLibrary::ScanJob final : private juce::Thread
{
public:
    ScanJob (juce::AudioPluginFormat& format, juce::KnownPluginList& list, const juce::FileSearchPath& paths,
             std::function<void (int)> onFinished)
        : juce::Thread ("SIS Plugin-Suche"),
          scanner (list, format, paths, true, juce::File(), false),
          finished (std::move (onFinished))
    {
        startThread();
    }

    ~ScanJob() override
    {
        signalThreadShouldExit();
        stopThread (8000);
    }

    bool isRunning() const { return isThreadRunning(); }

    juce::String getCurrentName() const
    {
        const juce::ScopedLock lock (nameLock);
        return currentName;
    }

private:
    void run() override
    {
        int found = 0;

        for (;;)
        {
            if (threadShouldExit())
                break;

            juce::String nextPlugin;

            if (! scanner.scanNextFile (true, nextPlugin))
                break;

            {
                const juce::ScopedLock lock (nameLock);
                currentName = nextPlugin;
            }

            ++found;
        }

        if (finished != nullptr)
            juce::MessageManager::callAsync ([callback = finished, found] { callback (found); });
    }

    juce::PluginDirectoryScanner scanner;
    std::function<void (int)> finished;
    juce::CriticalSection nameLock;
    juce::String currentName;
};

//==============================================================================
PluginLibrary::PluginLibrary()
{
    formatManager.addFormat (new juce::VST3PluginFormat());
    loadList();
}

PluginLibrary::~PluginLibrary()
{
    scanJob.reset();
}

juce::File PluginLibrary::getListFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Sample Instrument Studio")
               .getChildFile ("plugins.xml");
}

juce::FileSearchPath PluginLibrary::getSearchPaths() const
{
    if (formatManager.getNumFormats() > 0)
        return formatManager.getFormat (0)->getDefaultLocationsToSearch();

    return {};
}

void PluginLibrary::loadList()
{
    const auto file = getListFile();

    if (! file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse (file))
        knownPlugins.recreateFromXml (*xml);
}

void PluginLibrary::saveList() const
{
    const auto file = getListFile();

    // Unmittelbar schreiben, nicht über eine Zwischendatei - siehe XmlFile.h
    if (auto xml = knownPlugins.createXml())
        writeXmlDirectly (*xml, file);
}

juce::Array<juce::PluginDescription> PluginLibrary::getEffects() const
{
    juce::Array<juce::PluginDescription> effects;

    for (const auto& description : knownPlugins.getTypes())
        if (! description.isInstrument)
            effects.add (description);

    std::sort (effects.begin(), effects.end(),
               [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
               {
                   return a.name.compareIgnoreCase (b.name) < 0;
               });

    return effects;
}

juce::StringArray PluginLibrary::addPluginFile (const juce::File& file, juce::String& error)
{
    juce::StringArray identifiers;

    if (formatManager.getNumFormats() == 0)
    {
        error = "Kein Plugin-Format verfügbar."_u;
        return identifiers;
    }

    if (! file.exists())
    {
        error = "Die Datei gibt es nicht: "_u + file.getFullPathName();
        return identifiers;
    }

    juce::OwnedArray<juce::PluginDescription> found;
    formatManager.getFormat (0)->findAllTypesForFile (found, file.getFullPathName());

    if (found.isEmpty())
    {
        error = "Kein VST3-Plugin in: "_u + file.getFileName();
        return identifiers;
    }

    for (const auto* description : found)
    {
        knownPlugins.addType (*description);
        identifiers.add (description->createIdentifierString());
    }

    saveList();
    sendChangeMessage();
    return identifiers;
}

bool PluginLibrary::isScanning() const
{
    return scanJob != nullptr && scanJob->isRunning();
}

juce::String PluginLibrary::getCurrentScanName() const
{
    return scanJob != nullptr ? scanJob->getCurrentName() : juce::String();
}

void PluginLibrary::startScan (std::function<void (int)> onFinished)
{
    if (isScanning() || formatManager.getNumFormats() == 0)
        return;

    scanJob = std::make_unique<ScanJob> (*formatManager.getFormat (0), knownPlugins, getSearchPaths(),
                                         [this, onFinished] (int found)
    {
        saveList();
        sendChangeMessage();

        if (onFinished != nullptr)
            onFinished (found);
    });
}

HostedPlugin::Ptr PluginLibrary::createInstance (const juce::String& identifier, double sampleRate,
                                                 int blockSize, juce::String& error)
{
    for (const auto& description : knownPlugins.getTypes())
    {
        if (description.createIdentifierString() != identifier)
            continue;

        auto instance = formatManager.createPluginInstance (description, sampleRate, blockSize, error);

        if (instance == nullptr)
            return nullptr;

        HostedPlugin::Ptr hosted (new HostedPlugin (std::move (instance), identifier));
        hosted->prepare (sampleRate, blockSize);
        return hosted;
    }

    error = "Plugin nicht in der Liste: "_u + identifier;
    return nullptr;
}
} // namespace sis
