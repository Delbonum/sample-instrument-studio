#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <set>

namespace sis
{
namespace
{
    const juce::Identifier projectType { "SampleInstrumentStudio" };
    const juce::Identifier versionId { "version" };
    const juce::Identifier masterId { "master" };
    constexpr int projectVersion = 1;
}

StudioProcessor::StudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();

    addParameter (masterGain = new juce::AudioParameterFloat (juce::ParameterID { "master", 1 }, "Master",
                                                              juce::NormalisableRange<float> (0.0f, 1.0f), 0.74f));
    lastGain = masterGain->get();

    model.addChangeListener (this);

    // Als exportiertes Instrument: das mitgelieferte Instrument laden. Ob Plugin oder
    // eigenständige App, entscheidet nicht die Bauform, sondern ob neben dem Programm
    // ein Instrument liegt - beim Studio selbst liegt dort keines.
    // Ein Host, der einen Zustand gespeichert hat, überschreibt es danach.
    loadedBundledInstrument = loadBundledInstrument();

    rebuildRenderPlan();
    history.reset();     // was bis hier geschah, gehört nicht in den Verlauf
    startTimer (2000);   // gibt alte Pläne frei, sobald keine Stimme sie mehr hält
}

StudioProcessor::~StudioProcessor()
{
    stopTimer();
    model.removeChangeListener (this);
}

void StudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    loadMeasurer.reset (sampleRate, samplesPerBlock);
    keyboardState.reset();
    engine.prepare (sampleRate, samplesPerBlock);
    engine.reset();
    lastGain = masterGain->get();

    for (auto& [key, plugin] : hostedPlugins)
        if (plugin != nullptr)
            plugin->prepare (sampleRate, samplesPerBlock);

    // Filter-Koeffizienten hängen an der Samplerate
    rebuildRenderPlan();
}

bool StudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

double StudioProcessor::getTailLengthSeconds() const
{
    return juce::jmax (0.1, (double) model.envelope.release * 5.0);
}

void StudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::AudioProcessLoadMeasurer::ScopedTimer timer (loadMeasurer, buffer.getNumSamples());
    const juce::ScopedNoDenormals noDenormals;

    // Bildschirm-Klaviatur in den MIDI-Strom mischen (und Host-Noten dort anzeigen)
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);


    buffer.clear();
    engine.process (buffer, midi);

    const float gain = masterGain->get();
    buffer.applyGainRamp (0, buffer.getNumSamples(), lastGain, gain);
    lastGain = gain;

    // Spitzenpegel mit Nachleuchten, damit die Anzeige ablesbar bleibt
    const float peak = buffer.getMagnitude (0, buffer.getNumSamples());

    const float previous = outputLevel.load (std::memory_order_relaxed);
    outputLevel.store (juce::jmax (peak, previous * 0.82f), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* StudioProcessor::createEditor()
{
    return new StudioEditor (*this);
}

//==============================================================================
void StudioProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Der Verlauf hängt hier und nicht an den einzelnen Aufrufern - so ist
    // jede Änderung erfasst, auch eine, die niemand angemeldet hat.
    history.modelChanged();
    rebuildRenderPlan();
}

juce::String StudioProcessor::makeHostedKey (const juce::String& zoneId, int trackIndex,
                                             const juce::String& pluginIdentifier)
{
    return zoneId + "/" + juce::String (trackIndex) + "/" + pluginIdentifier;
}

HostedPlugin::Ptr StudioProcessor::provideHostedPlugin (const juce::String& zoneId, int trackIndex,
                                                        const Effect& effect)
{
    if (effect.pluginIdentifier.isEmpty())
        return nullptr;

    const auto key = makeHostedKey (zoneId, trackIndex, effect.pluginIdentifier);
    const auto existing = hostedPlugins.find (key);

    if (existing != hostedPlugins.end())
        return existing->second;

    const double rate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const int blockSize = getBlockSize() > 0 ? getBlockSize() : 512;

    juce::String error;
    auto hosted = pluginLibrary.createInstance (effect.pluginIdentifier, rate, blockSize, error);

    if (hosted == nullptr)
        return nullptr;

    hosted->setStateFromString (effect.pluginState);
    hostedPlugins[key] = hosted;
    return hosted;
}

HostedPlugin::Ptr StudioProcessor::findHostedPlugin (const juce::String& zoneId, int trackIndex,
                                                     const juce::String& pluginIdentifier) const
{
    const auto found = hostedPlugins.find (makeHostedKey (zoneId, trackIndex, pluginIdentifier));
    return found != hostedPlugins.end() ? found->second : nullptr;
}

void StudioProcessor::removeTrack (const juce::String& zoneId, int trackIndex)
{
    auto* zone = model.findZone (zoneId);

    if (zone == nullptr || ! juce::isPositiveAndBelow (trackIndex, (int) zone->tracks.size()))
        return;

    // Einstellungen sichern, dann alle Instanzen der Zone neu aufbauen lassen
    captureHostedPluginStates();

    const auto prefix = zoneId + "/";

    for (auto it = hostedPlugins.begin(); it != hostedPlugins.end();)
    {
        if (it->first.startsWith (prefix))
        {
            retiredPlugins.push_back (it->second);
            it = hostedPlugins.erase (it);
        }
        else
        {
            ++it;
        }
    }

    model.removeTrack (*zone, trackIndex);
}

