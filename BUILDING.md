# Sample Instrument Studio – Bauen

C++20 mit [JUCE 8](https://juce.com) (wird von CMake automatisch geladen).
Aus einer Codebasis entstehen **VST3**, **Standalone** und auf macOS zusätzlich **AU**.

## Voraussetzungen

**Windows**
- Visual Studio (2022 oder neuer, auch Insiders) bzw. die *Build Tools* mit dem Workload
  „Desktopentwicklung mit C++“
- CMake ≥ 3.22 (in Visual Studio enthalten, sonst `winget install Kitware.CMake`)
- Git (für den automatischen JUCE-Download)

**macOS**: Xcode ≥ 15, CMake ≥ 3.22.

## Bauen unter Windows

Am einfachsten über das mitgelieferte Skript. Es sucht Visual Studio selbst und setzt die
Compiler-Umgebung, sodass weder PATH-Einträge noch eine Developer-Shell nötig sind:

```powershell
.\build.ps1                    # Standalone, Debug
.\build.ps1 -Run               # bauen und starten
.\build.ps1 -Config Release    # alle Formate
.\build.ps1 -Target SisClipGeometryTests -Run   # Tests
```

**Wichtig:** `cmake` allein in einer normalen PowerShell genügt nicht — ohne die
Compiler-Umgebung meldet CMake „No CMAKE_CXX_COMPILER could be found“. Wer ohne das Skript
arbeiten will, startet die **Developer PowerShell für Visual Studio** und nutzt dort die Presets:

```powershell
cmake --preset ninja-debug
cmake --build --preset standalone-debug
ctest --preset debug
```

Die Ergebnisse liegen unter
`build/ninja-debug/SampleInstrumentStudio_artefacts/Debug/Standalone|VST3/`.

Mit einem vorhandenen JUCE-Checkout: `-DSIS_JUCE_PATH=C:/pfad/zu/JUCE`. Der Pfad sollte kurz
sein — JUCEs mitgelieferter FLAC-Code überschreitet sonst die Windows-Grenze von 260 Zeichen.

In CLion, Visual Studio oder VS Code (Erweiterungen *C/C++* und *CMake Tools*) lässt sich der
Ordner direkt als CMake-Projekt öffnen. Für den Bau ist die IDE aber unerheblich: entscheidend
ist das oben genannte C++-Toolset.

## Tests

```powershell
ctest --test-dir build
inja-debug --output-on-failure
```

- `tests/ClipGeometryTests.cpp` prüft die Clip-Gesten des Editors (Verschieben, Rasten,
  Zuschneiden, Stretchen, Fades) an der reinen Geometrie in `Source/Model/ClipGeometry.h` —
  ohne JUCE und ohne Fenster.
- `tests/EngineTests.cpp` rendert echte Audioblöcke: Tonhöhe, Velocity, Panorama, Fades,
  Ausschnitt, One-Shot gegen Loop, Release, Spur-Versatz, Velocity-Zonen, Polyphonie,
  Laden einer WAV-Datei und die Kette Klaviatur → MIDI → Engine. Kein Audiogerät nötig.

## Aufbau

```
Source/
  PluginProcessor.*       Audio-Prozessor (Zustand, Master-Parameter, MIDI; Engine folgt)
  PluginEditor.*          Weiche: volles Studio (Standalone) oder kompaktes Plugin-Fenster
  StandaloneApp.cpp       eigenes Standalone-Fenster ohne native Titelleiste (Windows/Linux)
  Model/                  Datenmodell: Zonen → Spuren → Effekte, Samples, ADSR, Makros, .sisp (XML)
                          ClipGeometry.h: Clip-Gesten als reine Funktionen (getestet)
  Audio/                  SampleCache (Audiodaten), RenderPlan (Momentaufnahme fürs Abspielen),
                          SamplerEngine (Stimmen, Hüllkurve, Layer, Time-Stretch)
  DSP/                    Equalizer (vier Biquad-Bänder) und Kompressor; Hall über juce::Reverb
  Effects/                SIS Equalizer: eigenständiges Plugin (Prozessor und Oberfläche)
  Export/                 InstrumentExporter: schreibt Projekt und Samples im Hintergrund
  UI/Theme.h              Design-Tokens (Farben, Maße) aus dem README
  UI/Assets.*             eingebettete IBM-Plex-Schriften und Programm-Icon
  UI/StudioLookAndFeel.*  flache Optik, Menüs, Textfelder, Scrollbalken
  UI/Widgets.*            FlatButton, ValueBar (Schieber), Zeichenhilfen
  UI/Shell/               Fenster-, Menü-, Werkzeug- und Statusleiste, Einblendung, Befehle
  UI/Panels/              Sample-Browser (links), Zone/Hüllkurve/Makros (rechts)
  UI/Mapping/             Zonenraster und Klaviatur
  UI/Editor/              Spuransicht, Clips und Sample-Editor
  UI/Export/              Export-Ansicht
  UI/Plugin/              Plugin-Oberfläche (erster Stand)
tests/                    Tests der Clip-Geometrie
assets/                   Icon, Schriften (OFL)
```

## Stand

| Bereich | Status |
| --- | --- |
| Build-Setup (CMake, VST3/AU/Standalone) | fertig |
| Datenmodell inkl. Speichern/Laden `.sisp` | fertig – Spuren liegen an der Zone |
| Rahmen: Fensterleiste, Menüs, Werkzeugleiste, Statusleiste, Einblendung, Tastenkürzel | fertig |
| Mapping: Zonenraster, Velocity-Achse, Klaviatur, Zone hinzufügen | fertig |
| Sample-Browser mit Suche, Drag & Drop und Import (liest Länge, Bittiefe, Wellenform) | fertig |
| Rechte Spalte: Zone, Hüllkurve (ADSR), Makros | fertig |
| Editor: Spuren, Clips, Trim/Stretch/Fades, Sample-Editor, Effektkette, Spur-Parameter | fertig |
| Export-Ansicht: Kennzahlen, Zielordner, Fortschritt; schreibt Projektdatei und Samples | fertig |
| Export als VST3-Instrument (spielbar in der DAW, ohne das Studio) | fertig |
| Export als Standalone-App | fertig |
| Export als Audio Unit | entfällt ohne Mac-Fassung; Zeile aus der Export-Ansicht entfernt |
| Credits (Hilfe → Credits) und zentral gepflegte Version | fertig |
| Plugin-Ansicht (Makro-Drehregler, Presets, Zonen-Dialog) | Grundgerüst |
| Sampler-Engine: Samples laden, Zonen/Velocity, Layer mit Versatz, Tonhöhe, Trim, Fades, Loop-Modi, Panorama, ADSR, Polyphonie (24 Stimmen) | fertig |
| Time-Stretch: Überlappungsverfahren, Dauer ändert sich ohne Tonhöhenänderung | fertig |
| Wiedergabe der Zone über Play / Leertaste, Ausgangspegel-Anzeige, Audio-Einstellungen (F12) | fertig |
| Zonen-Bereiche und Velocity in der rechten Spalte einstellbar | fertig |
| Equalizer: vier Bänder, in der Effektkette der Spur und als eigenes Plugin | fertig |
| Kompressor (gekoppelte Stereo-Regelung) und Hall in der Effektkette | fertig |
| Fremde VST3-Effekte hosten (suchen, einfügen, eigene Oberfläche, Zustand im Projekt) | fertig |
| Bounce: Zone samt Effekten in eine Audiodatei rechnen | fertig |
| Kanalfilter, Sättigung, Transienten, Chorus, Bit-Crusher | fertig |
| Overdrive und Distortion | fertig |
| Flanger, Phaser, Vibrato, Tremolo | fertig |
| Delay | fertig |
| Noise Gate, Expander, Limiter, De-Esser | fertig |
| Envelope Filter und Auto-Wah | fertig |
| Kanal-Streifen | fertig |
| Presets je Effektart | fertig (ohne mitgelieferte Klangvorlagen) |
| Undo-Verlauf (Strg+Z / Strg+Y, benannte Schritte) | fertig |

## Wie die Wiedergabe aufgebaut ist

Der Audio-Thread fasst das Datenmodell nie an. Bei jeder Änderung baut der Message-Thread
eine Momentaufnahme (`RenderPlan`): Mute/Solo, Trim, Fades, Panorama und Tonhöhe sind darin
bereits aufgelöst, Spuren ohne geladenes Sample fallen weg. Der Plan wird über eine
Spin-Lock-Übergabe veröffentlicht; klingende Stimmen halten ihren alten Plan selbst am Leben,
und freigegeben wird er ausschließlich auf dem Message-Thread (Timer im Prozessor).

Eine Note startet eine Stimme, die alle Spuren ihrer Zone übereinanderlegt. Eine Spur trägt
beliebig viele Clips, jeder mit seinem Versatz auf der Zeitachse, seinem Ausschnitt und seinen
Fades; die Tonhöhe (Abstand zum Grundton + Spur-Halbtöne + Cent) gilt für die ganze Spur.
Überlappen sich Clips, klingt der obere – außer in seinen Fades. Der Plan zerlegt jeden Clip
deshalb in die Abschnitte, in denen er zu hören ist (`geometry::audibleSegments`), und legt
je Abschnitt eine Schicht mit `gateStart/EndSeconds` an; an inneren Kanten wird 3 ms
geblendet, damit nichts knackt. Eine Schleife klingt bis zum nächsten Clip der Spur. Die Hüllkurve des Instruments gilt für die
ganze Stimme. Die Anschlagstärke wählt die Zonen und skaliert den Pegel: **alle** passenden
Zonen klingen, jede in einer eigenen Stimme. Überschneiden sie sich im Velocity-Bereich,
gewichtet `velocityWeight` (`RenderPlan.h`) sie mit gleicher Leistung – die untere blendet
über den gemeinsamen Bereich aus, die obere ein; liegt eine ganz in der anderen, klingen beide
voll. Zonen ohne Spuren zählen dabei nicht mit.

**Time-Stretch:** Bei Stretch 1,00× wird direkt abgespielt. Sonst legt die Engine überlappende
Körner (halbe Kornlänge Versatz, Hann-Fenster) aneinander: der Lesezeiger innerhalb eines Korns
folgt der Tonhöhe, die Körner selbst rücken langsamer oder schneller vor. Dauer und Tonhöhe sind
damit unabhängig. Dasselbe Verfahren transponiert mit **gehaltenem Tempo** (`Track::keepTempo`):
dann rückt die Zeit nur mit der Samplerate des Samples vor, die Tonhöhe übernehmen die Körner.
Direkt gelesen wird, sobald Zeit- und Tonhöhenfortschritt gleich sind. Die vier Algorithmen aus dem README wählen bisher nur die Körnung
(768 … 6144 Samples); eine echte Transienten-Erkennung bzw. ein Phasenvokoder fehlt noch, bei
starker Dehnung tonalen Materials hört man das typische Schwimmen.

**Wiedergabe (Play / Leertaste):** spielt die gewählte Zone auf ihrem Grundton an — die
Clip-Versätze ergeben den zeitlichen Aufbau. Gespielt wird ab dem Locator: der Prozessor meldet
der Engine vorher den Versatz (`SamplerEngine::setStartOffset`), und der nächste Anschlag genau
dieser Note überspringt, was davor liegt. LOOP wiederholt ab dem Locator, nach mindestens
8 Sekunden oder am Ende des letzten Clips. Die Laufmarke
im Editor folgt einem Timer der Oberfläche, nicht der Audio-Uhr; bei langen Tönen können
Bild und Ton um einige Millisekunden auseinanderlaufen.

**Kein Ton?** Die Statusleiste zeigt rechts Gerät, Puffer, belegten Speicher, Stimmenzahl und CPU;
ein Klick darauf (oder **F12**, oder Datei → Audio-Einstellungen) öffnet die Geräteauswahl mit
Prüfton. Der Pegelbalken unter dem Master-Regler zeigt, ob überhaupt Signal entsteht. Steht links
in der Statusleiste „keine Audiodaten", hat die Zone noch kein Sample: Datei in die SAMPLES-Spalte
ziehen und doppelt anklicken.

## Stolperstein: Tastenkürzel unter Windows

Zeichen-Tasten wie die Leertaste erreichen ein JUCE-Fenster zweimal — einmal als `WM_KEYDOWN`
und einmal als `WM_CHAR`. Beide Wege landen in der Befehlszuordnung, sodass ein Druck auf die
Leertaste die Wiedergabe startete **und sofort wieder stoppte**. `StudioShell::perform` verwirft
deshalb einen Befehl, der innerhalb von 150 ms erneut von der Tastatur kommt. Menü- und
Knopfaufrufe sind davon nicht betroffen.

## Was der Export heute tut

Geschrieben wird ein **Instrument-Ordner**:

```
<Zielordner>/
  <Name>.sisp        Projektdatei (XML)
  Samples/           Kopien der verwendeten Audiodateien
```

Die Sample-Pfade in der Projektdatei stehen relativ zum Ordner, solange die Dateien darin
liegen. Der Ordner lässt sich also kopieren oder verschieben und anderswo wieder öffnen –
`tests/ProcessorTests.cpp` prüft genau das, inklusive Umzug.

Die Dateiarbeit läuft in einem eigenen Thread (`InstrumentExporter`), der Fortschrittsbalken
zeigt den echten Stand; die Logik selbst ist als `performExport` ohne Thread aufrufbar und
darüber getestet.

### VST3-Instrument

Ist **VST3-Instrument** angehakt, entsteht zusätzlich ein fertiges Plugin:

```
<Zielordner>/<Name>.vst3/
  Contents/x86_64-win/Sample Instrument Studio.vst3   Kopie der Plugin-Vorlage
  Contents/Resources/moduleinfo.json
  Contents/Resources/Instrument.sisp                  das exportierte Instrument
  Contents/Resources/Samples/                         die verwendeten Audiodateien
```

Das Plugin sucht beim Start `Instrument.sisp` in seinen eigenen Resources und lädt es mitsamt
Samples (`StudioProcessor::findBundledInstrument`). Es spielt also ohne das Studio und ohne
die Originaldateien. Hat der Host einen eigenen Zustand gespeichert, gewinnt dieser.

Als Vorlage dient das mitgebaute VST3. Gesucht wird es neben der App, im Build-Ordner
(`.../Debug/VST3/`) und im gemeinsamen VST3-Ordner. Fehlt es, bleibt die Zeile ausgegraut.

`tests/ProcessorTests.cpp` exportiert ein Instrument, lädt das Ergebnis mit JUCEs VST3-Host
(wie eine DAW), schickt eine Note hinein und prüft, dass Signal herauskommt.

### Eigene Kennung je Instrument

Jedes exportierte Instrument erscheint in der DAW als **eigenes Plugin** mit eigenem Namen.
Möglich wird das so:

- `Source/PluginIdentity.*` hält einen Datenblock im Binary:
  `SIS-IDENTITY-v1|<4-stelliger Code>|<Name>`. Zwei Funktionen lesen ihn zur Laufzeit.
- CMake setzt `JucePlugin_Name` und `JucePlugin_PluginCode` auf diese Funktionen. Für VST3
  wertet JUCE beide nur zur Laufzeit aus (`juce_VST3ModuleInfo.h`), die Kennung entsteht aus
  Hersteller- und Plugin-Code. JUCEs Manifest-Helfer linkt unsere Quellen nicht und bekommt
  deshalb über `SIS_PLUGIN_NAME` / `SIS_PLUGIN_CODE` feste Rückfallwerte.
- Beim Export ersetzt `Source/Export/PluginIdentityPatch.cpp` den Block im kopierten Binary
  (nur diese 192 Bytes, an Ort und Stelle), zieht `moduleinfo.json` nach und benennt das Modul
  im Bundle um. Der Code entsteht aus dem Instrumentnamen, gleicher Name ergibt also immer
  dieselbe Kennung – ein erneuter Export ersetzt das Plugin, statt ein zweites anzulegen.

`tests/ProcessorTests.cpp` exportiert zwei Instrumente, lädt beide mit JUCEs VST3-Host und
prüft: verschiedene Kennungen, je eigener Name, und beide klingen.

**Grenzen:** Der Name darf rund 170 Zeichen nicht überschreiten (Größe des Blocks). Die Kennung
ist ein Hash aus vier Zeichen – theoretisch sind Kollisionen möglich, praktisch unwahrscheinlich.
Signierte Binärdateien würden durch das Patchen ihre Signatur verlieren; unter Windows ist das
für VST3 unkritisch, für einen späteren macOS-Build müsste nach dem Patchen neu signiert werden.

**So kommt das Plugin in die DAW:** den exportierten Ordner `<Name>.vst3` nach
`C:\Program Files\Common Files\VST3` kopieren (oder einen eigenen VST3-Pfad in der DAW
eintragen) und neu scannen lassen.

**Noch nicht möglich:** Audio Unit (braucht einen macOS-Build) und Standalone-App.

## Effekte

### Equalizer

`Source/DSP/Equalizer.*` ist die gemeinsame Grundlage: vier Bänder als Biquad-Filter
(Kuhschwanz tief, Glocke, Kuhschwanz hoch, Hoch- und Tiefpass), jeweils mit Frequenz,
Anhebung und Güte. `getMagnitudeAt` liefert den Frequenzgang für die Kurve.

**In der Spur:** Im Editor legt „+ → EQ 4-Band" einen Equalizer an. Dort steuern vier Regler
die Anhebung fester Bänder (120 Hz, 500 Hz, 2,5 kHz, 8 kHz) zwischen −12 und +12 dB.

**Als eigenes Plugin:** Das Ziel `SisEqualizer` baut „SIS Equalizer" als VST3 und als
Standalone-Programm — mit allen Parametern, Frequenzgang-Kurve und Aussteuerungsanzeige.
Es lässt sich in jeder DAW verwenden, unabhängig vom Studio:

```powershell
.\build.ps1 -Target SisEqualizer_VST3        # build/ninja-debug/SisEqualizer_artefacts/…/VST3/
.\build.ps1 -Target SisEqualizer_Standalone -Run
```

### Kompressor

`Source/DSP/Compressor.*`: Spitzenwert-Detektor mit getrennten Zeiten für Ansprechen und
Loslassen, **gekoppelte Stereo-Regelung** (beide Kanäle bekommen dieselbe Verstärkung, sonst
wandert das Stereobild), harte Kennlinie und Ausgleichsverstärkung. `getGainReductionDb()`
meldet die stärkste Absenkung eines Blocks für eine spätere Anzeige.

Die fünf Regler der Spur bilden ab: Schwelle −48 … 0 dB, Verhältnis 1:1 … 20:1 (unten fein
aufgelöst), Ansprechen 1 … 200 ms, Loslassen 20 … 1000 ms, Ausgleich 0 … 24 dB.

Gemessen geprüft: Signal unter der Schwelle bleibt unverändert; bei 4:1 werden aus 16 dB
Überschuss genau 4 dB; der Ausgleich hebt um den eingestellten Betrag an; langsames Ansprechen
lässt den Anfang durch und regelt danach.

### Hall

Freeverb über `juce::Reverb`, je Spur eine Instanz. Die vier Regler sind Größe, Dämpfung,
Breite und Hallanteil (dieser blendet zwischen trocken und nass über). Geprüft wird, dass nach
dem Ende eines kurzen Samples ohne Hall Stille herrscht und mit Hall messbar etwas nachklingt.

### Wie Effekte im Instrument laufen

Damit ein Effekt auf eine *Spur* wirkt und nicht auf eine einzelne Note, hat jede Spur einen
eigenen Stereo-Signalweg. Die Stimmen schreiben ihre Schichten dorthin, danach läuft die
Effektkette der Spur, zuletzt wird alles zusammengemischt.

Pro Spur wirkt **je ein Effekt jeder Art**, und zwar in der Reihenfolge, in der sie in der
Kette stehen. Ein zweiter Effekt derselben Art wird übergangen – das steht so im Renderplan
(`TrackEffectsPlan::steps`) und hält den Speicher klein, weil die Engine je Spur genau einen
Equalizer, einen Kompressor und einen Hall vorhält.

Filter-Koeffizienten entstehen **beim Bau des Renderplans** auf dem Message-Thread; der
Audio-Thread übernimmt sie nur noch als fertige Zeiger. Deshalb baut auch `prepareToPlay` den
Plan neu — die Koeffizienten hängen an der Samplerate. Blöcke, die größer sind als beim
Vorbereiten angekündigt, werden in Abschnitten verarbeitet, damit im Audio-Thread nichts
angefordert werden muss.

Geprüft wird das in `tests/EngineTests.cpp`: einzelne Bänder gegen gemessene Pegel
(+12 dB vervierfachen den Pegel, Nachbarfrequenzen bleiben stehen, ausgeschaltete Bänder
tun nichts) und der Equalizer durch die ganze Engine hindurch (angehobene Mitten sind
lauter, abgesenkte leiser, ein ausgeschalteter Effekt verändert nichts).

### Fremde Plugins hosten

Neben den eigenen Effekten lassen sich installierte **VST3-Effekte** in die Kette einer Spur
hängen — über „+ → Externes Plugin …“. Der Dialog (`Source/UI/Panels/PluginChooser.*`) durch-
sucht die Standard-Ordner im Hintergrund oder nimmt eine einzelne `.vst3`-Datei entgegen. Die
gefundenen Plugins landen in `%APPDATA%\Sample Instrument Studio\plugins.xml`, damit beim
nächsten Start nicht neu gesucht werden muss. Der Knopf „Öffnen“ auf der Effektkarte zeigt die
Oberfläche des Plugins in einem eigenen Fenster; Plugins ohne eigene Oberfläche bekommen
JUCEs allgemeine Reglerliste.

Instrumente (VSTi) werden bewusst **nicht** angeboten — das Studio ist selbst eines.

Die Instanzen verwaltet der Prozessor, nicht der Renderplan: `hostedPlugins` hält je
Zone/Spur/Kennung eine Instanz, die über Planwechsel hinweg bestehen bleibt, damit ein Plugin
nicht bei jeder Reglerbewegung neu geladen wird. Der Renderplan bekommt nur einen gezählten
Zeiger darauf, sodass ein Plan, der noch klingt, sein Plugin am Leben hält. Freigegeben wird
erst auf dem Message-Thread über die Liste `retiredPlugins` — nie im Audio-Thread.
`HostedPlugin::process` legt dabei nur einen `AudioBuffer` **um den vorhandenen Speicher**,
fordert also nichts an.

Der Zustand eines Plugins (sein `getStateInformation`, base64-kodiert) wandert beim Speichern
ins Projekt. Er wird außerdem gesichert, *bevor* eine Instanz ausrangiert wird — sonst käme ein
kurz abgeschalteter Effekt mit Werkseinstellungen zurück.

**Beim Export gilt:** fremde Plugins stecken nicht im Bundle. Auf einem Rechner ohne diese
Plugins bleiben ihre Effekte stumm; die Export-Ansicht weist darauf hin und verweist auf den
Bounce, der sie fest ins Instrument rechnet.

Geprüft wird das in `tests/ProcessorTests.cpp` mit dem selbst gebauten *SIS Equalizer* als
Versuchskaninchen: Er wird wie ein fremdes Plugin in die Liste aufgenommen, geladen, sein
Ausgang zugedreht — und der Prozessor rendert daraufhin leiser. Abschalten rangiert die
Instanz aus, Wiedereinschalten stellt die Reglerstellung wieder her, und der Zustand landet
im Projekt. `tests/EngineTests.cpp` prüft zusätzlich, dass Kennung und Zustand das Speichern
und Laden überstehen.

### Bounce

„Zone bouncen“ im Editor rendert eine Zone samt aller Effektketten in eine WAV-Datei und
ersetzt ihre Spuren durch diese eine. Damit stecken auch fremde Plugins fest im Instrument
und überleben den Export. Die Datei landet neben der Projektdatei in `Bounces/`, sonst unter
`Dokumente\Sample Instrument Studio\Bounces`.

Gerendert wird in `Source/Audio/Bounce.*`, schneller als in Echtzeit auf dem Message-Thread:
ein eigener `SamplerEngine`, ein eigenes Instrument mit nur dieser Zone über die ganze
Klaviatur, und eine Probenote auf dem Grundton.

Zwei Dinge bleiben bewusst **draußen**, weil sie beim Spielen weiterhin wirken und sonst
doppelt im Klang steckten:

- die **Hüllkurve** des Instruments (der Plan bekommt eine neutrale ADSR; nur ein 20-ms-Release
  verhindert den Knacks, wenn eine Schleife abgeschnitten wird),
- der **Master-Pegel** — der sitzt im Prozessor, nicht in der Engine, und bleibt damit von
  selbst außen vor.

Alles andere wandert hinein: Spur-Pegel, Panorama, Tonhöhe, Trim, Fades, Zeitdehnung und die
gesamte Effektkette. Die neue Spur steht deshalb auf Pegel 1,0, Panorama Mitte und ohne
Effekte.

Die Länge ergibt sich aus der Zeitachse der Zone (Ende des letzten Clips, höchstens 128 s), dazu zwei
Sekunden Nachklang für Hall und fremde Plugins; hinten wird die Stille wieder abgeschnitten.
Am Ende des Körpers wird die Note losgelassen, damit Schleifen aufhören und nur noch der
Nachklang stehen bleibt.

Fremde Plugins bekommen für den Bounce **eigene Instanzen** — die des laufenden Instruments
gehören dem Audio-Thread, und der darf nicht gleichzeitig darin rechnen. Ihre Einstellungen
werden vorher über `captureHostedPluginStates` gesichert und in die frischen Instanzen geladen.

Rückgängig machen lässt sich ein Bounce nicht (der Verlauf fehlt noch), deshalb fragt die
Oberfläche vorher nach.

Geprüft wird das in `tests/ProcessorTests.cpp`: eine Spur mit dem gehosteten *SIS Equalizer*,
dessen Ausgang auf -24 dB steht, wird gebounct. Danach hat die Zone genau eine Spur **ohne**
Effektkette, das Plugin wird nicht mehr gehalten — und sie klingt trotzdem weiter so leise
wie vorher. Die Dämpfung steckt also im Sample.

## Undo-Verlauf

`Strg+Z` und `Strg+Y`; im Menü *Bearbeiten* steht der Name des Schritts dabei
(„Rückgängig: Pegel geändert“), und was nicht geht, ist ausgegraut.

Der Verlauf (`Source/Model/UndoHistory.*`) arbeitet mit **Schnappschüssen des ganzen
Modells**, nicht mit einzelnen Befehlen. Das passt hier, weil sich das Instrument ohnehin
vollständig als Baum schreiben lässt — die Audiodaten stehen als Pfad darin, nicht als
Inhalt, ein Schnappschuss ist also nur ein paar Kilobyte groß. 60 Schritte werden behalten.

Entscheidend ist, **woran** er hängt: am Änderungssignal des Modells, nicht an den einzelnen
Aufrufern. Damit ist jede Änderung erfasst, auch eine, die niemand angemeldet hat — es gibt
keine Stelle, die man zu ändern vergessen kann und die dann still verloren ginge. Wer vorher
`ctx.step ("Spur angelegt")` aufruft, bekommt nur einen besseren Namen im Menü; ohne das
heißt der Schritt „Änderung“.

Drei Feinheiten, ohne die es im Alltag unbrauchbar wäre:

- **Zusammenfassen.** Ein Schritt wird erst 500 ms nach der letzten Änderung festgehalten,
  sonst hinterließe ein Reglerzug hundert Schritte. Ein *anderer* Name schließt einen offenen
  Schritt sofort ab, damit zwei verschiedene Handgriffe nicht verschmelzen.
- **Auswahl zählt nicht.** Eine Zone anzuklicken ändert das Modell (`selectedZone`), ist aber
  kein Schritt — verglichen wird über `withoutSelection`, sonst füllte bloßes Herumklicken
  den Verlauf.
- **Kein Geisterschritt.** Ein Zurücknehmen meldet selbst wieder eine Änderung. Weil der
  Vergleich sie gegen den gerade wiederhergestellten Stand hält, entsteht daraus nichts —
  das setzt voraus, dass `toValueTree`/`fromValueTree` verlustfrei hin und zurück läuft, und
  genau das prüft der Test.

Nicht im Verlauf: was ein **fremdes Plugin** intern an seinen Reglern ändert. Das steht nicht
im Modell und löst kein Änderungssignal aus, ist also auch vorher nie undo-fähig gewesen.
Laden, Neu anlegen und ein vom Host gesetzter Zustand **setzen den Verlauf zurück**.

Geprüft wird das in `tests/ProcessorTests.cpp`: Schritt anlegen, zurücknehmen, wiederholen,
der Name am Schritt, Auswahl erzeugt keinen Schritt, zwanzig Änderungen desselben Namens
bleiben ein Schritt, eine neue Änderung verwirft den Wiederherstellen-Zweig — und der
Geisterschritt-Test oben.

### Die übrigen internen Effekte

Damit klingen alle acht Einträge im „+“-Menü, keiner ist mehr Platzhalter:

| Effekt | Was er tut | Bausteine |
| --- | --- | --- |
| Kanalfilter | Hoch- und Tiefpass, um eine Spur im Mix freizustellen | `dsp::Equalizer` mit zwei Bändern |
| Sättigung | weiche Verformung über eine Tangens-Kennlinie | `dsp::Saturator` |
| Transienten | Anschlag und Ausklingen getrennt formen | `dsp::TransientShaper` |
| Chorus | modulierte Verzögerung, zwei Kanäle versetzt | `dsp::Chorus` |
| Bit-Crusher | gröbere Auflösung und gröbere Abtastung | `dsp::BitCrusher` |

Drei Entscheidungen, die den Unterschied machen:

- Der **Kanalfilter bekam keine eigene Klasse.** `dsp::Equalizer` kann Hoch- und Tiefpass
  längst, samt der Disziplin, Koeffizienten außerhalb des Audio-Threads zu rechnen. Er läuft
  darum als zweite Equalizer-Instanz je Signalweg mit zwei belegten Bändern.
- Die **Sättigung normiert ihre Kennlinie auf sich selbst** (`tanh(g·x) / tanh(g)`). Sonst wäre
  jede Drehung am Regler vor allem eine Lautstärkeänderung, und man hörte nicht, was der
  Effekt tut.
- Der **Transienten-Former begrenzt auf ±18 dB.** Er arbeitet mit dem Abstand zweier
  Hüllkurvenfolger; bei sehr leisen Stellen wird deren Quotient beliebig groß, und ohne
  Begrenzung entstünden dort Ausbrüche.

Die Zustände liegen jetzt als **ein Satz je Signalweg** in `BusProcessors` statt als acht
Arrays nebeneinander — bei drei Effekten ging das noch, bei acht nicht mehr.

`EffectType` wird **hinten erweitert**: der Zahlenwert steht so in den Projektdateien, ein
Einschub in der Mitte würde ältere Instrumente still verfälschen. Das „+“-Menü kommt seit
diesem Schritt aus `Effect::builtIn()`, damit Name und Regler an einer Stelle stehen und
nicht über Zeichenketten-Präfixe zusammengesucht werden müssen.

Geprüft wird jeder Effekt in `tests/EngineTests.cpp` an der Eigenschaft, die ihn ausmacht,
nicht bloß daran, dass sich etwas ändert:

- **Kanalfilter:** ein 80-Hz-Ton verliert hinter dem Hochpass über 80 % seines Pegels, ein
  6-kHz-Ton bleibt; bei offenen Reglern gelangt der Filter gar nicht erst in die Kette.
- **Sättigung:** gemessen wird das Verhältnis von Effektiv- zu Spitzenwert. Ein Sinus liegt
  fest bei 0,707; je eckiger die Welle, desto näher an 1. Das misst die Verformung selbst
  statt der Lautstärke.
- **Transienten:** der Spitzenwert in den ersten Millisekunden nach dem Einsatz steigt
  bzw. fällt — und steht eine Viertelsekunde später wieder, wo er war.
- **Chorus:** der Effektivwert eines glatten Sinus fängt an zu atmen (wandernde
  Auslöschungen); ohne Chorus schwankt er nicht.
- **Bit-Crusher:** gezählt werden die verschiedenen Werte im Signal. Bei grober Auflösung
  bleiben höchstens acht Stufen übrig; verglichen wird gegen den unbearbeiteten Sinus,
  denn der hat von sich aus nur so viele Werte, wie eine Periode Samples hat.

### Export als eigenständige App

Der Standalone-Export schreibt einen Ordner, den man am Stück weitergeben kann:

```
Nachtbass (App)/
    Nachtbass.exe        das Studio selbst, umbenannt und umgetauft
    Instrument.sisp      das Instrument
    Samples/             die verwendeten Audiodateien
```

Die Vorlage ist **das laufende Programm selbst** (`findStandaloneTemplate`). Kopiert wird es
und bekommt anschließend denselben Kennungs-Patch wie das VST3 — dafür wurde `patchBundle`
in `patchBinary` (der Datenblock) und den VST3-Teil (Moduldatei umbenennen,
`moduleinfo.json` nachziehen) aufgeteilt. Eine Programmdatei braucht nur den ersten Teil.

Dass die App ihr Instrument lädt, entscheidet **nicht die Bauform, sondern der Fundort**:
`loadBundledInstrument` läuft jetzt in jedem Fall und findet neben dem Programm eine
`Instrument.sisp` — neben dem Studio selbst liegt keine. Damit entfällt die Fallunterscheidung
nach Plugin oder App ganz.

Weil `JucePlugin_Name` zur Laufzeit aus dem Kennungsblock kommt, legt die exportierte App auch
ihre **eigene Einstellungsdatei** an (`Nachtbass.settings`). Sie startet also mit ihrem
Instrument und merkt sich danach ihre eigene Sitzung, ohne dem Studio dazwischenzufunken.

Dabei fiel auf, dass der Programmname an drei Stellen **fest eingebaut** war — Titelleiste,
Menüeintrag „Über …“ und der Dialog dahinter. Die exportierte App hieß im Fenstertitel
weiterhin „Sample Instrument Studio“, obwohl ihre Kennung längst stimmte. Alle drei lesen den
Namen jetzt aus `JucePlugin_Name`.

Die exportierte App zeigt **das ganze Studio**, nicht eine abgespeckte Spieloberfläche. Das ist
bewusst so: die Studio-Ansicht funktioniert, die kompakte Plugin-Ansicht ist noch Grundgerüst.
Wer sein Instrument weitergibt, gibt damit auch den Editor mit.

**Zum Weitergeben Release bauen.** Ein Debug-Build hängt an der Debug-Laufzeit von Visual
Studio, die auf fremden Rechnern nicht vorhanden und auch nicht weitergebbar ist.

Geprüft wird in `tests/ProcessorTests.cpp`: der Ordner entsteht mit Programm, Instrument und
Samples, der Kennungsblock im Programm trägt den neuen Namen und den alten nicht mehr (byteweise
gesucht — in einer Programmdatei bricht jede Textsuche am ersten Nullbyte ab), und das
mitgelieferte Instrument lässt sich laden und klingt. Gegengeprüft wurde außerdem von Hand:
die exportierte App startet, heißt im Fenstertitel „Nachtbass“ und zeigt ihre Zonen.

### Warum kein Audio Unit

Ein Audio Unit ist ein macOS-Bundle (`.component`), das nur ein Mac bauen und nur ein Mac laden
kann; CMake nimmt das Format deshalb nur unter `APPLE` in `SIS_FORMATS` auf. Von Windows aus
lässt sich weder eine Vorlage erzeugen noch das Ergebnis prüfen — der Kennungs-Patch müsste
dort zusätzlich in der `Info.plist` greifen (Subtype, Hersteller, Name), und das wäre
ungetesteter Code.

Die Zeile ist darum **ganz aus der Export-Ansicht entfernt** – ein Ziel anzubieten, das nie
etwas produziert, ist schlechter als keines. Das Feld `ExportTargets::audioUnit` bleibt im
Modell erhalten, damit ältere Projektdateien unverändert laden und eine spätere Mac-Fassung
daran anknüpfen kann; ausgewertet wird es derzeit nirgends.

Auf einem Mac gebaut wäre der Weg derselbe wie beim VST3: Bundle kopieren, Kennung setzen,
Instrument in die Resources legen – zusätzlich müsste der Kennungs-Patch in der `Info.plist`
greifen (Subtype, Hersteller, Name).

## Version und Credits

Die Version steht **nur** in `project(... VERSION x.y.z)` in `CMakeLists.txt`. JUCE macht
daraus `JucePlugin_VersionString`; `Source/Version.h` reicht sie als `sis::versionString()`
weiter und hält daneben den Entwicklernamen. Eine zweite Stelle mit derselben Zahl gibt es
bewusst nicht – zwei könnten auseinanderlaufen.

`Hilfe → Credits` öffnet ein kleines Fenster (`Source/UI/Shell/CreditsWindow.*`) mit
Programmname, Entwickler und Version. Der Programmname kommt auch dort aus `JucePlugin_Name`,
also aus dem Kennungsblock – eine exportierte App zeigt in den Credits ihren eigenen Namen.
Der Verlauf der Versionen steht im README.

### Overdrive und Distortion

Beide sitzen in `Source/DSP/Drive.*` – **eine** Klasse mit einem Modus, weil sie sich das
Gerüst teilen (Verstärkung → Kennlinie → Gleichanteil sperren → Klangregler → Mischung) und
sich nur in der Kennlinie unterscheiden. Zwei Dateien hätten dasselbe Gerüst zweimal
enthalten. In der Effektkette sind es trotzdem zwei Arten mit je eigenem Platz, man kann sie
also hintereinander schalten.

| | Kennlinie | Regler |
| --- | --- | --- |
| Overdrive | kubischer Weichbegrenzer, unsymmetrisch | STÄRKE, CHARAKTER, KLANG, MENGE, AUSGANG |
| Distortion | Tangens hyperbolicus → hartes Abschneiden | STÄRKE, KANTE, KLANG, MENGE, AUSGANG |

**Warum der Versatz vor der Verstärkung sitzt.** Der Punkt am Overdrive ist die Unsymmetrie:
eine symmetrische Kennlinie erzeugt nur ungeradzahlige Obertöne, eine unsymmetrische auch
geradzahlige – der Unterschied zwischen „scharf“ und „warm“. Zwei naheliegende Wege dorthin
funktionieren **nicht**:

- *Gleichanteil hinter der Verstärkung addieren.* Bei hoher Verstärkung ist er neben dem
  Nutzsignal bedeutungslos, der Regler täte oben herum fast nichts mehr.
- *Die negative Halbwelle herunterskalieren.* Ein Rechteck mit halbierter Unterseite ist
  nach dem Gleichanteil-Sperrfilter wieder **symmetrisch** – die Skalierung ist bei 50 %
  Tastverhältnis nichts anderes als ein Gleichanteil plus ein symmetrisches Rechteck.

Beides stand hier zwischenzeitlich im Code; der Test hat den zweiten Fall aufgedeckt. Richtig
ist, den Versatz **vor** die Verstärkung zu legen: dann wandern die Nulldurchgänge, die
positive Halbwelle wird breiter als die negative, und dieses schiefe Tastverhältnis bleibt
auch bei voller Übersteuerung erhalten.

**Kein Überabtasten.** Die Kennlinien erzeugen Obertöne weit über der halben Abtastrate, die
sich als Aliasing zurückfalten. Der Klangregler (einpoliger Tiefpass dahinter) nimmt den
obersten Teil weg, mehr nicht. Sauber wäre Überabtastung – die kostet über 32 Signalwege
Speicher und brächte Latenz zwischen den Spuren mit, was bei übereinandergelegten Schichten
Kammfiltereffekte gäbe. Das ist eine bewusste Abwägung, keine Auslassung.

**Der Sperrfilter liegt bei 5 Hz**, nicht höher: er lässt das Dach eines Rechtecks absacken,
und zwar umso stärker, je höher seine Grenzfrequenz liegt. Bei 15 Hz waren das über eine
Halbwelle bei 400 Hz schon rund 11 %.

Geprüft wird in `tests/EngineTests.cpp` je an der Eigenschaft, die den Effekt ausmacht:

- **Overdrive:** gemessen wird der **zweite Oberton** über eine einzelne Fourier-Komponente.
  Symmetrisch gefahren bleibt er unter 2 % des Grundtons, unsymmetrisch steigt er über 10 %
  und ist mehr als fünfmal so laut wie symmetrisch. Das misst die Unsymmetrie selbst, nicht
  bloß „es hat sich etwas geändert“.
- **Distortion:** gemessen wird das Verhältnis von Effektiv- zu Spitzenwert – ein Sinus liegt
  fest bei 0,707, ein Rechteck bei 1. Voll gefahren kommt 0,97 heraus; bei maßvoller
  Verstärkung liegt die harte Kante messbar über der weichen.

Einzelne Obertöne taugen für die Distortion **nicht** als Maß: ihre Pegel durchlaufen beim
Verzerren Nullstellen. Der siebte Oberton war bei schwacher Verstärkung hart abgeschnitten
sogar leiser als weich – ein Test darauf hätte das Gegenteil des Richtigen behauptet.

### Flanger, Phaser, Vibrato und Tremolo

Alle vier brauchen einen Niederfrequenz-Oszillator; der liegt deshalb als winziger
kopf-nur-Baustein in `Source/DSP/Lfo.h`. Seine Phase läuft in **Umdrehungen** (0 … 1) statt
im Bogenmaß – so lässt sich ein zweiter Kanal um einen Bruchteil versetzt abfragen, ohne mit
2π zu hantieren.

| Effekt | Verfahren | Datei |
| --- | --- | --- |
| Flanger | kurze modulierte Verzögerung (0,6 … 6,6 ms) **mit Rückkopplung**, beigemischt | `ModulatedDelay` |
| Vibrato | dieselbe Leitung, aber **ohne Trockenanteil** | `ModulatedDelay` |
| Phaser | sechs Allpässe erster Ordnung, Eckfrequenz wandert 200 Hz … 2 kHz | `Phaser` |
| Tremolo | Pegel folgt dem Oszillator; bei voller Breite gegenläufig | `Tremolo` |

**Warum das Vibrato keinen Mischregler hat.** Eine modulierte Verzögerung mit Trockenanteil
*ist* ein Chorus – den gibt es schon. Lässt man den Regler weg, bleiben die drei Effekte
hörbar verschieden, statt ineinander überzugehen.

**Warum der Flanger nicht bloß ein schneller Chorus ist.** Der Unterschied ist die
Rückkopplung, nicht die Verzögerungszeit: sie schickt das Verzögerte noch einmal hinein und
macht die Kerben des Kamms schmaler und tiefer. Der Test misst genau das – das Atmen des
Pegels wächst messbar, wenn die Rückkopplung aufgedreht wird.

**Der Allpass-Koeffizient und sein Vorzeichen.** Für `H(z) = (a + z⁻¹) / (1 + a z⁻¹)` liegt
der Pol bei `z = -a`. Damit eine **tiefe** Eckfrequenz den Pol in die Nähe von `z = +1` legt,
muss `a` gegen −1 gehen, also `a = (t − 1) / (t + 1)` mit `t = tan(π f / fs)`. Mit dem
umgekehrten Vorzeichen landet der Pol nahe der halben Abtastrate, und unten herum passiert
nichts – der Phaser war dann eine Durchleitung. Genau so war es zuerst gebaut; gefunden hat
es der Test, der die Kerben beim Mischen misst.

Das ist auch der Grund, warum dieser Test doppelt angelegt ist: „ganz nass bleibt der Pegel
stehen“ wäre bei einer Durchleitung **ebenfalls erfüllt**. Erst die zweite Prüfung – beim
Mischen müssen Kerben entstehen – schließt den Fehler aus.

**Ein durchgestimmter Allpass ist nicht ganz pegeltreu.** Das gilt nur bei festen
Koeffizienten. Bei 6 Hz Durchstimmung bleibt ein Restschwank von rund 9 %, gegenüber rund
68 % beim halb gemischten Signal. Der Test prüft beides: langsam durchgestimmt steht der
Pegel exakt, schnell schwankt er wenig, und beides liegt weit unter den Kerben.

**Das Tremolo wird nie wirklich rechteckig.** Die Form läuft durch eine Tangens-Kennlinie
und bleibt dadurch stetig; ein echter Sprung im Pegel knackt bei jedem Durchgang.

`Chorus` ist älter und bringt seine eigene Verzögerungsleitung mit. Beide ließen sich mit
`ModulatedDelay` zusammenlegen – das ist bewusst unterblieben, weil der Chorus getestet
ausgeliefert ist und ein Umbau ohne hörbaren Gewinn nur Risiko wäre.

Geprüft wird in `tests/EngineTests.cpp` je an der Eigenschaft, die den Effekt ausmacht:

- **Flanger:** der Pegel atmet, und mit Rückkopplung deutlich stärker.
- **Vibrato:** der Effektivwert bleibt stehen (±15 %), aber der Grundton dünnt auf unter
  70 % aus – die Energie wandert in Seitenbänder. Beim Tremolo wäre es genau umgekehrt, und
  genau das trennt Tonhöhen- von Pegelmodulation in einer Zahl.
- **Phaser:** langsam durchgestimmt und ganz nass ändert sich der Pegel nicht; halb
  gemischt entstehen wandernde Kerben.
- **Tremolo:** der Pegel schwankt mit der Tiefe, beide Kanäle laufen ohne Breite gleich und
  mit voller Breite gegenläufig.

### Delay

Im Kern dieselbe Verzögerungsleitung wie bei Flanger und Chorus, nur viel länger – und
genau daran hängt die einzige unangenehme Entscheidung.

**Warum höchstens eine Sekunde.** Die Bausteine aller 32 Signalwege entstehen in `prepare`,
weil im Audio-Thread nichts angefordert werden darf. Eine Leitung kostet
`Zeit × Abtastrate × 2 Kanäle × 4 Byte`: bei einer Sekunde und 48 kHz sind das 384 kB je
Signalweg, rund **12 MB für alle 32** – auch für die, die nie ein Delay benutzen. Bei zwei
Sekunden wären es 24 MB. Eine Sekunde deckt musikalisch das meiste ab (eine Viertelnote bei
60 bpm), und die Obergrenze bleibt vorhersehbar.

Längere Echos bräuchten Leitungen, die **erst bei Bedarf** entstehen. Das geht nicht
nebenbei: `BusProcessors` liegt in der Engine und wird vom Audio-Thread gelesen, während der
Message-Thread schon den nächsten Plan baut – die Leitung dort zu vergrößern wäre ein
Datenrennen. Sauber wäre, den Zustand je Signalweg **mit dem Plan zu tauschen** statt ihn in
der Engine zu halten. Das ist der Umbau, den ein Delay über eine Sekunde voraussetzt.

**Die Dämpfung sitzt in der Rückkopplung**, nicht am Ausgang. So wird jede Wiederholung eine
Stufe dunkler als die davor – der Unterschied zwischen einem Bandecho und einem Tiefpass
hinter dem Delay, wo alle Wiederholungen gleich dumpf klängen.

**Die Zeit wird geglättet, springt aber nach einem Reset.** Ein Sprung im Leseabstand knackt;
langsam nachgeführt entsteht stattdessen das Tonhöhenziehen von Bandmaschinen. Beim Laden
eines Instruments wäre dieses Ziehen aber falsch: das Delay glitte von der Voreinstellung zur
eingestellten Zeit und die erste Wiederholung käme verschmiert. Nach `reset` wird die Zeit
deshalb übernommen, erst danach angefahren.

**Ping-Pong** kreuzt die Rückkopplung. Damit das hörbar wird, darf das Trockensignal bei
vollem Ping-Pong nur noch in die linke Leitung – sonst starten beide Seiten gleichzeitig und
es bliebe beim gewöhnlichen Echo.

Geprüft wird in `tests/EngineTests.cpp` mit einer Note, die nach 50 ms losgelassen wird:
alles, was danach noch klingt, kommt aus der Leitung. Gemessen werden der Einsatz der ersten
Wiederholung (längere Zeit heißt später), das Ausbleiben einer zweiten ohne Rückkopplung, ihr
Auftauchen und Leiserwerden mit Rückkopplung, der Seitenwechsel bei Ping-Pong und das
Dunklerwerden durch die Dämpfung.

Beim Schreiben der Fenster lohnt sich das Nachrechnen: bei 100 ms Verzögerung und 50 ms
Quelle liegt die erste Wiederholung bei 4800 … 7200 Samples, die zweite bei 9600 … 12000.
Ein Fenster ab 12000 misst bereits die dritte – und lässt den Test aus dem falschen Grund
scheitern.

### Noise Gate, Expander, Limiter und De-Esser

Alle vier verfolgen einen Pegel, deshalb liegt der Hüllkurvenfolger als kopf-nur-Baustein
in `Source/DSP/EnvelopeFollower.h`. Er nimmt getrennte Zeiten für Steigen und Fallen –
mehr braucht keiner von ihnen.

**Gate und Expander sind dieselbe Rechnung**, deshalb gibt es nur eine Klasse `Gate` und
darin **keinen Modus-Schalter**: das Gate ist ein Expander mit steilem Verhältnis (fest 10:1)
und Haltezeit, der Expander ein Gate ohne Haltezeit und mit sanftem, einstellbarem
Verhältnis. Welches von beiden gemeint ist, entscheidet allein, was `buildRenderPlan`
einstellt. Die Regler unterscheiden sich trotzdem – das Gate hat HALTEN und TIEFE, der
Expander ein VERHÄLTNIS.

**Der Detektor des Gates fällt schnell (10 ms), nicht träge.** Ein träger Rückweg wäre
bequem gegen das Rasseln, macht aber die Haltezeit wirkungslos: mit 40 ms kommt die
Hüllkurve in einer 50-ms-Lücke gar nicht erst unter die Schwelle, das Gate schließt nie,
und der Regler tut nichts – ganz gleich, was eingestellt ist. Genau so war es zuerst
gebaut; aufgefallen ist es, weil der Test zur Haltezeit keinen Unterschied messen konnte.
Gegen das Rasseln ist die Haltezeit da, und die gehört dem Anwender.

**Der Limiter blickt nicht voraus.** Die Verstärkung wird aus dem *aktuellen* Sample
berechnet und auf dasselbe Sample angewandt – dadurch wird die Decke nie überschritten,
auch nicht um ein Sample. Der übliche Vorausblick bräuchte eine Verzögerung, und die brächte
Latenz: da jede Spur ihren eigenen Signalweg hat, lägen Spuren mit und ohne Begrenzer
gegeneinander verschoben, was bei übereinandergelegten Schichten Kammfiltereffekte gäbe.
Der Preis ist eine leichte Verzerrung bei tiefen Tönen, weil sich die Verstärkung innerhalb
einer Schwingung bewegt. Für ein Werkzeug, das Spitzen einfängt, ist das der bessere Tausch.

**Der De-Esser trennt zweipolig, nicht einpolig.** Mit einem Pol ist die Flanke so flach,
dass bei der doppelten Trennfrequenz noch gut ein Drittel des Signals im unteren Band
steckt – und was dort steckt, wird nie abgesenkt. Der De-Esser bekam den Zischlaut so
schlicht nicht zu fassen; der Test hat es gezeigt. Die Zerlegung bleibt exakt ergänzend
(`hoch = ein − tief`), die Summe ergibt bei Verstärkung 1 wieder das Eingangssignal.

Geprüft wird in `tests/EngineTests.cpp`:

- **Noise Gate:** über der Schwelle geht alles durch, darunter bleibt unter 5 % übrig; und
  mit Haltezeit überlebt ein pulsendes Signal die Lücken besser als ohne.
- **Expander:** zwei Pegel unter der Schwelle werden *weiter auseinandergezogen* – aus 2:1
  wird mehr als 4:1. Wichtig dabei: das Verhältnis maßvoll wählen, sonst laufen beide
  Pegel in die Begrenzung von 40 dB und ihr Abstand bleibt gerade gleich.
- **Limiter:** der Spitzenwert bleibt unter der Decke, auch mit +24 dB davor. Zu beachten:
  −6 dB sind **0,5012**, nicht 0,5 – eine Schwelle von genau 0,5 lässt den Test scheitern,
  obwohl der Begrenzer richtig rechnet.
- **De-Esser:** ein 9-kHz-Ton verliert über 60 %, ein 300-Hz-Ton bleibt unangetastet.

### Envelope Filter und Auto-Wah

Beide sind dasselbe resonante Bandfilter mit wandernder Eckfrequenz; verschieden ist allein,
**was** die Frequenz bewegt — deshalb eine Klasse `SweptFilter` mit einer Quelle statt zweier
fast gleicher.

| | Bewegt durch | Regler |
| --- | --- | --- |
| Envelope Filter | den Anschlag: lauter heißt offener | GRUNDTON, EMPFINDLICHKEIT, RESONANZ, ATTACK, RELEASE, MENGE |
| Auto-Wah | einen Oszillator, gleichmäßig zwischen zwei Frequenzen | VON, BIS, TEMPO, RESONANZ, MENGE |

Die Regler spiegeln den Unterschied: das Auto-Wah spannt mit **VON** und **BIS** ein
Intervall auf, in dem es gleichmäßig läuft; beim Envelope Filter gibt **GRUNDTON** die
Ruhelage an, und **EMPFINDLICHKEIT** bestimmt, wie weit der Anschlag von dort nach oben
zieht (bis drei Oktaven).

**Warum ein Zustandsvariablenfilter und nicht die Biquads des Equalizers.** Deren
Koeffizienten entstehen bewusst außerhalb des Audio-Threads – hier wandert die Frequenz aber
im Takt des Signals, sie müssten also laufend neu entstehen. Der Zustandsvariablenfilter ist
dafür gebaut: zwei Additionen je Abtastwert, und den einen Sinus für die Frequenz braucht er
nur **alle 16 Abtastwerte** neu. Bei 48 kHz sind das 3000 Stützstellen je Sekunde – für eine
Filterfahrt weit mehr als genug, und ein Bruchteil der Rechenzeit, die ein Biquad je
Abtastwert gekostet hätte.

Der Filtertyp ist das **Bandsignal**, nicht das Tiefpasssignal – das ist der Klang, den man
von einem Wah kennt. Weil davon der Körper verlorengeht, haben beide eine MENGE, über die man
trocken dazumischt.

Die Eckfrequenz ist auf ein Sechstel der Abtastrate begrenzt: darüber wird der
Zustandsvariablenfilter instabil. Für ein Wah, das sich zwischen 200 Hz und 3 kHz bewegt, ist
das reichlich.

Geprüft wird in `tests/EngineTests.cpp` genau der Unterschied zwischen beiden:

- **Envelope Filter:** derselbe 1,5-kHz-Ton wird einmal leise und einmal laut gespielt;
  gemessen wird, wie viel davon durchkommt – **bezogen auf den Eingangspegel**, sonst wäre
  bloß die Lautstärke gemessen. Leise bleibt das Filter zu, laut geht es auf.
- **Auto-Wah:** bei **gleichbleibendem** Pegel schwankt der Ausgangspegel trotzdem – es folgt
  der Uhr, nicht dem Anschlag. Dieselbe Messung am Envelope Filter zeigt, dass der bei
  gleichbleibendem Pegel stillsteht. Das ist die Unterscheidung in einer Zahl.

### Kanal-Streifen

Hier steckt **keine neue Signalverarbeitung**. `Source/DSP/ChannelStrip.*` setzt Gate,
Equalizer und Kompressor zusammen; der Beitrag des Streifens ist die **Reihenfolge** und
dass man sie mit sechs statt fünfzehn Reglern bedient: GATE, TIEFEN, MITTEN, HÖHEN,
KOMPRESSION, AUSGANG.

Die Reihenfolge folgt dem Mischpult und ist die eigentliche Entscheidung:

1. **Gate** zuerst – was weg soll, soll weg, bevor etwas es anhebt. Hinter einem Kompressor
   müsste es gegen dessen Anhebung der leisen Stellen anarbeiten.
2. **Klangregelung** danach – der Kompressor soll auf das hören, was man behalten will. Wer
   die Tiefen wegnimmt, will nicht, dass sie die Regelung noch steuern.
3. **Kompressor** zuletzt, weil er das Ergebnis der beiden Schritte festhält.
4. **Ausgang** als reine Verstärkung hinterher.

Wer eine andere Reihenfolge braucht, hängt die Einzeleffekte in die Kette – dafür sind sie
da. Der Streifen ist der schnelle Weg, nicht der einzige. Weil er eine eigene Effektart ist,
belegt er auch nur **einen** Platz: ein zusätzlicher eigener Kompressor bleibt möglich.

**Kompression an einem Regler.** Schwelle (−6 … −36 dB), Verhältnis (1,5 … 6:1) und
Ausgleich (0 … 8 dB) laufen gemeinsam mit. Getrennt einstellbar sind sie im eigenen
Kompressor; hier geht es um das schnelle „mehr davon“.

Die Klangregelung hat drei eigens gewählte Bänder (Kuhschwanz 150 Hz, Glocke 1 kHz mit
Güte 0,9, Kuhschwanz 6 kHz), nicht die Voreinstellungen des Equalizers – ein Streifen hat
andere Aufgaben als ein Vierband-Equalizer. Die Koeffizienten entstehen wie überall im
Renderplan, nicht im Audio-Thread.

Geprüft wird in `tests/EngineTests.cpp`, dass jede Stufe wirkt (Klangregelung nach oben und
unten, Ausgang, und zwei Pegel rücken unter Kompression zusammen) – und vor allem die
**Reihenfolge**: ein Ton knapp unter der Gate-Schwelle bleibt weg, *auch wenn die Tiefen
voll angehoben sind*. Säße die Klangregelung vorn, würde die Anhebung ihn über die Schwelle
heben und das Gate wieder aufreißen. Ohne diese Prüfung wäre die Reihenfolge eine Behauptung
im Kommentar.

Nebenbei aufgefallen: die Beschriftungsspalte der Effektkarten war mit 52 px zu schmal
geworden – „KOMPRESSION“ und „RÜCKKOPPLUNG“ wurden abgeschnitten. Jetzt 76 px; ein etwas
kürzerer Balken ist besser als ein abgeschnittener Reglername. Gesehen hat das kein Test,
sondern ein Bildschirmfoto.

### Das „+“-Menü der Effektkette

Zweiundzwanzig Einträge in einer flachen Liste waren nicht mehr zu überblicken. Das Menü
zeigt jetzt acht Zeilen: den **Kanal-Streifen** oben, fünf Untermenüs nach Familien, und
*Externes Plugin …* unten.

Die Familien stehen im **Modell** (`familyOf`, `toDisplayString`, `effectFamilyOrder` in
`Source/Model/Instrument.*`), nicht in der Oberfläche – dort, wo auch Name und Regler eines
Effekts liegen. Die Oberfläche ordnet nur noch an.

`familyOf` hat bewusst **kein `default`** im `switch`. Kommt eine Effektart hinzu und wird
hier vergessen, warnt der Übersetzer, statt dass sie still aus dem Menü verschwindet. Für
Arten, die gar nicht ins Menü gehören (der Platzhalter und fremde Plugins), gibt es
`std::nullopt`; die Oberfläche hängt solche Einträge sicherheitshalber unten an, statt sie
wegzulassen.

Der **Kanal-Streifen steht oben und nicht in seiner Familie**: für die meisten Spuren ist er
der schnellste Weg, und dafür soll man kein Untermenü aufklappen müssen.

Geprüft wird die Gliederung in `tests/EngineTests.cpp`: jeder Eintrag hat einen eindeutigen
Namen und eine Familie, keine Familie bleibt leer, und die Untermenüs zusammen ergeben genau
die Liste aus `Effect::builtIn()` – kein Effekt fällt durch. Der Übersetzer fängt die
vergessene Art ab, der Test die falsch einsortierte.

### Presets je Effektart

Jede Effektkarte eines internen Effekts hat einen Knopf **Presets**; fremde Plugins haben an
derselben Stelle **Öffnen**, weil sie ihre eigenen Presets mitbringen. Das Menü bietet
*Grundstellung*, die gesicherten Presets, *Sichern als …* und ein Untermenü *Löschen*.

Gespeichert wird in `%APPDATA%\Sample Instrument Studio\presets.xml`
(`Source/Model/PresetLibrary.*`). Drei Entscheidungen stecken darin:

- Ein Preset hält **nur die Reglerwerte**. Nicht den Namen des Effekts, nicht ob er
  eingeschaltet ist – beides gehört zur einzelnen Spur, nicht zum Klang.
- Zugeordnet wird über die **Beschriftung** des Reglers, nicht über seine Nummer. So
  überlebt ein Preset, wenn bei einem Effekt später ein Regler dazwischen kommt; was sich
  nicht zuordnen lässt, bleibt stehen.
- Am Effekt wird **nicht vermerkt**, welches Preset geladen wurde. Sobald man einen Regler
  anfasst, wäre der Name ohnehin falsch, und ein Name, der lügt, ist schlechter als keiner.
  Das spart außerdem ein weiteres Feld in der Projektdatei.

*Grundstellung* nimmt die Werte aus `Effect::builtIn()` – es gibt keine zweite Liste mit
Voreinstellungen neben den Fabriken.

**Mitgelieferte Klangvorlagen gibt es bewusst nicht.** Ein Satz Presets für zweiundzwanzig
Effekte wäre schnell geschrieben und nichts wert, wenn ihn niemand gehört hat. Die Ablage
steht; die Vorlagen gehören ans Ohr.

Geprüft wird in `tests/EngineTests.cpp` gegen eine **eigene Datei**, damit die echten Presets
des Anwenders unberührt bleiben: sichern, in der Liste finden, auf einen frischen Effekt
anwenden, Grundstellung, Überschreiben statt Verdoppeln, Löschen – und jedes Mal, dass es
einen Neustart der Sammlung übersteht. Dazu ein Test, dass die Zuordnung über die
Beschriftung läuft: ein Effekt mit vertauschten Reglern bekommt trotzdem die richtigen Werte.

**Dabei ist zum vierten Mal dieselbe Falle zugeschnappt.** `juce::XmlElement::writeTo (File)`
legt eine Zwischendatei an und benennt sie um; unter Windows scheitert genau das sporadisch
am Virenscanner – ohne Fehlermeldung, die Datei behält nur still ihren alten Inhalt. Hier
schlug es beim Löschen des letzten Presets zu und war beim nächsten Lauf wieder weg. Nach
dem Kennungs-Patch, der `moduleinfo.json` und dem Verschieben im Export ist das jetzt an
einer Stelle benannt: `Source/Model/XmlFile.h` schreibt unmittelbar. `PluginLibrary` benutzt
es ebenfalls – dort lauerte dieselbe Falle bisher unbemerkt.

### Makro-Zuweisung

Ein Makro ist ein Regler, der andere Regler mitzieht. Drei Entscheidungen dazu:

**Zugewiesen wird am Regler, nicht in einer Liste.** Rechte Maustaste auf einen Regler der
Effektkette, Makro wählen – dort weiß man, was man zuweist. `MacroWindow` („Makros
zuweisen …“) zeigt nur, was zugewiesen *ist*, und löst es auf Klick wieder. Eine Liste, in
der man Ziele *aussucht*, hätte jeden Regler jedes Effekts jeder Spur anbieten müssen.

**Ein Ziel gehört zu höchstens einem Makro.** `assignMacro` entfernt das Ziel erst überall
und hängt es dann an. Zwei Makros auf denselben Regler wären kein Mehrwert, sondern ein
Wettlauf: wer zuletzt bewegt wird, gewinnt, und der andere Regler zeigt etwas Falsches an.

**Gespeichert wird über Zonen-Kennung und Nummern**, nicht über Zeiger:

```xml
<Macros><Target macro="0" zone="z1" track="0" effect="2" parameter="1"/></Macros>
```

Das Instrument wird beim Laden neu aufgebaut – Zeiger überlebten das nicht. Zeigt ein Ziel
ins Leere, weil ein Effekt gelöscht wurde, überspringt `applyMacro` es stillschweigend und
`MacroWindow` schreibt „(verwaist)“. Ein zugewiesener Regler wird in `TrackInspector` mit
seiner Beschriftung in Akzentfarbe gezeichnet, sonst sieht man einer Kette nicht an, welche
Regler fremdgesteuert sind.

Geprüft wird in `tests/EngineTests.cpp`: zuweisen, das Makro bewegt den Regler, ein anderer
bleibt stehen, das zweite Zuweisen **verschiebt** statt zu verdoppeln, die Zuweisung
übersteht Speichern und Laden, und nach dem Lösen bleibt der Regler stehen.

### Die volle Plugin-Ansicht

Der Zonen-Dialog benutzt **denselben** `SampleEditor` wie das Studio, nicht eine zweite,
abgespeckte Fassung – eine zweite müsste jede Änderung an der ersten nachziehen. Dafür
führt `PluginShell` einen eigenen `StudioContext`, obwohl es dort weder Menüs noch Befehle
gibt; der `ApplicationCommandManager` darin bleibt leer. Der Verlauf ist der des
Prozessors, also zählt ein Schnitt im Plugin genauso wie einer im Studio.

**Dabei kam ein Fehler heraus, den das Studio nie gezeigt hat:** im 720 px breiten Dialog
lief die Kopfzeile des Sample-Editors unter die sechs Werkzeugknöpfe. Der Text ist jetzt
auf die Kante des ersten Knopfs begrenzt, und wird es eng, fällt zuerst der Titel
„SAMPLE-EDITOR“ weg – dass dies der Sample-Editor ist, sieht man auch ohne ihn; welche
Datei und welcher Ausschnitt gemeint sind, dagegen nicht.

Die Makro-Beschriftungen stehen zweizeilig: „Luft / Obertöne“ passt unter einen 44-px-Regler
sonst nicht, und ein abgeschnittener Name sagt nichts mehr über das, was der Regler tut.

**Prüfen ohne DAW:** `SIS_PLUGIN_VIEW=1` lässt auch die App das Plugin-Fenster zeigen,
`SIS_PLUGIN_VIEW=sheet` gleich mit offenem Zonen-Dialog. Ohne diesen Schalter bekäme man
diese Ansicht beim Entwickeln nie zu sehen, und genau so bleiben Layoutfehler wie der
obige liegen.

**Und dieselbe Falle zum fünften Mal.** `StudioShell::writeProject` und der Export
schrieben die Projektdatei noch mit `XmlElement::writeTo` – also über eine Zwischendatei,
die der Virenscanner sporadisch festhält. Beim Durchsehen für die Preset-Ablage der
Plugin-Ansicht aufgefallen; beide benutzen jetzt `writeXmlDirectly`. Damit ist im ganzen
Projekt kein `writeTo` mehr übrig.

### Die zweite Mapping-Art: Drumset

**Der Unterschied sitzt in der Engine, nicht in der Ansicht.** Ein Drumset spielt jedes
Sample in seiner eigenen Tonhöhe, egal welche Taste es auslöst – sonst wäre eine Snare
zwei Oktaven höher eine andere Snare. Deshalb hängt die Art am Instrument
(`InstrumentKind` in der `.sisp`) und nicht an der Oberfläche, und deshalb wird sie beim
Anlegen abgefragt. Umschalten geht trotzdem jederzeit; es wirft nichts weg.

Getragen wird das von **einem** Feld: `ZonePlan::followsPitch`. `buildRenderPlan` setzt es
aus der Instrumentart, `SamplerVoice::start` lässt dann den Abstand zur Taste weg:

```cpp
const double fromKey = zone.followsPitch ? (double) (midiNote - zone.rootNote) : 0.0;
const double semitones = fromKey + layer.semitoneOffset;
```

Die **Spur-Tonhöhe wirkt weiter** (`layer.semitoneOffset`). Eine tiefer gestimmte Snare
will man auch im Drumset; hätte man dort alles abgeschaltet, wäre der Regler stillschweigend
tot gewesen.

**Ein Kit-Teil ist eine ganz normale Zone** mit `lowNote == highNote == rootNote` und einer
Kennung in `Zone::drumPart`. Damit erbt das Drumset Spuren, Effekte, Hüllkurve, Bounce und
Export, ohne dass irgendetwas davon von Schlagzeugen wissen muss. Ein zweites Datenmodell
neben `Zone` wäre der teuerste Weg zum selben Ergebnis gewesen.

Die Tabelle der Teile steht in `Source/Model/DrumKit.h` (Kennung, Name, GM-Note); ihre
**Anordnung** dagegen in `DrumKitView.cpp`, als Anteile der Fläche. Die Kennung steht in
Projektdateien und darf sich nicht ändern, Name, Note und Platz dürfen.

Die Noten folgen **General MIDI**. Das ist keine Kosmetik: wer ein fertiges Drum-Pattern
aus seiner DAW daraufspielt, erwartet die Bassdrum auf 36. Eine eigene Belegung wäre nur
dann besser, wenn es gar keine verbreitete gäbe.

Zwei Kleinigkeiten, die beim Bauen auffielen:

- Das Kit behält sein **Seitenverhältnis** (1,4 : 1) und steht mittig. Über die ganze
  Fläche gezogen werden aus den Becken flache Ovale – ein Schlagzeug, das man nicht
  wiedererkennt, hilft beim Zuordnen gar nichts.
- In `mouseDown` wird das `DrumPart` **kopiert, bevor** das Modell geändert wird. Der
  Zeiger davor zeigte in `pieces`, und die Änderung kann über `resized()` diese Liste neu
  aufbauen – ein Zeiger ins Leere, der nur selten zugeschlagen hätte.

Geprüft wird in `tests/EngineTests.cpp` genau die Eigenschaft, die ein Drumset ausmacht:
eine Oktave höher spielt **gleich schnell** (Verhältnis 1,0), zwei Oktaven tiefer auch,
die Spur-Tonhöhe verdoppelt weiterhin, und dasselbe Instrument als Tonhöhen-Instrument
verstimmt sehr wohl. Dazu die Ablage: ein Teil sitzt auf einer Taste, zweimal anlegen legt
nicht zweimal an, Art und Kit-Kennung überstehen Speichern und Laden – und eine **alte
Projektdatei ohne die Angabe** bleibt ein Tonhöhen-Instrument.

### Die internen Effekte als eigenständige VST3

**Ein Quelltext, einundzwanzig Plugins.** `Source/Effects/EffectPlugin.cpp` wird für jedes
Ziel mit einem anderen `SIS_EFFECT_TYPE` übersetzt; die CMake-Funktion
`sis_add_effect_plugin` legt Ziel, Produktnamen, Plugin-Code und Kategorie fest. Der
Prototyp kommt aus `Effect::builtIn()`, also aus derselben Liste wie das „+“-Menü.

Möglich wird das durch zwei Extraktionen, die vorher fehlten:

- `applyEffectChain` (`Source/Audio/EffectChain.h`) — der Fall-für-Fall-Dispatch, der
  vorher in `SamplerEngine::applyEffects` steckte. Sampler und Plugin rufen jetzt dieselbe
  Funktion.
- `buildSingleEffect` (`RenderPlan.h`) — ein einzelner Effekt in einen `TrackEffectsPlan`.
  Bewusst über denselben Weg wie die Kette (`buildEffects` mit einer Ein-Effekt-Spur)
  statt über eine zweite Umrechnung daneben; letztere wäre die Stelle, an der ein Plugin
  eines Tages anders klingt als der Effekt, von dem es abstammt.

Die Thread-Aufteilung ist die des Studios: ein Regler setzt nur ein Flag
(`parameterChanged`, die ein Host auch aus dem Audio-Thread schicken darf), ein Timer baut
den Plan auf dem Message-Thread neu, der Audio-Thread liest ihn unter einer SpinLock mit
`tryLock`. Koeffizienten entstehen nie im `processBlock`.

**Presets teilen sich die Ablage mit dem Studio.** `PresetLibrary` ist nach `EffectType`
sortiert und liegt neben den Programmeinstellungen; das Plugin benutzt dieselbe Datei.
Was am „SIS Chorus“ in der DAW gesichert wird, steht anschließend im Presets-Menü der
Chorus-Karte im Studio. Das war kein Mehraufwand, sondern schlicht die vorhandene Klasse.

**Die Plugins hängen nicht am Standardziel** (`EXCLUDE_FROM_ALL`). Ein JUCE-Plugin-Ziel
übersetzt die JUCE-Module für sich; einundzwanzig davon dauern ein Vielfaches des
restlichen Projekts, und beim täglichen Bauen braucht man sie nicht.

`build.ps1 -Target SisEffectPlugins` baut alle auf einmal — aber **Ziel für Ziel**, nicht
alle gleichzeitig: `cmake --build` lässt Ninja sonst über alle einundzwanzig zugleich her,
und die Menge paralleler `cl.exe` hat hier schon einmal den Arbeitsspeicher der Maschine
erschöpft. Wer auf Nummer sicher gehen will, ruft sie einzeln auf:

```powershell
foreach ($t in 'SisChorus','SisDelay','…') { .uild.ps1 -Target "${t}_VST3" }
```

Zu beachten: `build.ps1` ohne `-Target` baut **nur** `SampleInstrumentStudio_Standalone`.
Das Instrument-VST3 (`SampleInstrumentStudio_VST3`) und die Testprogramme wollen einzeln
genannt werden — sonst hält man einen alten Stand für den aktuellen.

**Dabei musste `PluginLibrary.cpp` geteilt werden.** Ein Effekt-Plugin hostet selbst keine
Plugins, und ohne `JUCE_PLUGINHOST_VST3` gibt es `juce::VST3PluginFormat` gar nicht — die
Datei ließ sich also nicht mitnehmen. Weglassen ging aber auch nicht: `applyEffectChain`
kennt den Fall „fremdes Plugin“ und ruft `HostedPlugin::process`, und `TrackEffectsPlan`
hält einen `HostedPlugin::Ptr`. Die **Instanz** steht deshalb jetzt in
`Source/Audio/HostedPlugin.cpp`, das **Suchen und Verwalten** bleibt in `PluginLibrary.cpp`.
Nur die erste Hälfte geht in die Effekt-Plugins.

*Aufgefallen ist das erst spät, und beinahe gar nicht:* `build.ps1 -Target SisChorus` baut
das **Sammelziel**, das nur die gemeinsame Bibliothek erzeugt und nichts linkt — es meldete
brav Erfolg, während im Ausgabeordner noch ein altes `.vst3` aus einem früheren Lauf lag.
Wer ein Plugin prüfen will, baut `SisChorus_VST3` und sieht nach, ob die Datei
`…/Contents/x86_64-win/SIS Chorus.vst3` wirklich neu ist.

Mono-Betrieb lässt
beide Kanalzeiger auf denselben Speicher zeigen: die Effekte schreiben dann zweimal
dasselbe, was richtig ist — ein eigener Mono-Pfad wäre zweiundzwanzigmal Gelegenheit, es
verschieden zu machen.

Geprüft wird in `tests/EngineTests.cpp` **über alle Effektarten auf einmal**: jede ergibt
genau einen Prototyp, `buildSingleEffect` einen aktiven Schritt der richtigen Art, jede hat
Regler, und `applyEffectChain` liefert damit endliche Werte in vernünftiger Höhe. Vergisst
jemand beim Hinzufügen einer Art einen der beiden Fälle, bliebe das zugehörige Plugin
**still** — und das fiele sonst erst in einer fremden DAW auf.

### Warum es weiterhin interne Effekte gibt

Die Frage lag nahe, ob man nicht einfach alle installierten VST3 anzeigt und die
Unterscheidung fallen lässt. Im **Menü** ist sie jetzt weg: eigene Effekte und
installierte Plugins stehen zusammen, letztere unter „Installierte Plugins“. Im Unterbau
bleibt sie, und der gewichtigste Grund ist der Export: ein exportiertes VST3-Instrument
soll auf einem fremden Rechner klingen, und ein gehostetes Plugin muss dort installiert
sein. Dazu kommen die lesbare Projektdatei, der Verzicht auf eine Plugin-Instanz je Spur
und Effekt, und dass sich interne Effekte direkt testen lassen.

Die **eigenen** Effekt-VST3 werden im Menü „Installierte Plugins“ übersprungen (erkennbar
am Hersteller): sie stehen schon oben als interne Effekte, und dort sind sie die bessere
Wahl.
