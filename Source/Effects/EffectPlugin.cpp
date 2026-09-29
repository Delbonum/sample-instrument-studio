#include "EffectPlugin.h"
#include "EffectPluginEditor.h"

namespace sis
{
namespace
{
    constexpr int planRebuildMs = 30;   // Koeffizienten neu: oft genug, um nicht zu haken
}

EffectType EffectPlugin::effectType() noexcept
{
    return EffectType::SIS_EFFECT_TYPE;
}

Effect EffectPlugin::makePrototype()
{
    /* Aus derselben Liste wie das „+“-Menü des Studios – eine zweite Fabrik daneben wäre
       die Stelle, an der ein Plugin eines Tages andere Regler hätte als sein Vorbild. */
    for (const auto& effect : Effect::builtIn())
        if (effect.type == effectType())
            return effect;

    jassertfalse;   // dieser Effekt steht nicht in Effect::builtIn()
    return {};
}

juce::String EffectPlugin::parameterId (int index)
{
    return "p" + juce::String (index);
}

juce::AudioProcessorValueTreeState::ParameterLayout EffectPlugin::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto proto = makePrototype();

    for (int i = 0; i < (int) proto.parameters.size(); ++i)
    {
        const auto& parameter = proto.parameters[(size_t) i];

        /* Alle Regler laufen von 0 bis 1, wie im Modell. Was daraus wird – Hertz,
           Millisekunden, Dezibel – rechnet `buildSingleEffect` aus, an genau einer Stelle. */
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { parameterId (i), 1 },
            parameter.label,
            juce::NormalisableRange<float> (0.0f, 1.0f),
            parameter.value));
    }

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Umgehen", false));

    return layout;
}

EffectPlugin::~EffectPlugin()
{
    for (int i = 0; i < (int) prototype.parameters.size(); ++i)
        state.removeParameterListener (parameterId (i), this);
}

EffectPlugin::EffectPlugin()
    : AudioProcessor (BusesProperties().withInput ("Eingang", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Ausgang", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SisEffect", createLayout())
{
    for (int i = 0; i < (int) prototype.parameters.size(); ++i)
        state.addParameterListener (parameterId (i), this);

    rebuildPlan();
    startTimer (planRebuildMs);
}

bool EffectPlugin::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && layouts.getMainInputChannelSet() == out;
}

double EffectPlugin::getTailLengthSeconds() const
{
    // Hall und Delay klingen nach; der Rest nicht nennenswert
    switch (effectType())
    {
        case EffectType::reverb: return 4.0;
        case EffectType::delay:  return 2.0;
        default:                 return 0.0;
    }
}

void EffectPlugin::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = sampleRate;
    processors.prepare (sampleRate, samplesPerBlock);
    processors.reset();

    parametersChanged.store (true, std::memory_order_relaxed);
    rebuildPlan();
}

void EffectPlugin::rebuildPlan()
{
    // Message-Thread: hier darf gerechnet und angelegt werden
    auto effect = prototype;

    for (int i = 0; i < (int) effect.parameters.size(); ++i)
        if (auto* parameter = state.getRawParameterValue (parameterId (i)))
            effect.parameters[(size_t) i].value = parameter->load();

    auto built = buildSingleEffect (effect, preparedSampleRate);

    const juce::SpinLock::ScopedLockType lock (planLock);
    plan = std::move (built);
}

void EffectPlugin::parameterChanged (const juce::String&, float)
{
    /* Nur merken. Umgerechnet wird im Timer auf dem Message-Thread – ein Host darf diese
       Rückmeldung auch aus dem Audio-Thread schicken, und dort dürfen keine
       Koeffizienten entstehen. */
    parametersChanged.store (true, std::memory_order_relaxed);
}

void EffectPlugin::timerCallback()
{
    if (parametersChanged.exchange (false, std::memory_order_relaxed))
        rebuildPlan();
}

void EffectPlugin::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int channel = numChannels; channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const bool bypassed = state.getRawParameterValue ("bypass")->load() > 0.5f;

    if (! bypassed)
    {
        /* Mono: der zweite Kanal zeigt auf denselben Speicher. Die Effekte schreiben dann
           zweimal dasselbe, was richtig ist – ein eigener Mono-Pfad durch zweiundzwanzig
           Effekte wäre zweiundzwanzigmal Gelegenheit, es verschieden zu machen. */
        float* channels[2] = { buffer.getWritePointer (0),
                               buffer.getWritePointer (numChannels > 1 ? 1 : 0) };

        const juce::SpinLock::ScopedTryLockType lock (planLock);

        if (lock.isLocked())
            applyEffectChain (plan, processors, channels, numSamples);
    }

    outputLevel.store (buffer.getMagnitude (0, numSamples), std::memory_order_relaxed);
}

//==============================================================================
juce::StringArray EffectPlugin::getPresetNames() const
{
    return presets.getNames (effectType());
}

bool EffectPlugin::loadPreset (const juce::String& name)
{
    auto effect = prototype;

    if (! presets.apply (name, effect))
        return false;

    for (int i = 0; i < (int) effect.parameters.size(); ++i)
        if (auto* parameter = state.getParameter (parameterId (i)))
            parameter->setValueNotifyingHost (effect.parameters[(size_t) i].value);

    return true;
}

void EffectPlugin::savePreset (const juce::String& name)
{
    auto effect = prototype;

    for (int i = 0; i < (int) effect.parameters.size(); ++i)
        if (auto* parameter = state.getRawParameterValue (parameterId (i)))
            effect.parameters[(size_t) i].value = parameter->load();

    presets.save (name, effect);
}

void EffectPlugin::deletePreset (const juce::String& name)
{
    presets.remove (effectType(), name);
}

void EffectPlugin::resetToFactoryDefaults()
{
    auto effect = prototype;
    PresetLibrary::applyFactoryDefaults (effect);

    for (int i = 0; i < (int) effect.parameters.size(); ++i)
        if (auto* parameter = state.getParameter (parameterId (i)))
            parameter->setValueNotifyingHost (effect.parameters[(size_t) i].value);
}

//==============================================================================
void EffectPlugin::getStateInformation (juce::MemoryBlock& destination)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void EffectPlugin::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));

    parametersChanged.store (true, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* EffectPlugin::createEditor()
{
    return new EffectPluginEditor (*this);
}
} // namespace sis

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sis::EffectPlugin();
}
