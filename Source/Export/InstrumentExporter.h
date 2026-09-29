#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace sis
{
/** Was exportiert werden soll. Wird auf dem Message-Thread zusammengestellt,
    damit der Hintergrund-Thread das Modell nicht anfassen muss. */
struct ExportRequest
{
    juce::File targetFolder;
    juce::String instrumentName;
    juce::ValueTree projectState;        // Momentaufnahme des gesamten Projekts
    juce::Array<juce::File> samples;     // tatsächlich verwendete Audiodateien
    bool copySamples = true;

    /** Wenn `exportPlugin` an ist: Dieses VST3-Bundle wird kopiert und bekommt
        das Instrument in seine Resources gelegt. */
    juce::File pluginTemplate;

    /** Wenn `exportStandalone` an ist: Diese Programmdatei wird kopiert und bekommt
        das Instrument daneben gelegt. */
    juce::File standaloneTemplate;

    bool exportProjectFolder = true;
    bool exportPlugin = false;
    bool exportStandalone = false;
};

struct ExportOutcome
{
    bool succeeded = false;
    juce::String message;
    juce::File projectFile;     // geschriebene .sisp-Datei (falls gewählt)
    juce::File pluginBundle;    // geschriebenes .vst3-Bundle (falls gewählt)
    juce::File standaloneApp;   // geschriebene Programmdatei (falls gewählt)
    juce::String pluginCode;    // dessen VST3-Kennung (vier Zeichen)
    int copiedSamples = 0;
    juce::int64 totalBytes = 0;
};

/** Schreibt Projektdatei und Samples in einen Ordner.
    Die Dateiarbeit läuft im Hintergrund, die Rückmeldung kommt auf dem Message-Thread. */
class InstrumentExporter final : private juce::Thread
{
public:
    InstrumentExporter();
    ~InstrumentExporter() override;

    /** Startet den Export. Ein bereits laufender Export wird nicht unterbrochen. */
    bool start (ExportRequest, std::function<void (ExportOutcome)> onFinished);

    bool isExporting() const noexcept    { return isThreadRunning(); }
    double getProgress() const noexcept  { return progress.load (std::memory_order_relaxed); }
    void cancelAndWait();

    /** Größe der Dateien, die ein Export schreiben würde. */
    static juce::int64 estimateBytes (const ExportRequest&);

    /** Sucht das mitgelieferte VST3 (neben der App, im Build-Ordner oder im VST3-Ordner). */
    static juce::File findPluginTemplate();

    /** Sucht die Standalone-Programmdatei, aus der eine eigenständige App entsteht.
        Das ist im Normalfall das gerade laufende Programm selbst. */
    static juce::File findStandaloneTemplate();

    /** Die eigentliche Arbeit – ohne Thread, damit sie sich einzeln prüfen lässt. */
    static ExportOutcome performExport (const ExportRequest&, std::atomic<double>& progress,
                                        const std::function<bool()>& shouldCancel);

private:
    void run() override;

    ExportRequest request;
    std::function<void (ExportOutcome)> finished;
    std::atomic<double> progress { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentExporter)
};
} // namespace sis
