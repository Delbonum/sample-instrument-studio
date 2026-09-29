#pragma once

#include <JuceHeader.h>
#include <array>

namespace sis::dsp
{
/** Welche Kennlinie gefahren wird. */
enum class DriveMode
{
    overdrive,    // weiches, unsymmetrisches Anzerren – erzeugt auch geradzahlige Obertöne
    distortion    // von weich nach hart übersteuert, bis zum glatten Abschneiden
};

struct DriveSettings
{
    DriveMode mode = DriveMode::overdrive;
    float driveDb = 12.0f;
    float character = 0.0f;   // Overdrive: Unsymmetrie 0 … 1 · Distortion: Härte der Kante 0 … 1
    float toneHz = 12000.0f;  // Tiefpass hinter der Kennlinie
    float mix = 1.0f;
    float outputDb = 0.0f;

    bool operator== (const DriveSettings& other) const noexcept
    {
        return mode == other.mode
            && juce::approximatelyEqual (driveDb, other.driveDb)
            && juce::approximatelyEqual (character, other.character)
            && juce::approximatelyEqual (toneHz, other.toneHz)
            && juce::approximatelyEqual (mix, other.mix)
            && juce::approximatelyEqual (outputDb, other.outputDb);
    }

    bool operator!= (const DriveSettings& other) const noexcept { return ! (*this == other); }
};

/** Verzerrung über eine Kennlinie, in zwei Ausprägungen.

    **Overdrive** benutzt den kubischen Weichbegrenzer und verschiebt das Signal **vor** der
    Verstärkung um einen kleinen Betrag. Diese Unsymmetrie ist der eigentliche Punkt: eine
    symmetrische Kennlinie erzeugt nur ungeradzahlige Obertöne, eine unsymmetrische auch
    geradzahlige – das ist der Unterschied zwischen „scharf“ und „warm“.

    Der Versatz sitzt bewusst **vor** der Verstärkung. Dadurch wandern die Nulldurchgänge,
    die positive Halbwelle wird breiter als die negative, und dieses schiefe Tastverhältnis
    bleibt auch bei voller Übersteuerung erhalten. Säße der Versatz dahinter, wäre er bei
    hoher Verstärkung neben dem Nutzsignal bedeutungslos; und die negative Halbwelle bloß
    herunterzuskalieren hilft gar nicht – ein Rechteck mit halbierter Unterseite ist nach
    dem Gleichanteil-Sperrfilter wieder symmetrisch.

    Den Gleichanteil, den die Unsymmetrie erzeugt, nimmt der Sperrfilter hinter der
    Kennlinie weg – sonst wanderte die Nulllinie und mit ihr die Aussteuerungsreserve.

    **Distortion** blendet vom Tangens hyperbolicus zum harten Abschneiden über. Bei voller
    Kante bleibt vom Sinus ein Rechteck übrig.

    Zuletzt ein einpoliger Tiefpass als Klangregler. Der ist nicht nur Geschmack: **es wird
    nicht überabgetastet**, und die Kennlinie erzeugt Obertöne weit über der halben
    Abtastrate, die sich als Aliasing zurückfalten. Der Tiefpass nimmt den obersten Teil
    davon weg. Ganz sauber ginge es nur mit Überabtastung – die kostet hier über 32
    Signalwege Speicher und brächte Latenz zwischen den Spuren mit. */
class Drive
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSettings (const DriveSettings&) noexcept;
    DriveSettings getSettings() const noexcept { return settings; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Der kubische Weichbegrenzer für sich – begrenzt auf ±1. */
    static float softClip (float x) noexcept;

private:
    float shape (float driven) const noexcept;
    void updateCoefficients() noexcept;

    DriveSettings settings;
    double currentSampleRate = 44100.0;

    float drive = 4.0f;
    float inputOffset = 0.0f;     // Versatz vor der Verstärkung (nur Overdrive)
    float outputGain = 1.0f;

    float toneCoefficient = 1.0f;
    float dcCoefficient = 0.998f;

    std::array<float, maxChannels> toneState {};
    std::array<float, maxChannels> dcLastInput {};
    std::array<float, maxChannels> dcLastOutput {};
};
} // namespace sis::dsp
