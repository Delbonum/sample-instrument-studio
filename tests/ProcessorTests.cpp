// Prüft die Audio-Kette des echten Prozessors: Sample importieren, Spur anlegen,
// Taste drücken, Block rendern - also genau das, was die App im Betrieb tut.

#include <JuceHeader.h>

#include "../Source/Audio/PluginLibrary.h"
#include "../Source/Export/InstrumentExporter.h"
#include "../Source/Export/PluginIdentityPatch.h"
#include "../Source/PluginProcessor.h"
#include "../Source/UI/Assets.h"

#include <cstdio>
#include <cstring>

namespace
{
int checks = 0;
int failures = 0;

void section (const char* name)
{
    std::printf ("[ %s ]\n", name);
}

void expect (const juce::String& what, bool condition)
{
    ++checks;

    if (! condition)
    {
        std::printf ("FEHLER  %s\n", what.toRawUTF8());
        ++failures;
    }
}

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;

/** Schreibt eine kurze WAV-Datei mit Gleichanteil 0,5. */
juce::File writeTestWav()
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("sis_processor_test.wav");
    file.deleteFile();

    juce::AudioBuffer<float> source (1, (int) sampleRate);
    for (int i = 0; i < source.getNumSamples(); ++i)
        source.setSample (0, i, 0.5f);

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sampleRate, 1, 16, {}, 0));

    if (writer != nullptr)
    {
        stream.release();
        writer->writeFromAudioSampleBuffer (source, 0, source.getNumSamples());
    }

    return file;
}

/** Rendert einen Block und gibt den Spitzenpegel zurück. */
float renderBlock (sis::StudioProcessor& processor)
{
    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);
    return buffer.getMagnitude (0, buffer.getNumSamples());
}
} // namespace

