#include "PianoKeyboard.h"
#include "../SampleDrop.h"
#include "../Widgets.h"

namespace sis
{
namespace
{
    constexpr int midiChannel = 1;
    constexpr int numOctaves = InstrumentModel::numNotes / 12;
    constexpr int whiteOffsets[] = { 0, 2, 4, 5, 7, 9, 11 };

    // Halbton → Index der weißen Taste, rechts neben der die schwarze sitzt
    constexpr std::pair<int, int> blackKeys[] = { { 1, 1 }, { 3, 2 }, { 6, 4 }, { 8, 5 }, { 10, 6 } };
}

PianoKeyboard::PianoKeyboard (InstrumentModel& m, juce::MidiKeyboardState& state)
    : model (m), keyboardState (state)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    model.addChangeListener (this);
}

PianoKeyboard::~PianoKeyboard()
{
    model.removeChangeListener (this);

    if (soundingNote >= 0)
        keyboardState.noteOff (midiChannel, soundingNote, 0.0f);
}

void PianoKeyboard::resized()
{
    keys.clear();

    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    const float whiteWidth = area.getWidth() / (float) (numOctaves * 7);
    const float blackHeight = std::round (area.getHeight() * metrics::blackKeyRatio);

    for (int octave = 0; octave < numOctaves; ++octave)
        for (int i = 0; i < 7; ++i)
            keys.push_back ({ InstrumentModel::lowestNote + octave * 12 + whiteOffsets[i], false,
                              { area.getX() + (float) (octave * 7 + i) * whiteWidth, area.getY(),
                                whiteWidth, area.getHeight() } });

    for (int octave = 0; octave < numOctaves; ++octave)
        for (const auto& [semitone, whiteIndex] : blackKeys)
            keys.push_back ({ InstrumentModel::lowestNote + octave * 12 + semitone, true,
                              { area.getX() + (float) (octave * 7 + whiteIndex) * whiteWidth - whiteWidth * 0.31f,
                                area.getY(), whiteWidth * metrics::blackKeyWidth, blackHeight } });
}

void PianoKeyboard::paint (juce::Graphics& g)
{
    const auto* zone = model.getSelectedZone();
    auto inZone = [zone] (int note) { return zone != nullptr && zone->containsNote (note); };

    g.setColour (colours::surface);
    g.fillAll();

    const auto labelFont = monoFont (10.0f);

    for (const auto& key : keys)
    {
        if (key.black)
            continue;

        const bool hit = key.note == highlightedNote || key.note == dropNote;
        g.setColour (hit ? colours::accent : inZone (key.note) ? colours::accentSoft : colours::keyWhite);
        g.fillRect (key.bounds);

        g.setColour (colours::keyLine);
        g.fillRect (key.bounds.withLeft (key.bounds.getRight() - 1.0f));

        if (key.note % 12 == 0)
        {
            g.setColour (hit ? colours::white : colours::textTertiary);
            g.setFont (labelFont);
            g.drawText (noteName (key.note), key.bounds.withTrimmedBottom (4.0f),
                        juce::Justification::centredBottom, false);
        }
    }

    for (const auto& key : keys)
    {
        if (! key.black)
            continue;

        g.setColour (key.note == highlightedNote || key.note == dropNote ? colours::accent
                     : inZone (key.note)         ? colours::keyBlackInZone
                                                 : colours::keyBlack);
        g.fillRect (key.bounds);
    }

    g.setColour (colours::lineStrong);
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
}

const PianoKeyboard::Key* PianoKeyboard::keyAt (juce::Point<float> p) const
{
    // Schwarze Tasten liegen oben und werden zuerst geprüft
    for (auto it = keys.rbegin(); it != keys.rend(); ++it)
        if (it->bounds.contains (p))
            return &*it;
    return nullptr;
}

void PianoKeyboard::mouseDown (const juce::MouseEvent& e)
{
    const auto* key = keyAt (e.position);
    if (key == nullptr)
        return;

    const float depth = juce::jlimit (0.0f, 1.0f, (e.position.y - key->bounds.getY()) / key->bounds.getHeight());
    const float velocity = 0.25f + 0.75f * depth;

    if (soundingNote >= 0)
        keyboardState.noteOff (midiChannel, soundingNote, 0.0f);

    soundingNote = key->note;
    keyboardState.noteOn (midiChannel, soundingNote, velocity);

    highlightedNote = key->note;
    repaint();
    startTimer (metrics::keyHighlightMs);

    if (onHighlightChanged != nullptr)
        onHighlightChanged (highlightedNote, juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f)));
}

void PianoKeyboard::mouseUp (const juce::MouseEvent&)
{
    if (soundingNote >= 0)
    {
        keyboardState.noteOff (midiChannel, soundingNote, 0.0f);
        soundingNote = -1;
    }
}

void PianoKeyboard::timerCallback()
{
    stopTimer();
    highlightedNote = -1;
    repaint();

    if (onHighlightChanged != nullptr)
        onHighlightChanged (-1, 0);
}

bool PianoKeyboard::isInterestedInDragSource (const SourceDetails& details)
{
    return sampleDrop::indexOf (details) >= 0;
}

void PianoKeyboard::itemDragMove (const SourceDetails& details)
{
    const auto* key = keyAt (details.localPosition.toFloat());
    const int note = key != nullptr ? key->note : -1;

    if (note != dropNote)
    {
        dropNote = note;
        repaint();
    }
}

void PianoKeyboard::itemDragExit (const SourceDetails&)
{
    dropNote = -1;
    repaint();
}

void PianoKeyboard::itemDropped (const SourceDetails& details)
{
    dropNote = -1;
    repaint();

    if (const auto* key = keyAt (details.localPosition.toFloat()))
        if (onSampleDropped != nullptr)
            onSampleDropped (key->note, sampleDrop::indexOf (details));
}

void PianoKeyboard::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}
} // namespace sis
