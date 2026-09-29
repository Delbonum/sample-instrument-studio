#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

namespace sis::dsp
{
struct DelaySettings
{
    float timeMs = 350.0f;
    float feedback = 0.4f;    // 0 … 0.95
    float damping = 0.4f;     // 0 = helle Wiederholungen, 1 = dunkle
    float pingPong = 0.0f;    // 0 = beide Kanäle für sich, 1 = abwechselnd
    float mix = 0.35f;

    bool operator== (const DelaySettings& other) const noexcept
    {
        return juce::approximatelyEqual (timeMs, other.timeMs)
            && juce::approximatelyEqual (feedback, other.feedback)
            && juce::approximatelyEqual (damping, other.damping)
            && juce::approximatelyEqual (pingPong, other.pingPong)
            && juce::approximatelyEqual (mix, other.mix);
    }

    bool operator!= (const DelaySettings& other) const noexcept { return ! (*this == other); }
};

/** Echo mit Rückkopplung, Dämpfung und Ping-Pong.

    Im Kern dieselbe Verzögerungsleitung wie bei Flanger und Chorus, nur viel länger –
    und genau daran hängt die einzige unangenehme Entscheidung: der Speicher.

    **Warum höchstens eine Sekunde.** Die Bausteine der 32 Signalwege entstehen alle in
    `prepare`, weil im Audio-Thread nichts angefordert werden darf. Eine Leitung kostet
    `Zeit × Abtastrate × 2 Kanäle × 4 Byte`; bei einer Sekunde und 48 kHz sind das 384 kB je
    Signalweg, also rund 12 MB für alle 32 – auch für die, die nie ein Delay benutzen.
    Bei zwei Sekunden wären es 24 MB. Eine Sekunde deckt musikalisch das meiste ab (eine
    Viertelnote bei 60 bpm), und die Grenze bleibt vorhersehbar.

    Wer längere Echos braucht, müsste die Bausteine je Signalweg **erst bei Bedarf** anlegen.
    Das geht nicht nebenbei: `BusProcessors` liegt in der Engine und wird vom Audio-Thread
    gelesen, während der Message-Thread den nächsten Plan baut – die Leitung dort zu
    vergrößern wäre ein Datenrennen. Sauber wäre, den Zustand je Signalweg mit dem Plan zu
    tauschen statt in der Engine zu halten.

    **Die Dämpfung sitzt in der Rückkopplung**, nicht am Ausgang: so wird jede Wiederholung
    eine Stufe dunkler als die davor, statt alle gleich dumpf zu klingen. Das ist der
    Unterschied zwischen einem Bandecho und einem Tiefpass hinter dem Delay.

    **Die Verzögerungszeit wird geglättet.** Ein Sprung im Leseabstand knackt; langsam
    nachgeführt entsteht stattdessen das Tonhöhenziehen, das man von Bandmaschinen kennt.
    Nach einem Reset wird die Zeit allerdings übernommen und nicht angefahren – beim Laden
    eines Instruments soll die erste Wiederholung sofort richtig sitzen. */
class Delay
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double maxTimeSeconds = 1.0;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const DelaySettings&) noexcept;
    DelaySettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    float readInterpolated (int channel, double delaySamples) const noexcept;

    DelaySettings settings;
    double currentSampleRate = 44100.0;

    std::vector<std::vector<float>> lines;
    std::array<float, maxChannels> dampingState {};
    int lineLength = 0;
    int writeIndex = 0;

    double targetDelaySamples = 0.0;
    double currentDelaySamples = 0.0;

    /* Nach einem Reset wird die Zeit übernommen statt angefahren: sonst glitte das
       Delay beim Laden eines Instruments von der Voreinstellung zur eingestellten
       Zeit, und die erste Wiederholung käme verschmiert. Erst danach zieht es. */
    bool snapToTarget = true;
    float dampingCoefficient = 1.0f;
    float smoothingCoefficient = 0.0005f;
};
} // namespace sis::dsp
