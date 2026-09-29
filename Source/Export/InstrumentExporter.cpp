#include "InstrumentExporter.h"
#include "../Model/Instrument.h"
#include "../Model/XmlFile.h"
#include "PluginIdentityPatch.h"

namespace sis
{
namespace
{
    constexpr const char* samplesFolderName = "Samples";
}

InstrumentExporter::InstrumentExporter() : juce::Thread ("SIS Export")
{
}

InstrumentExporter::~InstrumentExporter()
{
    cancelAndWait();
}

void InstrumentExporter::cancelAndWait()
{
    signalThreadShouldExit();
    stopThread (4000);
}

juce::int64 InstrumentExporter::estimateBytes (const ExportRequest& request)
{
    juce::int64 sampleBytes = 0;

    for (const auto& file : request.samples)
        sampleBytes += file.getSize();

    juce::int64 bytes = 64 * 1024;   // Projektdatei

    if (request.exportProjectFolder && request.copySamples)
        bytes += sampleBytes;

    if (request.exportPlugin && request.pluginTemplate.isDirectory())
    {
        // Im Plugin liegen die Samples noch einmal, dazu das Plugin selbst
        bytes += sampleBytes;

        for (const auto& file : request.pluginTemplate.findChildFiles (juce::File::findFiles, true))
            bytes += file.getSize();
    }

    if (request.exportStandalone && request.standaloneTemplate.existsAsFile())
    {
        // Auch neben der App liegen die Samples noch einmal
        bytes += sampleBytes + request.standaloneTemplate.getSize();
    }

    return bytes;
}

juce::File InstrumentExporter::findPluginTemplate()
{
    const juce::String bundleName ("Sample Instrument Studio.vst3");
    const auto exeFolder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

    const juce::File candidates[] = {
        exeFolder.getChildFile (bundleName),                                     // neben der App
        exeFolder.getSiblingFile ("VST3").getChildFile (bundleName),             // Build-Ordner
        exeFolder.getParentDirectory().getChildFile ("VST3").getChildFile (bundleName),
        juce::File ("C:/Program Files/Common Files/VST3").getChildFile (bundleName)
    };

    for (const auto& candidate : candidates)
        if (candidate.isDirectory())
            return candidate;

    return {};
}

juce::File InstrumentExporter::findStandaloneTemplate()
{
    const auto running = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    // Läuft das Studio selbst als App, ist es seine eigene Vorlage. Als Plugin im Host
    // liegt daneben kein Programm, dann wird im Build-Ordner nachgesehen.
    if (running.existsAsFile() && running.hasFileExtension ("exe"))
        return running;

    const auto exeFolder = running.getParentDirectory();
    const juce::String appName ("Sample Instrument Studio.exe");

    const juce::File candidates[] = {
        exeFolder.getChildFile (appName),
        exeFolder.getSiblingFile ("Standalone").getChildFile (appName),
        exeFolder.getParentDirectory().getParentDirectory().getChildFile ("Standalone").getChildFile (appName)
    };

    for (const auto& candidate : candidates)
        if (candidate.existsAsFile())
            return candidate;

    return {};
}

namespace
{
    /** Schreibt Samples und Projektdatei in einen Ordner; die Pfade werden relativ abgelegt. */
    bool writeInstrumentFolder (const juce::File& folder, juce::ValueTree projectState,
                                const juce::Array<juce::File>& samples, bool copySamples,
                                const juce::String& projectFileName, ExportOutcome& outcome,
                                juce::File& writtenProject)
    {
        auto instrument = projectState.getChildWithName ("Instrument");

        if (copySamples && ! samples.isEmpty())
        {
            const auto samplesFolder = folder.getChildFile (samplesFolderName);

            if (! samplesFolder.createDirectory())
            {
                outcome.message = "Sample-Ordner konnte nicht angelegt werden"_u;
                return false;
            }

            for (const auto& source : samples)
            {
                const auto destination = samplesFolder.getChildFile (source.getFileName());
                const bool alreadyThere = destination.existsAsFile() && destination.getSize() == source.getSize();

                if (! alreadyThere && ! source.copyFileTo (destination))
                {
                    outcome.message = "Datei konnte nicht kopiert werden: "_u + source.getFileName();
                    return false;
                }

                outcome.totalBytes += destination.getSize();

                for (auto node : instrument)
                    if (node.hasType ("Sample") && juce::File (node.getProperty ("path").toString()) == source)
                        node.setProperty ("path", destination.getFullPathName(), nullptr);
            }

            makeSamplePathsRelative (instrument, folder);
        }

        writtenProject = folder.getChildFile (projectFileName);
        auto xml = projectState.createXml();

        if (xml == nullptr || ! writeXmlDirectly (*xml, writtenProject))
        {
            outcome.message = "Projektdatei konnte nicht geschrieben werden"_u;
            return false;
        }

        outcome.totalBytes += writtenProject.getSize();
        return true;
    }
}

bool InstrumentExporter::start (ExportRequest newRequest, std::function<void (ExportOutcome)> onFinished)
{
    if (isThreadRunning())
        return false;

    request = std::move (newRequest);
    finished = std::move (onFinished);
    progress.store (0.0, std::memory_order_relaxed);
    startThread();
    return true;
}

void InstrumentExporter::run()
{
    auto outcome = performExport (request, progress, [this] { return threadShouldExit(); });

    if (finished != nullptr)
        juce::MessageManager::callAsync ([callback = finished, outcome] { callback (outcome); });
}

ExportOutcome InstrumentExporter::performExport (const ExportRequest& request, std::atomic<double>& progress,
                                                 const std::function<bool()>& shouldCancel)
{
    ExportOutcome outcome;

    if (! request.targetFolder.createDirectory())
    {
        outcome.message = "Ordner konnte nicht angelegt werden: "_u + request.targetFolder.getFullPathName();
        return outcome;
    }

    const auto legalName = juce::File::createLegalFileName (request.instrumentName);
    const auto cancelled = [&shouldCancel] { return shouldCancel != nullptr && shouldCancel(); };

    // --- Instrument-Ordner: Projektdatei und Samples ---
    if (request.exportProjectFolder)
    {
        if (! writeInstrumentFolder (request.targetFolder, request.projectState.createCopy(), request.samples,
                                     request.copySamples, legalName + ".sisp", outcome, outcome.projectFile))
            return outcome;

        outcome.copiedSamples = request.copySamples ? request.samples.size() : 0;
    }

    const bool moreToDo = request.exportPlugin || request.exportStandalone;
    progress.store (moreToDo ? 0.3 : 1.0, std::memory_order_relaxed);

    if (cancelled())
    {
        outcome.message = "Export abgebrochen";
        return outcome;
    }

    // --- VST3: Vorlage kopieren und das Instrument hineinlegen ---
    if (request.exportPlugin)
    {
        if (! request.pluginTemplate.isDirectory())
        {
            outcome.message = "Plugin-Vorlage nicht gefunden"_u;
            return outcome;
        }

        const auto bundle = request.targetFolder.getChildFile (legalName + ".vst3");
        bundle.deleteRecursively();

        if (! request.pluginTemplate.copyDirectoryTo (bundle))
        {
            outcome.message = "Plugin konnte nicht kopiert werden"_u;
            return outcome;
        }

        // Kopien aus einem Programmordner koennen schreibgeschuetzt sein
        bundle.setReadOnly (false, true);

        // Reste aus dem Bauen entfernen
        for (const auto& leftover : bundle.findChildFiles (juce::File::findFiles, true, "*.ilk;*.pdb;*.exp;*.lib"))
            leftover.deleteFile();

        // Eigener Name und eigene Kennung, damit mehrere Instrumente nebeneinander laufen
        juce::String patchError;

        if (! identity::patchBundle (bundle, request.instrumentName,
                                     identity::makePluginCode (request.instrumentName), patchError))
        {
            outcome.message = "Plugin-Kennung: "_u + patchError;
            return outcome;
        }

        progress.store (0.7, std::memory_order_relaxed);

        const auto resources = bundle.getChildFile ("Contents").getChildFile ("Resources");

        if (! resources.createDirectory())
        {
            outcome.message = "Ressourcen-Ordner im Plugin fehlt"_u;
            return outcome;
        }

        juce::File bundledProject;

        if (! writeInstrumentFolder (resources, request.projectState.createCopy(), request.samples,
                                     true, "Instrument.sisp", outcome, bundledProject))
            return outcome;

        outcome.pluginBundle = bundle;
        outcome.pluginCode = identity::makePluginCode (request.instrumentName);

        if (! request.exportProjectFolder)
            outcome.copiedSamples = request.samples.size();
    }

    // --- Standalone: die App kopieren und das Instrument daneben legen ---
    if (request.exportStandalone)
    {
        if (! request.standaloneTemplate.existsAsFile())
        {
            outcome.message = "Programmvorlage nicht gefunden"_u;
            return outcome;
        }

        // Ein eigener Ordner, damit sich das Ganze am Stück weitergeben lässt:
        // die App findet ihr Instrument daneben, nicht an einem festen Ort.
        const auto appFolder = request.targetFolder.getChildFile (legalName + " (App)");
        appFolder.deleteRecursively();

        if (! appFolder.createDirectory())
        {
            outcome.message = "App-Ordner konnte nicht angelegt werden"_u;
            return outcome;
        }

        const auto app = appFolder.getChildFile (legalName + ".exe");

        if (! request.standaloneTemplate.copyFileTo (app))
        {
            outcome.message = "Programmdatei konnte nicht kopiert werden"_u;
            return outcome;
        }

        app.setReadOnly (false);

        juce::String appError;

        if (! identity::patchBinary (app, request.instrumentName,
                                     identity::makePluginCode (request.instrumentName), appError))
        {
            outcome.message = "Programm-Kennung: "_u + appError;
            return outcome;
        }

        juce::File bundledProject;

        if (! writeInstrumentFolder (appFolder, request.projectState.createCopy(), request.samples,
                                     true, "Instrument.sisp", outcome, bundledProject))
            return outcome;

        outcome.standaloneApp = app;

        if (! request.exportProjectFolder && ! request.exportPlugin)
            outcome.copiedSamples = request.samples.size();
    }

    outcome.succeeded = true;
    juce::StringArray parts;

    if (outcome.projectFile != juce::File())
        parts.add ("Projektdatei");

    if (outcome.pluginBundle != juce::File())
        parts.add ("VST3-Instrument");

    if (outcome.standaloneApp != juce::File())
        parts.add ("Standalone-App");

    if (outcome.copiedSamples > 0)
        parts.add (juce::String (outcome.copiedSamples) + (outcome.copiedSamples == 1 ? " Sample" : " Samples"));

    outcome.message = parts.joinIntoString (" · "_u) + " geschrieben"_u;
    progress.store (1.0, std::memory_order_relaxed);
    return outcome;
}

} // namespace sis