void StudioProcessor::captureHostedPluginStates()
{
    for (auto& zone : model.zones)
    {
        for (int trackIndex = 0; trackIndex < (int) zone.tracks.size(); ++trackIndex)
        {
            for (auto& effect : zone.tracks[(size_t) trackIndex].effects)
            {
                if (effect.type != EffectType::external)
                    continue;

                if (auto hosted = findHostedPlugin (zone.id, trackIndex, effect.pluginIdentifier))
                    effect.pluginState = hosted->getStateAsString();
            }
        }
    }
}

juce::File StudioProcessor::getDefaultBounceFolder() const
{
    if (model.projectFile != juce::File() && model.projectFile.getParentDirectory().isDirectory())
        return model.projectFile.getParentDirectory().getChildFile ("Bounces");

    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Sample Instrument Studio")
               .getChildFile ("Bounces");
}

StudioProcessor::BounceOutcome StudioProcessor::bounceZone (const juce::String& zoneId,
                                                            const juce::File& targetFolder)
{
    BounceOutcome outcome;

    auto* zone = model.findZone (zoneId);

    if (zone == nullptr)
    {
        outcome.message = "Die Zone gibt es nicht mehr."_u;
        return outcome;
    }

    if (zone->tracks.empty())
    {
        outcome.message = "Die Zone hat keine Spuren."_u;
        return outcome;
    }

    const double rate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const int block = getBlockSize() > 0 ? getBlockSize() : 512;

    // Damit die frischen Instanzen so klingen wie die laufenden
    captureHostedPluginStates();

    BounceRequest request;
    request.model = &model;
    request.cache = &sampleCache;
    request.zoneId = zoneId;
    request.sampleRate = rate;
    request.blockSize = block;

    // Eigene Instanzen: die des laufenden Instruments gehören dem Audio-Thread.
    request.hostedPlugins = [this, rate, block] (const juce::String&, int, const Effect& effect) -> HostedPlugin::Ptr
    {
        juce::String error;
        auto instance = pluginLibrary.createInstance (effect.pluginIdentifier, rate, block, error);

        if (instance != nullptr)
            instance->setStateFromString (effect.pluginState);

        return instance;
    };

    const auto bounce = renderZone (request);

    if (! bounce.succeeded)
    {
        outcome.message = bounce.message;
        return outcome;
    }

    const auto file = targetFolder.getChildFile (juce::File::createLegalFileName (zone->name) + ".wav")
                          .getNonexistentSibling();

    juce::String error;

    if (! writeBounceToFile (bounce, file, error))
    {
        outcome.message = error;
        return outcome;
    }

    // Die gerenderte Datei wird zur einzigen Spur der Zone
    juce::Array<juce::File> files;
    files.add (file);
    model.importSamples (files, formatManager);

    const auto colour = zone->tracks.front().colour;
    const auto softColour = zone->tracks.front().softColour;

    Track bounced;
    bounced.name = zone->name;
    bounced.colour = colour;
    bounced.softColour = softColour;
    bounced.gain = 1.0f;          // Pegel, Panorama, Tonhöhe und Effekte stecken schon im Sample
    bounced.pan = 0.0f;
    bounced.pitch = 0;
    bounced.cents = 0.5f;
    bounced.loop = LoopMode::oneShot;

    Clip whole;
    whole.sample = file.getFileName();
    whole.natural = juce::jmax (0.02, bounce.getLengthSeconds() / InstrumentModel::timelineSeconds);
    bounced.clips.push_back (whole);

    zone->tracks.clear();
    zone->tracks.push_back (std::move (bounced));
    zone->selectedTrack = 0;

    model.notifyChanged();

    outcome.succeeded = true;
    outcome.file = file;
    outcome.lengthSeconds = bounce.getLengthSeconds();
    outcome.message = "Zone "_u + zone->name + " gebounct · "_u
                      + juce::String (bounce.getLengthSeconds(), 2) + " s"_u;
    return outcome;
}

