#include "SampleDrop.h"

namespace sis::sampleDrop
{
namespace
{
    const juce::String prefix ("sis-sample:");
}

juce::var describe (int sampleIndex)
{
    return prefix + juce::String (sampleIndex);
}

int indexOf (const juce::DragAndDropTarget::SourceDetails& details)
{
    const auto text = details.description.toString();
    return text.startsWith (prefix) ? text.substring (prefix.length()).getIntValue() : -1;
}

const SampleFile* resolve (StudioContext& ctx, int sampleIndex)
{
    if (! juce::isPositiveAndBelow (sampleIndex, (int) ctx.model.samples.size()))
        return nullptr;

    const auto& sample = ctx.model.samples[(size_t) sampleIndex];

    if (! sample.file.existsAsFile())
    {
        ctx.toast (sample.name + " ist ein Beispiel-Eintrag ohne Datei"_u);
        return nullptr;
    }

    return &sample;
}

void intoZone (StudioContext& ctx, Zone& zone, int sampleIndex)
{
    const auto* sample = resolve (ctx, sampleIndex);

    if (sample == nullptr)
        return;

    const bool hadSamples = zone.numSamples() > 0;

    ctx.step (sample->name + " zugeordnet");
    ctx.model.selectZone (zone.id);
    ctx.model.addSampleTrack (zone, *sample);

    ctx.toast (sample->name + " → "_u + zone.name
               + (hadSamples ? " · als weitere Spur"_u : juce::String()));
}

void ontoNote (StudioContext& ctx, int note, int velocity, int sampleIndex)
{
    auto& model = ctx.model;

    if (model.kind == InstrumentKind::drumKit)
    {
        if (auto* zone = const_cast<Zone*> (model.findZoneForNote (note)))
        {
            intoZone (ctx, *zone, sampleIndex);
            return;
        }

        for (const auto& part : drumParts())
        {
            if (part.note == note)
            {
                intoDrumPart (ctx, part, sampleIndex);
                return;
            }
        }

        ctx.toast ("Auf "_u + noteName (note) + " liegt kein Teil des Kits"_u);
        return;
    }

    if (auto* zone = const_cast<Zone*> (model.findZoneForNote (note, velocity)))
    {
        intoZone (ctx, *zone, sampleIndex);
        return;
    }

    const auto* sample = resolve (ctx, sampleIndex);

    if (sample == nullptr)
        return;

    ctx.step (sample->name + " zugeordnet");
    auto& zone = model.addZoneAt (note, velocity < 0 ? 64 : velocity);
    zone.name = sample->name.upToLastOccurrenceOf (".", false, false);
    model.addSampleTrack (zone, *sample);

    ctx.toast ("Neue Zone "_u + zone.name + " · "_u + noteName (zone.lowNote) + "–"_u + noteName (zone.highNote));
}

void intoDrumPart (StudioContext& ctx, const DrumPart& part, int sampleIndex)
{
    const auto* sample = resolve (ctx, sampleIndex);

    if (sample == nullptr)
        return;

    auto* zone = ctx.model.findZoneForDrumPart (part.id);
    const bool hadSamples = zone != nullptr && zone->numSamples() > 0;

    ctx.step (sample->name + " zugeordnet");

    auto& target = ctx.model.addDrumZone (part);
    ctx.model.selectZone (target.id);
    ctx.model.addSampleTrack (target, *sample);

    ctx.toast (sample->name + " → "_u + juce::String::fromUTF8 (part.name)
               + (hadSamples ? " · als weitere Spur"_u : juce::String()));
}
} // namespace sis::sampleDrop
