#include "PluginLibrary.h"
#include "../Model/Text.h"

namespace sis
{
/* Nur die geladene Instanz, nicht das Suchen und Verwalten.

   Getrennt, weil die **Effekt-Plugins** diesen Teil brauchen: `applyEffectChain` kennt
   den Fall „fremdes Plugin“ und ruft `HostedPlugin::process`. Das Suchen dagegen braucht
   `juce::VST3PluginFormat`, und das gibt es nur mit `JUCE_PLUGINHOST_VST3` – das wollte
   man nicht in einundzwanzig Effekt-Plugins schleppen, die selbst nichts hosten. */
//==============================================================================
HostedPlugin::HostedPlugin (std::unique_ptr<juce::AudioPluginInstance> newInstance, juce::String newIdentifier)
    : instance (std::move (newInstance)), identifier (std::move (newIdentifier))
{
}

HostedPlugin::~HostedPlugin()
{
    // Der Audio-Thread darf hier nicht mehr zugreifen; dafür sorgt die Liste
    // ausrangierter Plugins im Prozessor.
    readyToProcess.store (false, std::memory_order_release);

    if (instance != nullptr)
        instance->releaseResources();
}

void HostedPlugin::prepare (double sampleRate, int blockSize)
{
    if (instance == nullptr)
        return;

    readyToProcess.store (false, std::memory_order_release);

    instance->setPlayConfigDetails (2, 2, sampleRate, blockSize);
    instance->prepareToPlay (sampleRate, blockSize);
    emptyMidi.ensureSize (256);

    readyToProcess.store (true, std::memory_order_release);
}

void HostedPlugin::process (float* const* channels, int numSamples) noexcept
{
    if (! readyToProcess.load (std::memory_order_acquire) || instance == nullptr)
        return;

    // Puffer um vorhandenen Speicher legen – keine Speicheranforderung im Audio-Thread
    juce::AudioBuffer<float> buffer (const_cast<float**> (channels), 2, numSamples);
    emptyMidi.clear();
    instance->processBlock (buffer, emptyMidi);
}

juce::String HostedPlugin::getName() const
{
    return instance != nullptr ? instance->getPluginDescription().name : juce::String();
}

juce::String HostedPlugin::getStateAsString() const
{
    if (instance == nullptr)
        return {};

    juce::MemoryBlock data;
    instance->getStateInformation (data);
    return data.toBase64Encoding();
}

void HostedPlugin::setStateFromString (const juce::String& text)
{
    if (instance == nullptr || text.isEmpty())
        return;

    juce::MemoryBlock data;

    if (data.fromBase64Encoding (text) && data.getSize() > 0)
        instance->setStateInformation (data.getData(), (int) data.getSize());
}
} // namespace sis