int main()
{
    // Ungepuffert, damit bei einem Absturz sichtbar bleibt, wie weit der Test kam
    std::setvbuf (stdout, nullptr, _IONBF, 0);

    juce::ScopedJuceInitialiser_GUI juceInit;

    auto processorOwner = std::make_unique<sis::StudioProcessor>();   // auf dem Heap: mehrere Prozessoren sprengen sonst den Stack
    auto& processor = *processorOwner;
    processor.prepareToPlay (sampleRate, blockSize);

    expect ("Startet mit Beispielzonen", ! processor.getModel().zones.empty());
    expect ("Ohne Audiodateien ist keine Zone spielbar",
            ! processor.zoneHasAudio (processor.getModel().zones.front()));
    expect ("Ohne Noten bleibt es still", renderBlock (processor) < 1.0e-6f);

    // --- Sample importieren, wie es Drag & Drop oder Strg+I tun --------------------
    section ("Sample importieren, wie es Drag & Drop oder Strg+I tun");
    const auto file = writeTestWav();
    juce::Array<juce::File> files;
    files.add (file);

    const int imported = processor.getModel().importSamples (files, processor.getFormatManager());
    expect ("Import meldet eine Datei", imported == 1);

    // --- Spur in der ersten Zone anlegen, wie der Doppelklick im Browser ----------
    section ("Spur in der ersten Zone anlegen, wie der Doppelklick im Browser");
    auto& zone = processor.getModel().zones.front();
    sis::Track track;
    track.name = "Test";
    { sis::Clip piece; piece.sample = file.getFileName(); track.clips.push_back (piece); }
    track.gain = 1.0f;
    track.loop = sis::LoopMode::sustainLoop;
    zone.tracks.push_back (track);
    processor.getModel().selectZone (zone.id);
    // Im Betrieb meldet das Modell die Änderung asynchron; hier direkt zustellen,
    // damit der Prozessor den Plan ohne laufende Nachrichtenschleife neu baut.
    processor.getModel().sendSynchronousChangeMessage();

    expect ("Zone hat jetzt Audiodaten", processor.zoneHasAudio (zone));
    expect ("Audiodaten liegen im Speicher", processor.getSampleMemoryBytes() > 0);

    // --- Taste auf der Bildschirm-Klaviatur --------------------------------------
    section ("Taste auf der Bildschirm-Klaviatur");
    // Die Hüllkurve des Beispiels braucht 120 ms bis zum vollen Pegel, also über
    // mehrere Blöcke messen statt nur über den ersten.
    processor.getKeyboardState().noteOn (1, zone.rootNote, 0.9f);

    float pressed = 0.0f;
    for (int i = 0; i < 60; ++i)
        pressed = juce::jmax (pressed, renderBlock (processor));

    expect ("Tastendruck erzeugt Signal", pressed > 0.1f);
    expect ("Stimme läuft", processor.getActiveVoiceCount() == 1);

    processor.getKeyboardState().noteOff (1, zone.rootNote, 0.0f);

    // Release dauert hier 2,2 s
    const int releaseBlocks = (int) (4.0 * sampleRate / blockSize);
    for (int i = 0; i < releaseBlocks && processor.getActiveVoiceCount() > 0; ++i)
        renderBlock (processor);

    expect ("Loslassen beendet die Stimme", processor.getActiveVoiceCount() == 0);

    // --- Master-Pegel wirkt -------------------------------------------------------
    section ("Master-Pegel wirkt");
    processor.getMasterGain().setValueNotifyingHost (0.0f);
    processor.getKeyboardState().noteOn (1, zone.rootNote, 0.9f);

    float withMasterClosed = 0.0f;
    for (int i = 0; i < 60; ++i)
    {
        const float level = renderBlock (processor);
        if (i > 1)   // erster Block enthält noch die Rampe des Master-Pegels
            withMasterClosed = juce::jmax (withMasterClosed, level);
    }

    expect ("Master ganz zu macht still", withMasterClosed < 1.0e-4f);
    processor.getKeyboardState().noteOff (1, zone.rootNote, 0.0f);

    // --- Verlauf: Aenderungen zuruecknehmen und wiederholen ------------------------
    section ("Verlauf: Aenderungen zuruecknehmen und wiederholen");
    {
        auto undoTestOwner = std::make_unique<sis::StudioProcessor>();   // auf dem Heap: mehrere Prozessoren sprengen sonst den Stack
        auto& undoTest = *undoTestOwner;
        undoTest.prepareToPlay (sampleRate, blockSize);
        undoTest.getModel().importSamples (files, undoTest.getFormatManager());

        auto& history = undoTest.getHistory();
        auto& undoModel = undoTest.getModel();

        // Ohne laufende Nachrichtenschleife beides von Hand: zustellen und festhalten
        const auto commit = [&undoModel, &history]
        {
            undoModel.sendSynchronousChangeMessage();
            history.flush();
        };

        expect ("Am Anfang gibt es nichts zurueckzunehmen", ! history.canUndo());
        expect ("Und nichts zu wiederholen", ! history.canRedo());

        const int zonesAtStart = (int) undoModel.zones.size();

        // Eine Spur anlegen
        history.nameNextStep ("Spur angelegt");
        auto& firstZone = undoModel.zones.front();
        const int tracksAtStart = (int) firstZone.tracks.size();

        sis::Track newTrack;
        newTrack.name = "Zum Zuruecknehmen";
        { sis::Clip piece; piece.sample = file.getFileName(); newTrack.clips.push_back (piece); }
        firstZone.tracks.push_back (newTrack);
        commit();

        expect ("Der Schritt steht im Verlauf", history.canUndo());
        expect ("Er traegt seinen Namen", history.getUndoName() == "Spur angelegt");
        expect ("Die Spur ist da", (int) undoModel.zones.front().tracks.size() == tracksAtStart + 1);

        // Zuruecknehmen
        expect ("Zuruecknehmen gelingt", history.undo());
        expect ("Die Spur ist wieder weg", (int) undoModel.zones.front().tracks.size() == tracksAtStart);
        expect ("Jetzt laesst sich wiederherstellen", history.canRedo());
        expect ("Zurueck am Anfang", ! history.canUndo());
        expect ("Der Name bleibt am Schritt", history.getRedoName() == "Spur angelegt");

        // Nach dem Zuruecknehmen meldet das Modell selbst eine Aenderung. Liefe der
        // Schnappschuss nicht verlustfrei hin und zurueck, entstuende daraus ein
        // Geisterschritt - und der Wiederherstellen-Zweig waere verloren.
        const int stepsAfterUndo = history.getNumSteps();
        commit();
        expect ("Zuruecknehmen erzeugt keinen Geisterschritt", history.getNumSteps() == stepsAfterUndo);
        expect ("Wiederherstellen ist noch moeglich", history.canRedo());

        // Wiederherstellen
        expect ("Wiederherstellen gelingt", history.redo());
        expect ("Die Spur ist zurueck", (int) undoModel.zones.front().tracks.size() == tracksAtStart + 1);
        expect ("Die Spur traegt ihren Namen",
                undoModel.zones.front().tracks.back().name == "Zum Zuruecknehmen");

        // Bloss etwas auswaehlen ist kein Schritt
        const int stepsBefore = history.getNumSteps();
        auto& addedZone = undoModel.addZone();          // das schon
        commit();
        expect ("Eine neue Zone ist ein Schritt", history.getNumSteps() == stepsBefore + 1);
        expect ("Ohne Namen heisst sie Aenderung", history.getUndoName() == juce::String::fromUTF8 ("Änderung"));
        expect ("Die Zone ist da", (int) undoModel.zones.size() == zonesAtStart + 1);

        const int stepsAfterZone = history.getNumSteps();
        undoModel.selectZone (undoModel.zones.front().id);
        commit();
        expect ("Auswaehlen allein erzeugt keinen Schritt", history.getNumSteps() == stepsAfterZone);

        undoModel.selectZone (addedZone.id);
        commit();
        expect ("Auch ein zweites Mal nicht", history.getNumSteps() == stepsAfterZone);

        // Aufeinanderfolgende Aenderungen desselben Namens werden zusammengefasst
        const int stepsBeforeDrag = history.getNumSteps();

        for (int i = 0; i < 20; ++i)
        {
            history.nameNextStep ("Pegel geaendert");
            undoModel.zones.front().tracks.back().gain = 0.1f + 0.02f * (float) i;
            undoModel.sendSynchronousChangeMessage();   // ohne flush: wie ein Reglerzug
        }

        history.flush();
        expect ("Ein Reglerzug bleibt ein Schritt", history.getNumSteps() == stepsBeforeDrag + 1);
        expect ("Mit seinem Namen", history.getUndoName() == "Pegel geaendert");

        // Eine neue Aenderung nach dem Zuruecknehmen verwirft den Wiederherstellen-Zweig
        expect ("Noch einmal zurueck", history.undo());
        expect ("Es gaebe etwas zu wiederholen", history.canRedo());

        history.nameNextStep ("Etwas anderes");
        undoModel.zones.front().name = "Anders";
        commit();

        expect ("Der alte Zweig ist verworfen", ! history.canRedo());
        expect ("Der neue Schritt steht da", history.getUndoName() == "Etwas anderes");

        // Ein geladenes Instrument setzt den Verlauf zurueck
        history.reset();
        expect ("Nach dem Zuruecksetzen ist der Verlauf leer", ! history.canUndo() && ! history.canRedo());
    }

    // --- Export: Projektdatei und Samples in einen Ordner schreiben ----------------
    section ("Export: Projektdatei und Samples in einen Ordner schreiben");
    {
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("sis_export_test");
        folder.deleteRecursively();

        sis::ExportRequest request;
        request.targetFolder = folder;
        request.instrumentName = "Export-Test";
        request.projectState = processor.createProjectState();
        request.samples.add (file);
        request.copySamples = true;

        std::atomic<double> progress { 0.0 };
        const auto outcome = sis::InstrumentExporter::performExport (request, progress, nullptr);

        expect ("Export war erfolgreich", outcome.succeeded);
        expect ("Fortschritt steht am Ende auf 100 %", progress.load() >= 1.0);
        expect ("Projektdatei liegt im Zielordner", outcome.projectFile.existsAsFile());
        expect ("Sample wurde kopiert",
                folder.getChildFile ("Samples").getChildFile (file.getFileName()).existsAsFile());
        expect ("Ein Sample gezaehlt", outcome.copiedSamples == 1);

        // Der exportierte Ordner muss sich auch nach einem Umzug laden lassen -
        // deshalb stehen die Sample-Pfade relativ in der Projektdatei.
        const auto moved = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("sis_export_test_verschoben");
        moved.deleteRecursively();

        /* Gerade geschriebene Dateien haelt der Virenscanner kurz fest, dann scheitert
           das Umbenennen mit einer Zugriffsverletzung. Genau daran sind schon der
           DLL-Patch und die moduleinfo.json haengengeblieben - hier wird es einfach
           ein paar Mal erneut versucht. */
        bool movedOk = false;

        for (int attempt = 0; attempt < 20 && ! movedOk; ++attempt)
        {
            movedOk = folder.moveFileTo (moved);

            if (! movedOk)
                juce::Thread::sleep (100);
        }

        expect ("Ordner laesst sich verschieben", movedOk);

        const auto movedProject = moved.getChildFile (outcome.projectFile.getFileName());

        if (auto xml = juce::XmlDocument::parse (movedProject))
        {
            auto loadedOwner = std::make_unique<sis::StudioProcessor>();   // auf dem Heap: mehrere Prozessoren sprengen sonst den Stack
            auto& loaded = *loadedOwner;
            expect ("Exportiertes Projekt laedt",
                    loaded.loadProjectState (juce::ValueTree::fromXml (*xml), moved));
            loaded.getModel().sendSynchronousChangeMessage();

            const auto* loadedZone = loaded.getModel().getSelectedZone();
            expect ("Zonen sind erhalten",
                    loaded.getModel().zones.size() == processor.getModel().zones.size());
            expect ("Spur ist erhalten", loadedZone != nullptr && ! loadedZone->tracks.empty());
            expect ("Sample wird am neuen Ort gefunden",
                    loadedZone != nullptr && loaded.zoneHasAudio (*loadedZone));
        }
        else
        {
            expect ("Exportiertes Projekt laesst sich lesen", false);
        }

        moved.deleteRecursively();
    }

    // --- Export als VST3 und Gegenprobe im Host ------------------------------------
    section ("Export als VST3 und Gegenprobe im Host");
    {
        // Der Master steht aus dem vorigen Test noch auf null - der Export wuerde ihn mitnehmen
        processor.getMasterGain().setValueNotifyingHost (0.74f);

        const juce::File templateBundle (SIS_VST3_TEMPLATE);
        expect ("Plugin-Vorlage ist gebaut", templateBundle.isDirectory());

        if (templateBundle.isDirectory())
        {
            const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getChildFile ("sis_vst3_export");
            folder.deleteRecursively();

            sis::ExportRequest request;
            request.targetFolder = folder;
            request.instrumentName = "Plugin-Test";
            request.projectState = processor.createProjectState();
            request.samples.add (file);
            request.copySamples = true;
            request.exportProjectFolder = false;
            request.exportPlugin = true;
            request.pluginTemplate = templateBundle;

            std::atomic<double> progress { 0.0 };
            const auto outcome = sis::InstrumentExporter::performExport (request, progress, nullptr);

            expect ("VST3-Export war erfolgreich", outcome.succeeded);
            expect ("Bundle wurde geschrieben", outcome.pluginBundle.isDirectory());

            const auto bundled = outcome.pluginBundle.getChildFile ("Contents")
                                                     .getChildFile ("Resources")
                                                     .getChildFile ("Instrument.sisp");
            expect ("Instrument liegt im Bundle", bundled.existsAsFile());
            expect ("Sample liegt im Bundle", outcome.pluginBundle.getChildFile ("Contents")
                                                                  .getChildFile ("Resources")
                                                                  .getChildFile ("Samples")
                                                                  .getChildFile (file.getFileName())
                                                                  .existsAsFile());

            // Jetzt wie eine DAW: das exportierte Plugin laden und spielen lassen
            juce::VST3PluginFormat format;
            juce::OwnedArray<juce::PluginDescription> found;
            format.findAllTypesForFile (found, outcome.pluginBundle.getFullPathName());

            expect ("Host findet das exportierte Plugin", ! found.isEmpty());

            if (! found.isEmpty())
            {
                juce::String error;
                auto instance = format.createInstanceFromDescription (*found[0], sampleRate, blockSize, error);

                expect ("Exportiertes Plugin startet: " + error, instance != nullptr);

                if (instance != nullptr)
                {
                    instance->prepareToPlay (sampleRate, blockSize);

                    juce::AudioBuffer<float> buffer (2, blockSize);
                    float peak = 0.0f;

                    for (int i = 0; i < 80; ++i)
                    {
                        buffer.clear();
                        juce::MidiBuffer midi;

                        if (i == 0)
                            midi.addEvent (juce::MidiMessage::noteOn (1, zone.rootNote, 0.9f), 0);

                        instance->processBlock (buffer, midi);
                        peak = juce::jmax (peak, buffer.getMagnitude (0, buffer.getNumSamples()));
                    }

                    expect ("Exportiertes Plugin klingt im Host", peak > 0.05f);
                    instance->releaseResources();
                }
            }

            folder.deleteRecursively();
        }
    }

    // --- Export als eigenstaendige App ---------------------------------------------
    section ("Export als eigenstaendige App");
    {
        const juce::File appTemplate (SIS_STANDALONE_TEMPLATE);
        expect ("Die Programmvorlage ist gebaut", appTemplate.existsAsFile());

        if (appTemplate.existsAsFile())
        {
            processor.getMasterGain().setValueNotifyingHost (0.74f);

            const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getChildFile ("sis_app_export");
            folder.deleteRecursively();

            sis::ExportRequest request;
            request.targetFolder = folder;
            request.instrumentName = "Nachtbass";
            request.projectState = processor.createProjectState();
            request.samples.add (file);
            request.copySamples = true;
            request.exportProjectFolder = false;
            request.exportPlugin = false;
            request.exportStandalone = true;
            request.standaloneTemplate = appTemplate;

            std::atomic<double> progress { 0.0 };
            const auto outcome = sis::InstrumentExporter::performExport (request, progress, nullptr);

            expect ("Der App-Export gelingt: " + outcome.message, outcome.succeeded);

            if (outcome.succeeded)
            {
                const auto app = outcome.standaloneApp;
                expect ("Das Programm liegt da", app.existsAsFile());
                expect ("Es traegt den Namen des Instruments", app.getFileNameWithoutExtension() == "Nachtbass");
                expect ("Es steht in einem eigenen Ordner",
                        app.getParentDirectory().getFileName() == "Nachtbass (App)");

                // Alles, was die App zum Spielen braucht, liegt daneben
                const auto appFolder = app.getParentDirectory();
                const auto bundled = appFolder.getChildFile ("Instrument.sisp");
                expect ("Das Instrument liegt daneben", bundled.existsAsFile());
                expect ("Die Samples liegen daneben",
                        appFolder.getChildFile ("Samples").getChildFile (file.getFileName()).existsAsFile());

                // Der Kennungsblock im Programm traegt jetzt den neuen Namen
                juce::MemoryBlock binary;
                expect ("Das Programm laesst sich lesen", app.loadFileAsData (binary));

                /* Byteweise suchen, nicht ueber juce::String: eine Programmdatei ist voller
                   Nullbytes und ungueltiger UTF-8-Folgen, daran bricht jede Textsuche ab. */
                const auto containsBytes = [] (const juce::MemoryBlock& data, const juce::String& text)
                {
                    const auto* bytes = static_cast<const char*> (data.getData());
                    const auto* needle = text.toRawUTF8();
                    const size_t needleLength = (size_t) text.getNumBytesAsUTF8();

                    if (data.getSize() < needleLength || needleLength == 0)
                        return false;

                    for (size_t i = 0; i + needleLength <= data.getSize(); ++i)
                        if (std::memcmp (bytes + i, needle, needleLength) == 0)
                            return true;

                    return false;
                };

                const juce::String expectedBlock = juce::String (sis::identity::marker)
                                                   + sis::identity::makePluginCode ("Nachtbass") + "|Nachtbass";

                expect ("Der Name steht im Programm", containsBytes (binary, expectedBlock));
                expect ("Der alte Name steht nicht mehr drin",
                        ! containsBytes (binary, juce::String (sis::identity::marker)
                                                 + "Sist|Sample Instrument Studio"));

                // Und das mitgelieferte Instrument laedt auch wirklich
                if (auto xml = juce::XmlDocument::parse (bundled))
                {
                    auto playerOwner = std::make_unique<sis::StudioProcessor>();   // auf dem Heap: mehrere Prozessoren sprengen sonst den Stack
                    auto& player = *playerOwner;
                    player.prepareToPlay (sampleRate, blockSize);

                    expect ("Die App koennte das Instrument laden",
                            player.loadProjectState (juce::ValueTree::fromXml (*xml), appFolder));
                    player.getModel().sendSynchronousChangeMessage();

                    const auto* playerZone = player.getModel().getSelectedZone();
                    expect ("Es hat spielbare Zonen",
                            playerZone != nullptr && player.zoneHasAudio (*playerZone));

                    if (playerZone != nullptr)
                    {
                        player.getKeyboardState().noteOn (1, playerZone->rootNote, 0.9f);

                        float peak = 0.0f;
                        for (int i = 0; i < 60; ++i)
                            peak = juce::jmax (peak, renderBlock (player));

                        expect ("Und es klingt", peak > 0.05f);
                    }
                }
                else
                {
                    expect ("Das mitgelieferte Instrument laesst sich lesen", false);
                }
            }
        }
    }

    // --- Zwei Exporte ergeben zwei eigenstaendige Plugins ---------------------------
    section ("Zwei Exporte ergeben zwei eigenstaendige Plugins");
    {
        const juce::File templateBundle (SIS_VST3_TEMPLATE);

        if (templateBundle.isDirectory())
        {
            processor.getMasterGain().setValueNotifyingHost (0.74f);

            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("sis_identity_test");
            root.deleteRecursively();

            struct Exported { juce::String name; juce::File bundle; juce::String code; };
            std::vector<Exported> exported;

            for (const juce::String& name : { juce::String ("Nocturne Bass"), juce::String ("Air Pad") })
            {
                sis::ExportRequest request;
                request.targetFolder = root.getChildFile (name);
                request.instrumentName = name;
                request.projectState = processor.createProjectState();
                request.samples.add (file);
                request.copySamples = true;
                request.exportProjectFolder = false;
                request.exportPlugin = true;
                request.pluginTemplate = templateBundle;

                std::atomic<double> progress { 0.0 };
                const auto outcome = sis::InstrumentExporter::performExport (request, progress, nullptr);

                expect ("Export von " + name + " gelingt: " + outcome.message, outcome.succeeded);
                exported.push_back ({ name, outcome.pluginBundle, outcome.pluginCode });
            }

            expect ("Beide Instrumente haben verschiedene Kennungen",
                    exported.size() == 2 && exported[0].code != exported[1].code);

            juce::VST3PluginFormat format;
            juce::StringArray seenIds, seenNames;

            for (const auto& item : exported)
            {
                juce::OwnedArray<juce::PluginDescription> found;
                format.findAllTypesForFile (found, item.bundle.getFullPathName());

                expect ("Host findet " + item.name, ! found.isEmpty());

                if (found.isEmpty())
                    continue;

                seenIds.add (found[0]->createIdentifierString());
                seenNames.add (found[0]->name);

                juce::String error;
                auto instance = format.createInstanceFromDescription (*found[0], sampleRate, blockSize, error);
                expect (item.name + " startet: " + error, instance != nullptr);

                if (instance != nullptr)
                {
                    instance->prepareToPlay (sampleRate, blockSize);
                    juce::AudioBuffer<float> buffer (2, blockSize);
                    float peak = 0.0f;

                    for (int i = 0; i < 80; ++i)
                    {
                        buffer.clear();
                        juce::MidiBuffer midi;

                        if (i == 0)
                            midi.addEvent (juce::MidiMessage::noteOn (1, zone.rootNote, 0.9f), 0);

                        instance->processBlock (buffer, midi);
                        peak = juce::jmax (peak, buffer.getMagnitude (0, buffer.getNumSamples()));
                    }

                    expect (item.name + " klingt im Host", peak > 0.05f);
                    instance->releaseResources();
                }
            }

            expect ("Die DAW sieht zwei verschiedene Plugins",
                    seenIds.size() == 2 && seenIds[0] != seenIds[1]);
            expect ("Jedes traegt seinen Instrumentnamen",
                    seenNames.size() == 2 && seenNames[0] == "Nocturne Bass" && seenNames[1] == "Air Pad");

            root.deleteRecursively();
        }
    }

    // --- Ein fremdes Plugin als Effekt einer Spur ----------------------------------
    section ("Ein fremdes Plugin als Effekt einer Spur");
    // Als Versuchskaninchen dient unser eigener SIS Equalizer: Aus Sicht des Studios
    // ist er ein ganz gewöhnliches VST3 von der Festplatte.
    {
        const juce::File equalizerBundle (SIS_EQUALIZER_VST3);
        expect ("Der SIS Equalizer ist gebaut", equalizerBundle.exists());

        if (equalizerBundle.exists())
        {
            sis::PluginLibrary library;
            juce::String error;
            const auto identifiers = library.addPluginFile (equalizerBundle, error);

            expect ("Die Plugin-Liste nimmt das VST3 auf: " + error, ! identifiers.isEmpty());

            if (! identifiers.isEmpty())
            {
                const auto identifier = identifiers[0];

                const auto effects = library.getEffects();
                expect ("Es taucht unter den Effekten auf",
                        std::any_of (effects.begin(), effects.end(),
                                     [&identifier] (const juce::PluginDescription& description)
                                     {
                                         return description.createIdentifierString() == identifier;
                                     }));

                auto hosted = library.createInstance (identifier, sampleRate, blockSize, error);
                expect ("Das Plugin laedt: " + error, hosted != nullptr);

                if (hosted != nullptr)
                {
                    // Ausgangspegel des gehosteten Equalizers ganz nach unten (-24 dB)
                    bool foundOutput = false;

                    if (auto* loaded = hosted->getInstance())
                        for (auto* parameter : loaded->getParameters())
                            if (parameter->getName (64) == "Ausgang")
                            {
                                parameter->setValueNotifyingHost (0.0f);
                                foundOutput = true;
                            }

                    expect ("Der Ausgangsregler ist erreichbar", foundOutput);

                    const auto measure = [&hosted] (bool through)
                    {
                        constexpr int length = 9600;
                        juce::AudioBuffer<float> buffer (2, length);

                        for (int channel = 0; channel < 2; ++channel)
                            for (int i = 0; i < length; ++i)
                                buffer.setSample (channel, i,
                                                  (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                                    * 440.0 * i / sampleRate));

                        if (through)
                            for (int offset = 0; offset < length; offset += blockSize)
                            {
                                float* channels[2] = { buffer.getWritePointer (0, offset),
                                                       buffer.getWritePointer (1, offset) };
                                hosted->process (channels, juce::jmin (blockSize, length - offset));
                            }

                        // zweite Haelfte messen, damit Anlaufvorgaenge draussen bleiben
                        return buffer.getRMSLevel (0, length / 2, length / 2);
                    };

                    const float dry = measure (false);
                    const float wet = measure (true);

                    expect ("Ohne Plugin bleibt der Sinus laut", dry > 0.6f);
                    expect ("Das gehostete Plugin daempft hoerbar", wet < dry * 0.2f);
                    expect ("Es bleibt aber ein Signal uebrig", wet > 0.0f);

                    // --- Und jetzt derselbe Effekt im echten Prozessor ----------------
                    // Der Prozessor liest die Plugin-Liste beim Anlegen, deshalb ein frischer.
                    {
                        auto hostOwner = std::make_unique<sis::StudioProcessor>();   // auf dem Heap: mehrere Prozessoren sprengen sonst den Stack
                        auto& host = *hostOwner;
                        host.prepareToPlay (sampleRate, blockSize);
                        host.getModel().importSamples (files, host.getFormatManager());

                        auto& hostZone = host.getModel().zones.front();
                        sis::Track hostTrack;
                        hostTrack.name = "Mit fremdem Effekt";
                        { sis::Clip piece; piece.sample = file.getFileName(); hostTrack.clips.push_back (piece); }
                        hostTrack.gain = 1.0f;
                        hostTrack.loop = sis::LoopMode::sustainLoop;
                        hostTrack.effects.push_back (sis::Effect::makeExternal ("SIS Equalizer", identifier));
                        hostZone.tracks.push_back (hostTrack);
                        host.getModel().selectZone (hostZone.id);
                        host.getModel().sendSynchronousChangeMessage();

                        // Die Beispielzone bringt schon Spuren mit - unsere ist die letzte
                        const int hostTrackIndex = (int) hostZone.tracks.size() - 1;
                        auto inPlace = host.findHostedPlugin (hostZone.id, hostTrackIndex, identifier);

                        expect ("Der Prozessor laedt das Plugin selbst", inPlace != nullptr);

                        if (inPlace != nullptr)
                        {
                            const auto renderNote = [&host, &hostZone]
                            {
                                host.getKeyboardState().noteOn (1, hostZone.rootNote, 0.9f);

                                float peak = 0.0f;
                                for (int i = 0; i < 60; ++i)
                                    peak = juce::jmax (peak, renderBlock (host));

                                host.getKeyboardState().noteOff (1, hostZone.rootNote, 0.0f);

                                for (int i = 0; i < 400 && host.getActiveVoiceCount() > 0; ++i)
                                    renderBlock (host);

                                return peak;
                            };

                            const float loud = renderNote();
                            expect ("Mit neutralem Plugin klingt die Spur", loud > 0.1f);

                            // Ausgang des Plugins zu - der Prozessor muss das hoeren
                            if (auto* loaded = inPlace->getInstance())
                                for (auto* parameter : loaded->getParameters())
                                    if (parameter->getName (64) == "Ausgang")
                                        parameter->setValueNotifyingHost (0.0f);

                            const float quiet = renderNote();
                            expect ("Der fremde Effekt wirkt in der Spur", quiet < loud * 0.3f);

                            // Abgeschaltet gehoert das Plugin wieder aus der Kette
                            hostZone.tracks.back().effects.front().enabled = false;
                            host.getModel().sendSynchronousChangeMessage();

                            expect ("Abgeschaltet wird das Plugin ausrangiert",
                                    host.findHostedPlugin (hostZone.id, hostTrackIndex, identifier) == nullptr);
                            expect ("Ohne den Effekt ist es wieder laut", renderNote() > loud * 0.5f);

                            // Beim Speichern muss der Zustand des Plugins mitkommen
                            hostZone.tracks.back().effects.front().enabled = true;
                            host.getModel().sendSynchronousChangeMessage();

                            if (auto reloaded = host.findHostedPlugin (hostZone.id, hostTrackIndex, identifier))
                            {
                                float restored = -1.0f;

                                for (auto* parameter : reloaded->getInstance()->getParameters())
                                    if (parameter->getName (64) == "Ausgang")
                                    {
                                        restored = parameter->getValue();
                                        parameter->setValueNotifyingHost (0.25f);
                                    }

                                // Aus- und wieder Einschalten darf die Einstellungen nicht verwerfen
                                expect ("Nach dem Wiedereinschalten stehen die Regler noch",
                                        restored >= 0.0f && restored < 0.01f);

                                const auto saved = host.createProjectState();
                                sis::InstrumentModel readBack;
                                juce::AudioFormatManager formats;
                                formats.registerBasicFormats();

                                juce::String storedState;

                                if (readBack.fromValueTree (saved.getChildWithName ("Instrument"), formats, juce::File()))
                                    for (const auto& zoneRead : readBack.zones)
                                        for (const auto& trackRead : zoneRead.tracks)
                                            for (const auto& effectRead : trackRead.effects)
                                                if (effectRead.pluginIdentifier == identifier)
                                                    storedState = effectRead.pluginState;

                                expect ("Der Plugin-Zustand landet im Projekt", storedState.isNotEmpty());
                            }
                        }
                    }

                    // --- Bounce: die Effekte wandern fest ins Sample ------------------
                    section ("Bounce: die Effekte wandern fest ins Sample");
                    {
                        auto bouncerOwner = std::make_unique<sis::StudioProcessor>();   // auf dem Heap: mehrere Prozessoren sprengen sonst den Stack
                        auto& bouncer = *bouncerOwner;
                        bouncer.prepareToPlay (sampleRate, blockSize);
                        bouncer.getModel().importSamples (files, bouncer.getFormatManager());

                        auto& zoneToBounce = bouncer.getModel().zones.front();
                        sis::Track loudTrack;
                        loudTrack.name = "Vor dem Bounce";
                        { sis::Clip piece; piece.sample = file.getFileName(); loudTrack.clips.push_back (piece); }
                        loudTrack.gain = 1.0f;
                        loudTrack.loop = sis::LoopMode::oneShot;
                        loudTrack.effects.push_back (sis::Effect::makeExternal ("SIS Equalizer", identifier));
                        loudTrack.effects.front().enabled = false;   // erst ohne Effekt messen
                        zoneToBounce.tracks.push_back (loudTrack);
                        bouncer.getModel().selectZone (zoneToBounce.id);
                        bouncer.getModel().sendSynchronousChangeMessage();

                        const int bounceTrackIndex = (int) zoneToBounce.tracks.size() - 1;

                        // Erst ab Block 30 messen: bis dahin sind Huellkurve und die
                        // Glaettung der Plugin-Regler eingeschwungen. Danach wird sauber
                        // ausgespielt, damit keine Stimme in die naechste Messung klingt.
                        const auto playZone = [&bouncer, &zoneToBounce]
                        {
                            bouncer.getKeyboardState().noteOn (1, zoneToBounce.rootNote, 0.9f);

                            float peak = 0.0f;
                            for (int i = 0; i < 90; ++i)
                            {
                                const float level = renderBlock (bouncer);

                                if (i >= 30)
                                    peak = juce::jmax (peak, level);
                            }

                            bouncer.getKeyboardState().noteOff (1, zoneToBounce.rootNote, 0.0f);

                            for (int i = 0; i < 4000 && bouncer.getActiveVoiceCount() > 0; ++i)
                                renderBlock (bouncer);

                            return peak;
                        };

                        const float withoutEffect = playZone();
                        expect ("Ohne Effekt klingt die Zone", withoutEffect > 0.1f);

                        // Effekt an und ganz leise stellen
                        zoneToBounce.tracks.back().effects.front().enabled = true;
                        bouncer.getModel().sendSynchronousChangeMessage();

                        auto plugin = bouncer.findHostedPlugin (zoneToBounce.id, bounceTrackIndex, identifier);
                        expect ("Das Plugin haengt in der Spur", plugin != nullptr);

                        if (plugin != nullptr)
                            for (auto* parameter : plugin->getInstance()->getParameters())
                                if (parameter->getName (64) == "Ausgang")
                                    parameter->setValueNotifyingHost (0.0f);   // -24 dB

                        const float withEffect = playZone();
                        expect ("Der Effekt daempft vor dem Bounce", withEffect < withoutEffect * 0.3f);

                        // Jetzt bouncen
                        const auto bounceFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                                      .getChildFile ("sis_bounce_test");
                        bounceFolder.deleteRecursively();

                        const auto outcome = bouncer.bounceZone (zoneToBounce.id, bounceFolder);
                        expect ("Der Bounce gelingt: " + outcome.message, outcome.succeeded);

                        if (outcome.succeeded)
                        {
                            expect ("Die Audiodatei liegt da", outcome.file.existsAsFile());
                            expect ("Sie hat Inhalt", outcome.file.getSize() > 1000);
                            expect ("Sie ist nicht laenger als noetig",
                                    outcome.lengthSeconds > 0.5 && outcome.lengthSeconds < 6.0);

                            // Ohne laufende Nachrichtenschleife den Plan von Hand neu bauen
                            bouncer.getModel().sendSynchronousChangeMessage();

                            auto& bounced = bouncer.getModel().zones.front();
                            expect ("Die Zone hat jetzt genau eine Spur", bounced.tracks.size() == 1);

                            if (bounced.tracks.size() == 1)
                            {
                                expect ("Die Spur zeigt auf die gerenderte Datei",
                                        bounced.tracks.front().clips.size() == 1
                                        && bounced.tracks.front().clips.front().sample == outcome.file.getFileName());
                                expect ("Die Effektkette ist leer", bounced.tracks.front().effects.empty());
                                expect ("Das fremde Plugin wird nicht mehr gehalten",
                                        bouncer.findHostedPlugin (bounced.id, 0, identifier) == nullptr);
                            }

                            // Der Beweis: ohne jeden Effekt klingt es weiter so leise wie vorher,
                            // die Daempfung steckt also im Sample.
                            const float afterBounce = playZone();
                            expect ("Nach dem Bounce bleibt es leise", afterBounce < withoutEffect * 0.4f);
                            expect ("Aber nicht stumm", afterBounce > withEffect * 0.4f);
                        }

                        bounceFolder.deleteRecursively();
                    }

                    // Zustand sichern und in einer frischen Instanz wiederherstellen
                    const auto state = hosted->getStateAsString();
                    expect ("Der Plugin-Zustand laesst sich sichern", state.isNotEmpty());

                    auto restored = library.createInstance (identifier, sampleRate, blockSize, error);

                    if (restored != nullptr)
                    {
                        restored->setStateFromString (state);

                        float output = -1.0f;

                        if (auto* loaded = restored->getInstance())
                            for (auto* parameter : loaded->getParameters())
                                if (parameter->getName (64) == "Ausgang")
                                    output = parameter->getValue();

                        expect ("Der Zustand kommt zurueck", output >= 0.0f && output < 0.01f);
                    }
                }
            }
        }
    }

    file.deleteFile();

    // --- Handbuch: eingebettet und in sich stimmig -------------------------------------
    section ("Handbuch: eingebettet und in sich stimmig");
    {
        const auto manual = sis::manualHtml();
        expect ("Das Handbuch steckt im Programm", manual.length() > 10000);
        expect ("Mit Platzhalter für die Version", manual.contains ("{{VERSION}}"));
        expect ("Umlaute kommen heil an", manual.contains (juce::String::fromUTF8 ("Überblick")));

        // Jeder Verweis im Inhaltsverzeichnis hat sein Ziel
        int links = 0;
        for (int at = manual.indexOf ("href=\"#"); at >= 0; at = manual.indexOf (at + 1, "href=\"#"))
        {
            const auto target = manual.substring (at + 7).upToFirstOccurrenceOf ("\"", false, false);
            expect ("Ziel des Verweises vorhanden: " + target, manual.contains ("id=\"" + target + "\""));
            ++links;
        }

        expect ("Das Inhaltsverzeichnis ist da", links >= 15);
    }

    std::printf ("%d Prüfungen, %d Fehler\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