void StudioProcessor::rebuildRenderPlan()
{
    sampleCache.syncWith (model, formatManager);

    auto plan = buildRenderPlan (model, sampleCache, getSampleRate() > 0.0 ? getSampleRate() : 44100.0,
                                 [this] (const juce::String& zoneId, int trackIndex, const Effect& effect)
                                 {
                                     return provideHostedPlugin (zoneId, trackIndex, effect);
                                 });

    // Instanzen, die im Instrument nicht mehr vorkommen, ausrangieren
    std::set<juce::String> stillNeeded;

    for (const auto& zone : model.zones)
        for (int trackIndex = 0; trackIndex < (int) zone.tracks.size(); ++trackIndex)
            for (const auto& effect : zone.tracks[(size_t) trackIndex].effects)
                if (effect.type == EffectType::external && effect.enabled)
                    stillNeeded.insert (makeHostedKey (zone.id, trackIndex, effect.pluginIdentifier));

    // Wer gleich ausrangiert wird, gibt vorher seine Einstellungen ins Modell zurück.
    // Sonst käme ein kurz abgeschalteter Effekt mit Werkseinstellungen wieder.
    for (auto& zone : model.zones)
        for (int trackIndex = 0; trackIndex < (int) zone.tracks.size(); ++trackIndex)
            for (auto& effect : zone.tracks[(size_t) trackIndex].effects)
            {
                if (effect.type != EffectType::external)
                    continue;

                const auto key = makeHostedKey (zone.id, trackIndex, effect.pluginIdentifier);

                if (stillNeeded.count (key) > 0)
                    continue;

                const auto leaving = hostedPlugins.find (key);

                if (leaving != hostedPlugins.end() && leaving->second != nullptr)
                    effect.pluginState = leaving->second->getStateAsString();
            }

    for (auto it = hostedPlugins.begin(); it != hostedPlugins.end();)
    {
        if (stillNeeded.count (it->first) > 0)
        {
            ++it;
            continue;
        }

        retiredPlugins.push_back (it->second);
        it = hostedPlugins.erase (it);
    }

    if (publishedPlan != nullptr)
        retiredPlans.push_back (publishedPlan);

    publishedPlan = plan;
    engine.setPlan (std::move (plan));
}

void StudioProcessor::timerCallback()
{
    // Plan und fremde Plugins werden erst hier freigegeben – nie auf dem Audio-Thread.
    retiredPlans.erase (std::remove_if (retiredPlans.begin(), retiredPlans.end(),
                                        [] (const RenderPlan::Ptr& plan)
                                        {
                                            return plan == nullptr || plan->getReferenceCount() <= 1;
                                        }),
                        retiredPlans.end());

    retiredPlugins.erase (std::remove_if (retiredPlugins.begin(), retiredPlugins.end(),
                                          [] (const HostedPlugin::Ptr& plugin)
                                          {
                                              return plugin == nullptr || plugin->getReferenceCount() <= 1;
                                          }),
                          retiredPlugins.end());
}

juce::File StudioProcessor::findBundledInstrument()
{
    const auto module = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    if (module == juce::File())
        return {};

    // VST3-Bundle:  <Name>.vst3/Contents/x86_64-win/<Name>.vst3
    const auto resources = module.getParentDirectory().getParentDirectory().getChildFile ("Resources");

    for (const auto& folder : { resources, module.getParentDirectory() })
    {
        const auto candidate = folder.getChildFile ("Instrument.sisp");

        if (candidate.existsAsFile())
            return candidate;
    }

    return {};
}

bool StudioProcessor::loadBundledInstrument()
{
    const auto file = findBundledInstrument();

    if (! file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr)
        return false;

    if (! loadProjectState (juce::ValueTree::fromXml (*xml), file.getParentDirectory()))
        return false;

    model.projectFile = file;
    return true;
}

float StudioProcessor::peakOf (const Clip& clip) const
{
    const auto data = sampleCache.get (clip.sample);

    if (data == nullptr || data->getNumSamples() < 2)
        return 0.0f;

    const int total = data->getNumSamples();
    const int start = juce::jlimit (0, total - 1, (int) (clip.trimStart * total));
    const int end = juce::jlimit (start + 1, total, (int) std::ceil (clip.trimEnd * total));
    return data->buffer.getMagnitude (start, end - start);
}

bool StudioProcessor::zoneHasAudio (const Zone& zone) const
{
    for (const auto& track : zone.tracks)
        for (const auto& clip : track.clips)
            if (clip.hasSample() && sampleCache.get (clip.sample) != nullptr)
                return true;

    return false;
}

//==============================================================================
juce::ValueTree StudioProcessor::createProjectState() const
{
    const_cast<StudioProcessor*> (this)->captureHostedPluginStates();

    juce::ValueTree state (projectType);
    state.setProperty (versionId, projectVersion, nullptr);
    state.setProperty (masterId, masterGain->get(), nullptr);
    state.appendChild (model.toValueTree(), nullptr);
    return state;
}

bool StudioProcessor::loadProjectState (const juce::ValueTree& state, const juce::File& baseFolder)
{
    if (! state.hasType (projectType))
        return false;

    const auto instrument = state.getChild (0);
    if (! model.fromValueTree (instrument, formatManager, baseFolder))
        return false;

    masterGain->beginChangeGesture();
    *masterGain = juce::jlimit (0.0f, 1.0f, (float) state.getProperty (masterId, 0.74f));
    masterGain->endChangeGesture();

    // Ein geladenes Instrument ist der neue Ausgangspunkt - nicht zurücknehmbar
    history.reset();

    // fromValueTree meldet die Änderung bereits; der Plan entsteht daraufhin neu
    return true;
}

void StudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createProjectState().createXml())
        copyXmlToBinary (*xml, destData);
}

void StudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Hinweis: Das Modell gehört dem Message-Thread. Die gängigen Hosts rufen dies dort auf;
    // der Audio-Thread liest ausschließlich den daraus gebauten RenderPlan.
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        loadProjectState (juce::ValueTree::fromXml (*xml));
}
} // namespace sis

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sis::StudioProcessor();
}
