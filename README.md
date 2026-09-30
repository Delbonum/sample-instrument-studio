# Handoff: Sample Instrument Studio

## Überblick

Sample Instrument Studio ist ein PC-Programm, mit dem Anwender aus eigenen Audiodateien
ein spielbares virtuelles Instrument bauen. Der Ablauf: Musikdateien laden, zuschneiden und
bearbeiten (Time-Stretch, Pitch, Reverse, Fades), mehrere Spuren übereinanderlegen (z. B.
langer Basston + kurzer Attack-Sound = ein Sample), mit internen und externen Effekten
versehen, über die Tastatur in Zonen verteilen und als VST3 oder Standalone-App
exportieren.

Das Programm läuft selbst in zwei Formen:

- **Standalone** — volles Studio-Fenster mit Fensterleiste, Menüleiste, drei Ansichten.
- **Plugin (VST3)** — kompaktes Fenster in der DAW zum Spielen, Mischen und für
  schnelle Korrekturen; tiefe Bearbeitung führt zurück ins Standalone.

## Über die Design-Dateien

Die Dateien in diesem Paket sind **Design-Referenzen in HTML** – Prototypen, die Aussehen und
Verhalten zeigen. Sie sind **kein Produktionscode zum Übernehmen**. Die Aufgabe ist, diese
Entwürfe in der Zielumgebung nachzubauen. Für ein Audio-Plugin dieser Art heißt das in der
Praxis C++ mit **JUCE** (VST3/AU/Standalone aus einer Codebasis, eigene Plugin-Hosting-API
für externe Effekte) – die Entscheidung liegt beim umsetzenden Team. Die HTML-Prototypen
dienen als verbindliche Vorlage für Layout, Maße, Farben, Typografie und Interaktion.

## Fidelity

**High fidelity.** Farben, Typografie, Abstände und Interaktionen sind final gedacht und in
der Referenz exakt umgesetzt. Alle Maße im HTML sind CSS-Pixel bei einer Auslegungsbreite von
1440 × 900 px (Standalone) bzw. 960 px Fensterbreite (Plugin).

## Ansichten

Der Standalone-Rahmen ist über alle Ansichten gleich, von oben nach unten:

1. **Fensterleiste**, Höhe 34 px, Hintergrund `#DEDBD5`, Unterlinie `#C4C0B8`.
   Links App-Icon 17 px + Text `Nocturne Hybrid Bass.sisp — Sample Instrument Studio`
   (12 px, `#3A3833`). Rechts drei Fensterknöpfe je 44 px breit (—, □, ✕);
   Hover `#CFCCC6`, beim Schließen `#B4553F` mit weißer Glyphe.
   Unter Windows zeichnet die App diese Leiste selbst, sie sitzt immer über der Menüleiste.
2. **Menüleiste**, Höhe 26 px, `#EDEBE7`. Einträge: Datei, Bearbeiten, Ansicht, Instrument,
   Hilfe (12 px). Offenes Menü: Knopf `#FBFAF8`, Dropdown 262 px breit, weiß, Rand `#BFBBB3`,
   Schatten `0 10px 26px rgba(25,24,23,0.16)`. Menüzeile: 12 px Haken-Spalte (`✓`, `#5546CE`),
   Label, rechts Tastenkürzel in Mono 10 px `#6E6B66`. Hover `#F1EFFD`. Trennlinien 1 px `#E4E1DB`.
   Die Leiste ist ausblendbar (siehe Ansicht-Menü).
3. **Werkzeugleiste**, Höhe 46 px, `#EDEBE7`. Links Reiter **Mapping | Editor | Export**
   (12,5 px / 500; aktiv: Fläche `#FBFAF8`, Text `#191817`, 2 px Unterstrich `#7A6CF0`).
   Rechts: Play/Stop (34 × 30 px), LOOP-Schalter, Beschriftung `MASTER`, Master-Pegel
   (88 × 6 px Schiene) mit dB-Anzeige, Knopf **Exportieren** (`#7A6CF0`, weiß, Hover `#5546CE`).
4. **Arbeitsbereich**: linke Spalte 264 px, Mitte flexibel, rechte Spalte 286 px.
   Beide Spalten sind ausblendbar.
5. **Statusleiste**, Höhe 30 px, `#F4F3F0`, Mono 10,5 px: links Kontext
   („Zone … · Spur …“ bzw. beim Tastenklick „Note C2 → Sub Sustain“),
   rechts `44.1 kHz · 24 Bit · Puffer 256`, `RAM 184 MB`, `CPU 9 %`.

### 1. Mapping (Startansicht)

**Zweck:** Zonen über die Tastatur verteilen – das Herz des Instruments.

- Kopfzeile: Titel „Tastatur-Zonen“ (13 px / 600), Hinweis in Mono 10 px
  „Klick wählt · Doppelklick öffnet den Editor · Samples auf Zone oder Taste ziehen“,
  rechts der Segmentschalter **„Tonhöhen / Drumset“** und davor der Knopf
  „Zone hinzufügen“ (im Drumset „Teil hinzufügen“).
- **Zonenraster**: Fläche `#FBFAF8`, Rand `#D6D3CD`. X-Achse = Tonhöhe C1–C7 (MIDI 24–96),
  Y-Achse = Velocity 127 (oben) bis 0 (unten), Achsenbeschriftung 30 px breit links.
  Oktav-Trennlinien 1 px `#E4E1DB`, gestrichelte Mittellinie bei Velocity 64.
  Zonen sind Rechtecke: Breite = Tastenbereich, Höhe = Velocity-Bereich.
  Ausgewählt: `#EEEBFE` mit Rand und Innenring `#7A6CF0`; sonst `#F3F1ED`, Rand `#D6D3CD`.
  Beschriftung: Name 11,5 px / 500, darunter Mono 10 px `C1–B2 · 3 Smp`
  (nur wenn die Zone breit genug ist, ≥ 17 % der Achse).
- **Klaviatur**, Höhe 92 px: 42 weiße Tasten (6 Oktaven, `#FDFDFC`, Trennlinie `#DCD9D3`),
  30 schwarze Tasten (`#26241F`, Breite 62 % einer weißen Taste, Höhe 56 px).
  Tasten der gewählten Zone sind eingefärbt (weiß `#EEEBFE`, schwarz `#4A4380`),
  angeschlagene Taste `#7A6CF0` für 340 ms. C-Tasten tragen Mono-Label 10 px.
- **Samples hineinziehen**: eine Zeile aus dem Sample-Browser auf eine Zone gezogen wird
  dort eine Spur (eine leere Spur wird gefüllt). Auf freie Fläche gezogen, entsteht eine
  neue Zone um die Taste herum – so breit, wie Platz ist, höchstens eine Oktave, im freien
  Velocity-Bereich. Auf eine Taste der Klaviatur gezogen: in deren Zone, sonst ebenso eine
  neue.
- **Überlappende Zonen**: wo sich Zonen in Tasten und Velocity überschneiden, klingen alle,
  und die Überschneidung ist schraffiert. Ragt eine nach oben über die andere hinaus,
  blendet über den gemeinsamen Velocity-Bereich die untere aus und die obere ein (gleiche
  Leistung); liegt eine ganz in der anderen, klingen beide voll (Schichtung). Die gewählte
  Zone wird obenauf gezeichnet. Die Ansicht wechselt dabei **nicht**; stattdessen trägt eine Zone mit Samples eine
  Marke mit ihrer Anzahl und unten eine kleine Wellenform, und sie leuchtet nach dem
  Ablegen kurz auf.
- Beispieldaten: 5 Zonen – Sub Sustain (C1–B2, Velocity 0–127), Hybrid Mid (C3–F#4, 0–86),
  Hybrid Mid · hart (C3–F#4, 87–127), Air Top (G4–B5), FX Tail (C6–B6).

#### 1b. Schlagzeug-Ansicht (Drumset)

**Zweck:** Ein Instrument bauen, dessen Tasten *verschiedene Dinge* auslösen statt
desselben Klangs in anderer Höhe.

Die Art des Instruments (`InstrumentKind`) steht in der Projektdatei und wird beim
Anlegen abgefragt; umstellen lässt sie sich jederzeit über den Segmentschalter im
Mapping. Sie ist **nicht** bloß eine Ansicht:

| | Tonhöhen-Instrument | Drumset |
| --- | --- | --- |
| Taste | verstimmt das Sample um den Abstand zum Grundton | lässt es in seiner eigenen Tonhöhe |
| Zone | ein Tastenbereich × Velocity-Bereich | **ein** Kit-Teil auf **einer** Taste |
| Spur-Tonhöhe / Cent | wirkt | wirkt ebenfalls |

- **Gezeichnetes Schlagzeug von oben**, mittig, Seitenverhältnis 1,4 : 1. Becken als flache
  Ellipsen mit zwei Rillen, Kessel als volle Ellipsen.
- **Welche Teile aufgebaut sind**, steht im Instrument (`kitPieces`). Ein neues Kit hat
  Kick, Snare, HiHat, drei Toms, Crash und Ride. Über **„Teil hinzufügen“** kommen dazu:
  Crash 2, China, Splash, bis zu zwei weitere Toms (höchstens fünf), Clap, Cowbell und
  Tamburin. **Rechtsklick** auf ein Teil nimmt es samt seinen Zonen wieder heraus
  (Strg+Z holt es zurück). Hängetoms teilen sich den Bogen über der Bassdrum, je nach
  Anzahl; jedes Becken hat seinen festen Platz.
- **Spielweisen**: HiHat (zu · offen · Fuß), Snare (Fell · Rim · Klick · Flam) und Ride
  (Fläche · Glocke) stehen nur einmal da, mit einem Umschalter darunter. Jede Spielweise ist
  trotzdem eine eigene Zone auf eigener Note – sonst bekäme eine DAW sie nicht auseinander.
  Ein Punkt auf dem Umschalter zeigt, welche Spielweise schon ein Sample hat. Der frühere
  „Sidestick“ ist jetzt der Rim-Click der Snare; der Flam hat in General MIDI keine Note
  und liegt auf G1 (31).
- Ein Teil **mit** Zone: Fläche in der Zonenfarbe (kräftiger, sobald ein Sample darin
  liegt), durchgezogener Rand. Ein Teil **ohne** Zone: `#F3F1ED` mit gestricheltem Rand
  `#BFBBB3`. Ausgewählt: 2-px-Rand und Beschriftung in `#7A6CF0`.
- Beschriftung: Name 11 px (500, wenn ein Sample da ist), darunter die Note in Mono
  9,5 px und, wenn Platz ist, der Name des Samples. Oben rechts am Rand eine Marke mit der
  Zahl der Samples.
- **Klick** schlägt das Teil an *und* wählt es; hatte es noch keine Zone, bekommt es sie
  dabei. **Doppelklick** öffnet den Editor. Ein Sample kommt am schnellsten per
  **Ziehen aus dem Browser** auf das Teil (oder auf eine Spielweise im Umschalter); die
  Ansicht bleibt dabei, das Teil leuchtet kurz auf.
- Die Noten folgen **General MIDI** (Kick 36, Snare 38, HiHat zu 42 …). Wer ein fertiges
  Drum-Pattern aus seiner DAW daraufspielt, erwartet genau das. Verschieben lässt sich
  die Note trotzdem, sie hängt an der Zone.
- Velocity-Achse und „Zone hinzufügen“ entfallen hier: ein Kit-Teil sitzt auf einer Taste.

### 2. Editor (Spuransicht, Cubase-ähnlich)

**Zweck:** Innerhalb einer Zone mehrere Spuren (Layer) zeitlich anordnen, schneiden und mischen.

- Kopfzeile: Rücksprung „‹ Mapping“, „Zone <Name>“, Hinweistext (wechselt mit dem Modus),
  rechts: Segmentschalter **Zuschneiden | Stretchen**, Zoom **− / +**, Knopf **Raster**
  (zeigt die Rasterweite; Klick öffnet die Auswahl: aus, 10 ms, 50 ms, 100 ms, 250 ms,
  0,5 s, 1 s), **Spur hinzufügen**, **Zone bouncen** (dunkel `#191817`, weiße Schrift).
- **Zeitlineal**: links über den Spurköpfen die Anzeige des Locators („AB 2.50 s“), danach
  die Zeit des sichtbaren Ausschnitts (Mono 10 px); die Striche werden mit dem Zoom feiner
  oder gröber (10 ms bis 1 min). Ohne Zoom sind **8 Sekunden** sichtbar, hineingezoomt bis
  0,25 s, herausgezoomt bis 32 s – mit **Strg+Mausrad** um den Zeiger oder mit − / +. Die
  Achse wächst mit dem letzten Clip (bis 128 s). Ein **waagerechter Rollbalken** unter den
  Spuren erscheint, sobald es mehr gibt; Shift+Mausrad und waagerechtes Wischen rollen
  ebenfalls, während der Wiedergabe blättert die Ansicht mit.
- **Locator**: Klick oder Ziehen im Lineal setzt ihn (am Raster, wenn es an ist), ein
  Klick auf die Anzeige links setzt ihn auf 0. Wiedergabe beginnt und loopt ab dort. Der
  Locator steht auch im Sample-Editor und lässt sich dort ziehen – es ist derselbe.
- **Spuren**, je 78 px hoch:
  - **Spurkopf** 140 px: Farbquadrat 8 px, Name, darunter M- und S-Knopf (je 21 × 18 px;
    M aktiv `#B4553F`, S aktiv `#7A6CF0`) und rechts der Pegel in dB. Gewählte Spuren
    haben einen Balken in Akzentfarbe links. **Doppelklick** auf den Kopf benennt die Spur
    um; **Rechtsklick** öffnet das Spur-Menü (Spur löschen, Spur hinzufügen).
  - **Lane**: Rasterlinien alle 1 s (`#E1DED8`). Eine Spur trägt **beliebig viele Clips**.
    Clip: Hintergrund = helle Spurfarbe, 1 px Rand in Spurfarbe, gewählt 2 px `#3B2FA8`.
    Kopfstreifen 15 px in Spurfarbe mit Samplename und Stretch-Badge (`1.28×`, bei einem
    normalisierten Clip davor der Pegel, `+3.2 dB · 1.00×`), darunter
    die Wellenform als Balken in Spurfarbe. Eine leere Spur zeigt „Sample aus der Liste
    hierher ziehen“.
  - **Überlappende Clips**: es klingt der obere, also zuletzt gesetzte (auch ein
    verschobener Clip kommt nach oben). Die Überschneidung ist **schraffiert**. In den
    Fades des oberen Clips scheint der untere durch – so entsteht ein Crossfade.
  - **Fade-Flächen**: Dreiecke `#FBFAF8` bei 82 % Deckkraft über Anfang und Ende des Clips.
  - **Griffe**: links/rechts je 7 px breite unsichtbare Kantenzonen (Cursor `ew-resize`);
    an den beiden oberen Ecken je ein 11 px großes Quadrat in Spurfarbe mit weißem Rand
    für Fade-in und Fade-out.
  - Locator 1 px Akzentfarbe, Wiedergabelinie 1 px `#191817`.
- **Werkzeugleiste**: rechte Maustaste in der Zeitleiste **halten** öffnet sie über dem
  Zeiger; Loslassen über einem Symbol wählt das Werkzeug. Nur kurz geklickt, bleibt sie
  offen (Linksklick wählt, Klick daneben oder Esc schließt). Werkzeuge: **Auswahl** (Pfeil,
  normales Verhalten), **Löschen** (Radiergummi; Klick oder Wischen löscht Clips),
  **Schere** (Zeiger mit senkrechtem Strich; Klick schneidet genau dort, am Raster, wenn
  es an ist – das Sample läuft über den Schnitt nahtlos weiter).
- **Sample-Editor unten**, feste Höhe 186 px, `#F1EFEB`, Oberlinie `#BFBBB3`: zeigt den
  **gewählten Clip**. Kopfzeile mit Samplename, Auswahlbereich in Sekunden und Werkzeugen
  (Auswahl, Zuschneiden, Fade ein, Fade aus, Normalisieren, Umkehren; aktiv `#EEEBFE`
  mit Rand `#7A6CF0`, Text `#3B2FA8`). Darunter die **volle** Wellenform des Samples; was
  außerhalb des Zuschnitts liegt, tritt zurück, die Auswahl ist violett. Loop-Bereich und
  Überblendungen (X-FADE, schraffiert) sind **grün** und liegen immer im gespielten Teil.
  Oben im Wellenfeld ein Streifen für den Locator. **Normalisieren** hebt den lautesten Punkt
  des gespielten Ausschnitts auf 0 dBFS (höchstens +24 dB) – als Pegel des Clips, die Datei
  bleibt unberührt; ein zweiter Klick nimmt es zurück.
- **Linke Spalte zeigt in dieser Ansicht die gewählte Spur** (nicht die Sample-Liste):
  Gain, Pan, Fade-Längen des gewählten Clips, Tonhöhe in Halbtönen (Stepper) und Cent,
  Stretch-Faktor mit „1:1“-Rückstellknopf, Algorithmus-Auswahl (Transienten-treu /
  Glatt (Pad) / Monophon / Korn · Granular), **„Tempo beim Transponieren halten“**,
  Umkehren (je **Clip**: der gewählte bzw. alle gewählten Clips), Loop-Modus (One-Shot / Sustain-Loop / Vor-Rückwärts), bei Schleifen
  **Loop ab** und **X-Fade**, und die **Effektkette der Spur**.
- **Transponieren**: klassisch (höher = schneller, wie jeder Sampler) oder mit gehaltenem
  Tempo – dann transponiert das Körnerverfahren des gewählten Algorithmus, und die Dauer
  bleibt. Je Spur einstellbar, für alle Spuren auf einmal im Menü „Instrument“. Klassisch
  läuft die **ganze Anordnung** mit, wie ein Tonband: eine Oktave höher beginnen auch
  versetzte Clips doppelt so früh, und Überschneidungen und Crossfades passen bei jeder
  Taste. Die Zeitachse des Editors zeigt den Grundton.
- **Schleifen**: „Loop ab“ legt fest, wo die Schleife beginnt (davor klingt der Anschlag
  einmal), „X-Fade“ blendet an der Naht über. Eine Schleife blendet nur einmal ein und nie
  aus, und sie klingt bis zum nächsten Clip der Spur.

### 3. Export

**Zweck:** Das fertige Instrument ausgeben.

- Vier Kennzahlen-Karten (Zonen, Samples, Spuren in Zone, Größe): Fläche `#FBFAF8`,
  Rand `#D6D3CD`, Label Mono 10 px versal, Wert 19 px / 600.
- Drei Zielformate als anklickbare Zeilen (max. 620 px breit), je mit 15 px Kästchen,
  Name, Erläuterung und Größe; ausgewählt `#EEEBFE` mit Rand `#7A6CF0`:
  - VST3-Instrument — Cubase, Live, Reaper, Studio One
  - Standalone-App — ohne DAW spielbar
  - Projektdatei **.sisp** — weiterbearbeiten, teilen

  > Der Prototyp zeigte hier vier Zeilen; **Audio-Unit-Instrument** ist entfernt, weil das
  > Format nur unter macOS existiert und es keine Mac-Fassung gibt. Sollte je an einer
  > gearbeitet werden, gehört die Zeile zwischen VST3 und Standalone zurück — der Weg wäre
  > derselbe wie beim VST3 (Bundle kopieren, Kennung setzen, Instrument in die Resources),
  > zusätzlich müsste der Kennungs-Patch in der `Info.plist` greifen.
- Zielpfad-Feld + „Ordner …“, darunter Knopf „Exportieren“ mit Fortschrittsbalken und
  Prozentanzeige (im Prototyp simuliert, ca. 1,5 s bis „fertig“).

### 4. Linke Spalte: Sample-Browser (Mapping und Export)

Kopf „SAMPLES“ (Mono 10,5 px versal), Suchfeld, Liste mit je einer 42 × 22 px
Mini-Wellenform, Dateiname 12 px und Mono-Metazeile (`4.21 s · 24 Bit`).
Ausgewählter Eintrag `#EEEBFE`, Hover `#E9E7E2`. Unten eine gestrichelte Ablagefläche:
„Dateien hierher ziehen“ / „WAV · AIFF · FLAC · MP3“. Jede Zeile lässt sich in die Mitte
ziehen – aufs Mapping (Zone, Kit-Teil, Taste) oder im Editor auf eine Spur; Doppelklick
legt das Sample als Spur in die gewählte Zone und wechselt in den Editor. In der
Editor-Ansicht steht der Browser rechts.

### 5. Rechte Spalte: Zone

Kopf „ZONE“, darunter Name mit Farbpunkt und vier Felder im 2 × 2-Raster
(Grundton, Bereich, Velocity, Samples; Mono 12 px auf `#FBFAF8`).
Abschnitt **Hüllkurve**: 56 px hohe Fläche mit der ADSR-Kurve als gefülltes Polygon
(`#7A6CF0`, 22 % Deckkraft, per `clip-path` aus den vier Werten berechnet), darunter vier
Schieber A/D/S/R mit Werten in ms bzw. dB.
Abschnitt **Makros am Instrument**: vier Zeilen (Filter-Farbe, Attack-Anteil,
Luft / Obertöne, Hall-Anteil) mit je einem 100 px Schieber und Prozentwert.

### 6. Plugin-Ansicht

Fenster maximal 960 px breit, Rand `#BFBBB3`, auf grauem Grund `#D9D7D2` zentriert.

- **Host-Leiste** 30 px, `#CFCCC6`, Mono 10 px: links `HOST · INSTRUMENT-SPUR 3`,
  rechts `SISTUDIO VST3` und der Knopf `IM STUDIO ÖFFNEN ↗`.
- **Kopf**: App-Icon 18 px, Instrumentname, rechts Preset-Auswahl und Play/Stop. Die
  Preset-Auswahl listet die `.sisp`-Dateien aus *Eigene Dokumente / Sample Instrument
  Studio / Presets*, sichert den aktuellen Stand dorthin und öffnet den Ordner. Play hält
  den Grundton der gewählten Zone — einen eigenen Transport gibt es nicht, den gibt die
  DAW vor.
- **Zonenstreifen** 50 px: dieselben Zonen wie im Mapping, nur horizontal, mit Name und
  Tastenbereich. Bei einem **Drumset** stattdessen gleich breite **Pads**, nach Note
  geordnet – ein Kit-Teil ist nur eine Taste breit und wäre sonst ein Faden. Hinweis
  daneben: „Doppelklick öffnet die Bearbeitung“.
- **Makros**: vier Drehregler 44 px (Kreis, Rand `#BFBBB3`, Zeiger 2 × 17 px `#7A6CF0`,
  Bereich −140° bis +140°), vertikal ziehbar.
- **Klaviatur** 72 px, gleiche Logik wie im Standalone.
- **Fußzeile** 26 px mit Kontext und Stimmenzahl.
- **Zonen-Dialog** (Doppelklick auf eine Zone): überlagert das Fenster
  (`rgba(25,24,23,0.28)`), Karte max. 720 px. Inhalt: Kopf mit Zonenname und Tastenbereich,
  Liste der Spuren (Farbe, Name, Mini-Wellenform, Pegel, M-Knopf, Klick wählt die Spur),
  darunter ein **vollwertiger Sample-Editor** (96 px hohe Wellenform mit Auswahlgriffen und
  denselben sechs Werkzeugen wie im Standalone), Fußzeile mit Hinweis und Knopf
  „Im Studio bearbeiten“.
- **Exportierte App:** zeigt diese Oberfläche statt des Studios – wer ein fertiges Instrument
  bekommt, will es spielen. Ohne Host-Leiste und ohne „Im Studio öffnen“ (der Empfänger hat das
  Studio meist nicht), dafür mit „Audio …“ im Kopf für Ausgabegerät und MIDI; das Fenster ist
  960 × 560 px groß und frei skalierbar.
- **Richtungsentscheid:** Es gibt nur den Weg Plugin → Standalone. Ein Schalter
  Standalone → Plugin wäre sinnlos, weil dafür erst eine DAW geöffnet werden müsste.
- **Wie die Übergabe läuft:** Aus dem Plugin heraus ist die App nicht zu finden — die
  eigene Programmdatei ist die der DAW. Geraten wird deshalb nicht: die App **merkt sich
  bei jedem Start ihren Pfad** in der gemeinsamen Einstellungsdatei, das Plugin liest ihn
  dort und startet sie mit einer `.sisp` im Temp-Ordner als Argument (`Source/StudioHandover.h`).
  Wer die App noch nie gestartet hat, bekommt einen Hinweis statt eines Fehlversuchs.
- **Ansehen ohne DAW:** Mit gesetzter Umgebungsvariable `SIS_PLUGIN_VIEW` zeigt auch die
  App das Plugin-Fenster, mit dem Wert `sheet` gleich den Zonen-Dialog. Sonst bliebe diese
  Ansicht beim Entwickeln ungeprüft.

### 7. Die Effekte als eigenständige VST3

Jeder interne Effekt gibt es auch einzeln, zum Einsetzen in eine beliebige DAW — 21
Plugins plus den „SIS Equalizer“, der seine eigene Kurvenanzeige behält.

- **Ein Quelltext für alle.** Welcher Effekt daraus wird, sagt `SIS_EFFECT_TYPE` beim
  Übersetzen. Regler, Umrechnung und Rechnen kommen aus denselben Funktionen wie die
  Effektkette des Studios. „SIS Chorus“ klingt deshalb nicht *ähnlich* wie der Chorus im
  Studio — es ist derselbe Code.
- **Fenster** 360 px breit, in der Gestalt der Effektkarte: Kopf mit Name und Schalter
  „UMGEHEN“, darunter die Preset-Leiste, dann eine Zeile je Regler (Beschriftung Mono
  10 px links, Schieber, Wert in Prozent rechts), unten die Aussteuerungsanzeige.
- **Presets**: der Knopf „Presets“ öffnet die gesicherten Einstellungen, „Sichern unter …“,
  „Löschen“ und „Grundstellung“. Die Ablage ist **dieselbe Datei** wie im Studio: was am
  Plugin gesichert wird, steht in der Effektkarte im Studio und umgekehrt.
- Dazu kommen die Presets des Hosts von allein: der Zustand hängt an einem
  `AudioProcessorValueTreeState`.
- Gebaut werden sie **nicht** beim normalen Bauen — 21 JUCE-Plugins dauern ein Vielfaches
  des restlichen Projekts. Alle auf einmal: `build.ps1 -Target SisEffectPlugins`.

### Intern oder fremd — und warum das Menü es nicht zeigt

Im „+“-Menü der Effektkette stehen die eigenen Effekte und die installierten VST3
**zusammen**; letztere unter „Installierte Plugins“. Beim Aussuchen eines Klangs hilft die
Unterscheidung niemandem.

Im Unterbau bleibt sie trotzdem, aus vier Gründen:

| | intern | gehostet |
| --- | --- | --- |
| Export | wandert im Instrument mit | muss auf dem fremden Rechner installiert sein |
| Projektdatei | ein paar lesbare Zahlen | undurchsichtiger Zustands-Klumpen |
| Laufzeit | keine Plugin-Instanz je Spur | eine Instanz je Spur und Effekt |
| Tests | direkt prüfbar | nur von Hand |

Der Export ist der gewichtigste: ein exportiertes VST3-Instrument soll auf einem fremden
Rechner klingen. Deshalb sind die **eigenen** Effekt-VST3 im Menü „Installierte Plugins“
ausgeblendet — sie stehen schon oben als interne Effekte, und dort sind sie die bessere
Wahl. An der Karte des eingefügten Effekts steht weiterhin, woher er kommt (`Intern` bzw.
`VST3 · extern`); dort ist die Angabe nützlich, weil sie sagt, was beim Export passiert.

## Interaktionen & Verhalten

### Clips im Editor (zentrale Interaktion)

| Geste | Wirkung |
| --- | --- |
| Klick auf einen Clip | wählt ihn (und seine Spur); **Shift+Klick** erweitert oder verkleinert die Auswahl |
| Klick auf einen Spurkopf | wählt die Spur; **Shift+Klick** erweitert die Spurauswahl |
| Clip-Körper ziehen | verschiebt alle gewählten Clips, waagerecht **und** auf andere Spuren; unter die letzte Spur gezogen entsteht eine neue |
| Kante ziehen | **Zuschneiden**: verschiebt `trimStart` / `trimEnd`, das Sample behält sein Tempo |
| **Shift** + Kante ziehen | **Time-Stretch**: ändert `stretch` (0,25×–4×) |
| Obere Ecke nach innen ziehen | Fade-in (links) bzw. Fade-out (rechts), 0–100 % der Clip-Länge |
| Entf | löscht die gewählten Clips – **nie** eine Spur |
| X | Crossfade über die ganze Überschneidung zweier gewählter Clips derselben Spur |
| Strg+C / Strg+X / Strg+V | kopieren, ausschneiden, einfügen (siehe unten) |
| Rechte Maustaste halten | Werkzeugleiste: Auswahl, Löschen, Schere |
| Rechtsklick auf den Spurkopf | Spur-Menü: Spur(en) löschen, Spur hinzufügen |
| Doppelklick auf den Spurkopf | Spur umbenennen (Enter übernimmt, Esc bricht ab) |
| Sample aus dem Browser auf eine Spur ziehen | neuer Clip obenauf, an der Stelle des Zeigers; darunter gezogen: neue Spur |

Der Segmentschalter „Zuschneiden / Stretchen“ legt fest, was eine Kante ohne Zusatztaste
tut; Shift kehrt die Belegung um. Auf dem Clip-Körper erweitert Shift dagegen die Auswahl.
Bei aktivem **Raster** rasten Verschieben, Kanten, Schere und Locator auf die gewählte
Rasterweite ein (voreingestellt 0,5 s).

**Einfügen**: der früheste Clip der obersten kopierten Spur landet am Locator auf der
gewählten Spur; alle anderen behalten Zeit- und Spurabstand zu ihm. Fehlen darunter Spuren,
entstehen sie.

Geometrie: `Clip-Länge = natural × stretch × (trimEnd − trimStart)`, alle Werte als Anteil
der 8-Sekunden-Achse. Beim Ziehen der linken Kante wandert `offset` mit, sodass das rechte
Clip-Ende stehen bleibt. Die reinen Regeln (auch Schere, Crossfade und welche Abschnitte
eines überdeckten Clips hörbar sind) stehen ohne JUCE in `Source/Model/ClipGeometry.h`.

### Weitere Interaktionen

- **Zonen**: Klick wählt, Doppelklick öffnet den Editor (Standalone) bzw. den Zonen-Dialog (Plugin).
- **Klaviatur**: Mausklick hebt die Taste 340 ms hervor und schreibt „Note … → Zone …“ in die Statusleiste.
- **Schieber**: alle Regler reagieren auf Ziehen, Drehregler im Plugin auf vertikales
  Ziehen (160 px = voller Weg).
- **Effekte**: Kippschalter schaltet einzelne Effekte stumm (Zeile wird grau), „+“ öffnet das
  nach Familien gegliederte Menü mit den internen Effekten und den installierten VST3.
  Externe Effekte tragen das Kürzel `VST3 · extern` in `#5546CE` und einen Knopf „Öffnen“
  für die Original-Oberfläche des Plugins.
- **Werkzeuge im Sample-Editor**: „Zuschneiden“ übernimmt die Auswahl als neuen Clip-Ausschnitt,
  „Fade ein / aus“ setzen Startwerte, die man danach an den Clip-Ecken feinjustiert.
- **Rückmeldung**: kurze Einblendung unten mittig (`#191817`, weiße Schrift, 2,2 s).
- **Wiedergabe**: Play spielt die gewählte Zone ab dem Locator, mindestens 8 s, bei längeren
  Clips bis zu ihrem Ende; LOOP wiederholt ab dem Locator.

### Tastenkürzel

`Strg+M` Menüleiste, `F9` linke Spalte, `F10` rechte Spalte, `F2/F3/F4`
Mapping/Editor/Export, `F12` Audio-Einstellungen, `Leertaste` Wiedergabe. Wird ein Bereich
ausgeblendet, nennt die Einblendung das Kürzel zum Zurückholen. Datei: `Strg+N/O/S`,
`Strg+Shift+S`, `Strg+I` (Samples importieren), `Strg+E` (Exportieren); Bearbeiten:
`Strg+Z/Y`, `Strg+X/C/V` (Clips im Editor). Im Editor: `Entf` löscht gewählte Clips, `X`
legt einen Crossfade, `Esc` hebt die Auswahl auf bzw. schließt die Werkzeugleiste.
Die Tasten A S D F G H J K L spielen die weißen, W E T Z U O die schwarzen Tasten ab der Oktave der gewählten Zone, solange kein Textfeld den Fokus hat.

### Menüstruktur

- **Datei**: Neues Instrument, Öffnen … (.sisp), Speichern, Speichern unter …, │
  Samples importieren …, Instrument exportieren …, │ Beenden
- **Bearbeiten**: Rückgängig, Wiederherstellen, │ Ausschneiden, Kopieren, Einfügen, │
  Auf Auswahl zuschneiden
- **Ansicht**: Mapping, Editor, Export, │ Menüleiste, Linke Spalte, Rechte Spalte, │
  Velocity-Layer, Am Raster einrasten (die letzten fünf mit Haken)
- **Instrument**: Zone hinzufügen, Spur hinzufügen, │ Zone bouncen, Makros zuweisen …, │
  Tempo halten: alle Spuren ein, Tempo halten: alle Spuren aus

Das „+“ der Effektkette öffnet ein nach Familien gegliedertes Menü: oben der
**Kanal-Streifen** als schnellster Weg, darunter die Untermenüs *Filter & Klangregelung,
Verzerrung, Modulation, Zeit & Raum, Dynamik*, unten *Externes Plugin …*.
- **Hilfe**: Handbuch, Tastaturkürzel, │ Über …, Credits

## State

Der Prototyp hält folgenden Zustand; die Namen taugen als Ausgangspunkt für das Datenmodell.

**Global:** `view` (mapping | editor | export), `shell` (standalone | plugin),
`menu` (offenes Menü), `showMenubar`, `showLeft`, `showRight`, `edgeMode` (trim | stretch),
`snap`, `velLayers`, `playing`, `ph` (Laufmarke 0–1), `loopOn`, `gain`, `toast`.

**Instrument:** `zones[]` mit `{ id, name, lo, hi, vlo, vhi, root, samples, color }`,
`zone` (gewählte Id), `adsr {a,d,s,r}`, `macro {m1..m4}`,
`targets {vst3, au, standalone, pack}`, `exporting`, `prog`.

**Spuren der gewählten Zone:** `tracks[]` mit
`{ name, clip, color, soft, shape, off, nat, stretch, trimA, trimB, fadeIn, fadeOut,
gain, pan, pitch, cents, reverse, loop, algo, mute, solo, fx[] }`,
`track` (Index), `sel` ([Anfang, Ende] der Auswahl im Sample-Editor), `tool`.
Effekt: `{ name, kind, on, ext, params:[{l, v}] }`.

**Hinweis:** Im Prototyp hängen die Spuren global am Zustand, nicht an der einzelnen Zone,
und jede Spur hat genau einen Clip. In der Umsetzung gehört `tracks` in die Zone, und die
Clip-Werte (`sample, offset, natural, stretch, trimStart, trimEnd, fadeIn, fadeOut`) stehen
in `Clip`, von denen eine Spur beliebig viele trägt (`Source/Model/Instrument.h`).
Projektdateien bis 1.11 mit dem Clip in der Spur werden weiterhin gelesen.

## Design-Tokens

**Farben**

| Rolle | Wert |
| --- | --- |
| Fensterleiste | `#DEDBD5`, Rand `#C4C0B8` |
| Menü- und Werkzeugleiste | `#EDEBE7` |
| Panels links/rechts, Statusleiste | `#F4F3F0` |
| Arbeitsfläche Mitte | `#E9E7E3` |
| Flächen, Felder, Karten | `#FBFAF8` |
| Untere Zone (Sample-Editor) | `#F1EFEB` |
| Linien | `#E1DED8` (fein), `#D6D3CD` (normal), `#BFBBB3` / `#C4C0B8` (stark) |
| Schrift | `#191817` (primär), `#56534E` (sekundär), `#6E6B66` (tertiär) |
| Akzent | `#7A6CF0`, Hover/Text auf hell `#5546CE`, dunkel `#3B2FA8`, Fläche `#EEEBFE` / `#F1EFFD` |
| Spurfarben | `#7A6CF0` (hell `#EEEBFE`), `#2E9E96` (`#E2F2F0`), `#C0862E` (`#FAF1DE`), `#B4553F`, neutral `#6E6B66` (`#EFEEEA`) |
| Warnung / Mute | `#B4553F` |
| Wellenform inaktiv | `#C9C6BF`, außerhalb des Ausschnitts `#E4E1DB` |
| Schwarze Tasten | `#26241F`, in der Zone `#4A4380` |

**Typografie**

- Oberflächenschrift: **IBM Plex Sans** — 13 px/600 Überschriften, 12,5 px/500 Reiter und
  Knöpfe, 12 px Fließtext, 11,5 px kompakte Beschriftungen.
- Zahlen, Werte, Labels: **IBM Plex Mono** — 12 px Werte, 10,5 px Statuszeile,
  10 px Kleinbeschriftung (Minimum). Versale Abschnittstitel: 10 px, `letter-spacing: 0.09em`,
  `text-transform: uppercase`, Farbe `#56534E`.
- Kein Text unter 10 px; Kontrast mindestens 4,5:1.

**Maße**

- Leistenhöhen 34 / 26 / 46 / 30 px; Panels 264 px links, 286 px rechts;
  Spurkopf 140 px; Spurhöhe 78 px; Clip-Kopfstreifen 15 px; untere Zone 186 px.
- Innenabstände 7 / 8 / 12 / 13 px, Abstände zwischen Elementen 6 / 8 / 10 px.
- **Keine Rundungen** (`border-radius: 0`) außer bei den Drehreglern (Kreis).
- **Keine Schatten** außer bei Menüs (`0 10px 26px rgba(25,24,23,0.16)`),
  Dialogen (`0 18px 44px rgba(25,24,23,0.28)`) und der Einblendung.
- Schieber: Schiene 5–6 px hoch, Farbe `#DCD9D3`, Füllung Akzent oder Spurfarbe.

## Assets

- `assets/sis_icon.png` — Programm-Icon (violette Form mit Wellenform/Tastatur),
  vom Auftraggeber geliefert. Verwendung: Fensterleiste (17 px), Plugin-Kopf (18 px),
  zusätzlich Taskleiste, Desktop, Installer. Für die Umsetzung werden die üblichen
  Größen gebraucht (16/24/32/48/256 px, `.ico` für Windows, `.icns` für macOS).
- Schriften: IBM Plex Sans und IBM Plex Mono (SIL Open Font License), im Prototyp über
  Google Fonts geladen, in der Anwendung mitzuliefern.
- Keine weiteren Bilder; alle Wellenformen sind aus Daten gezeichnet.

## Offene Entwurfsfragen

- Zeitbasis: bewusst **Sekunden**, nicht Takte – ein Instrument löst Klänge je Note aus und
  kennt das Songtempo nicht. Takte lohnen erst, wenn tempo-gebundene Dinge dazukommen
  (Loops, die zum Song passen sollen; Delay oder LFOs im Songtempo), und dann über das Tempo
  der DAW, nicht über eine eigene BPM-Einstellung.

## Dateien

- `Sample Instrument Studio.dc.html` — der vollständige interaktive Prototyp
  (Standalone und Plugin; die Plugin-Ansicht lässt sich im Code über den Anfangswert
  `shell: 'plugin'` zeigen). Im Browser direkt zu öffnen.
- `assets/sis_icon.png` — Programm-Icon.
- `docs/Handbuch.html` — das Handbuch für Anwender (F1 im Programm).

## Versionen

Die Versionsnummer wird an **einer** Stelle gepflegt: `project(... VERSION x.y.z)` in
`CMakeLists.txt`. JUCE macht daraus `JucePlugin_VersionString`, das `Source/Version.h`
weiterreicht; die Credits zeigen es an. Beim Ändern mitziehen: **x** großer Umbau oder Bruch
mit älteren Projektdateien, **y** neue Funktionen, **z** Fehlerbehebungen.

| Version | Was dazugekommen ist |
| --- | --- |
| 1.16.0 | Überlappende Velocity-Zonen klingen gemeinsam, mit Überblendung über den gemeinsamen Bereich (Schraffur im Mapping); Umkehren je Clip statt je Spur; Handbuch (F1); exportierte App zeigt die Spiel-Oberfläche mit Audio-Einstellungen statt des Studios |
| 1.15.0 | Zoom auf der Zeitachse (Strg+Mausrad, − / +), Raster wählbar von 10 ms bis 1 s; Normalisieren als Pegel des Clips; klassisches Transponieren nimmt die ganze Anordnung mit (Überschneidungen passen bei jeder Taste); Drumset-Pads in der Plugin-Ansicht; Kopieren, Einfügen und Verschieben über Spuren im Modell und getestet |
| 1.14.0 | Menü „Instrument“: „Tempo halten“ für alle Spuren auf einmal ein- oder ausschalten (einzeln bleibt es im Inspektor umstellbar) |
| 1.13.0 | Spur per Doppelklick auf den Spurkopf umbenennen; Schalter „Tempo beim Transponieren halten“ je Spur (Tonhöhe ohne Tempowechsel über das Stretch-Verfahren); Umlaute in Großbuchstaben-Überschriften korrigiert |
| 1.12.0 | Beliebig viele Clips je Spur (der obere klingt, Überschneidungen schraffiert, X legt einen Crossfade); Clips und Spuren unabhängig wählbar, Shift für Mehrfachauswahl; Clips auf andere Spuren ziehen (unter die letzte: neue Spur); Entf löscht Clips, Spuren nur per Rechtsklick auf den Spurkopf; Strg+C/X/V am Locator, über mehrere Spuren hinweg (fehlende Spuren entstehen); Werkzeugleiste per gehaltenem Rechtsklick (Auswahl, Löschen, Schere); Locator auch im Sample-Editor; Menü-Kopfzeilen werden gezeichnet; Umlaute in Spur-Menü und EQ-Bändern korrigiert |
| 1.11.0 | Drumset-Teile hinzufügen und entfernen (Becken, bis zu 5 Toms, Percussion), Spielweisen-Umschalter für HiHat, Snare und Ride; Samples aus dem Browser aufs Mapping und auf Spuren ziehen; Loop-Beginn und Überblendung für Sustain-Loop und Vor/Rückwärts (Schleifen pumpen nicht mehr an der Naht); Clip mit Entf entfernen, Spur per Rechtsklick löschen, Locator im Zeitlineal, waagerechtes Scrollen für Clips über 8 s |
| 1.10.0 | Jeder interne Effekt auch als eigenständiges VST3, mit Presets; „+“-Menü zeigt eigene und installierte Plugins zusammen |
| 1.9.0 | Zweite Mapping-Art: Drumset mit gezeichnetem Schlagzeug, Instrumentart in der Projektdatei |
| 1.8.0 | Makro-Zuweisung, volle Plugin-Ansicht, umsortierbare und einklappbare Effekte |
| 1.7.0 | Presets je Effektart: sichern, laden, löschen, Grundstellung |
| 1.6.1 | „+“-Menü nach Familien gegliedert |
| 1.6.0 | Kanal-Streifen: Gate, Klangregelung und Kompressor in einer Karte |
| 1.5.0 | Envelope Filter (folgt dem Anschlag) und Auto-Wah (läuft gleichmäßig im Intervall) |
| 1.4.0 | Noise Gate, Expander, Limiter und De-Esser – die Wunschliste ist damit vollständig |
| 1.3.0 | Delay mit Rückkopplung, Dämpfung und Ping-Pong |
| 1.2.0 | Flanger, Phaser, Vibrato und Tremolo |
| 1.1.0 | Overdrive und Distortion |
| 1.0.0 | Erste vollständige Fassung: Rahmen, Mapping, Editor, Export, Sampler-Engine, acht interne Effekte, Hosting fremder VST3, Bounce, Undo-Verlauf, Export als VST3 und als App |

## Stand der Umsetzung

Die Umsetzung in **C++/JUCE** liegt in `Source/`; wie gebaut und getestet wird, steht in
`BUILDING.md`. Dort ist auch jede Entscheidung begründet, die beim Bauen nicht offensichtlich
war — wer hier weitermacht, liest am besten zuerst dort.

### Fertig

| Bereich | |
| --- | --- |
| Rahmen | Fensterleiste, Menüs, Werkzeugleiste, Statusleiste, Einblendungen, Tastenkürzel |
| Mapping | Zonenraster, Velocity-Achse, Klaviatur, Zonen anlegen, Bereich und Velocity einstellen; überlappende Zonen mit Velocity-Überblendung; Samples auf Zonen und Tasten ziehen, Marke mit Sample-Zahl |
| Drumset | zweite Mapping-Art: gezeichnetes Schlagzeug, ein Kit-Teil je Taste, keine Verstimmung über die Tastatur; General-MIDI-Noten; Teile hinzufügen/entfernen, Spielweisen für HiHat, Snare, Ride |
| Sample-Browser | Suche, Dateien hineinziehen, Import (liest Länge, Bittiefe, Wellenform), Zeilen in die Mitte ziehen |
| Editor | Spuren mit beliebig vielen Clips, Überlappung mit Schraffur, Crossfade (X), Trim/Stretch/Fades, Auswahl mit Shift, Clips über Spuren ziehen, Zwischenablage, Werkzeugleiste (Auswahl, Löschen, Schere), Spuren umbenennen und löschen, Locator, Scrollen und Zoom, wählbares Raster (10 ms … 1 s), Sample-Editor mit Normalisieren, Effektkette, Spur-Parameter |
| Rechte Spalte | Zone, Hüllkurve (ADSR), Makros |
| Sampler-Engine | Zonen und Velocity, mehrere Clips je Spur (nur hörbare Abschnitte werden gespielt), Tonhöhe, Trim, Fades, Loop-Modi mit Loop-Beginn und Überblendung, Panorama, ADSR, 24 Stimmen |
| Time-Stretch | Überlappungsverfahren, vier Körnungen; Dauer ändert sich ohne Tonhöhenänderung – und umgekehrt: Transponieren mit gehaltenem Tempo |
| Wiedergabe | Play / Leertaste ab dem Locator, Pegelanzeige, Audio-Einstellungen (F12) |
| Interne Effekte | **Kanal-Streifen** (Gate + Klangregelung + Kompressor in einer Karte), Kanalfilter, Sättigung, Overdrive, Distortion, Transienten, EQ 4-Band, Envelope Filter, Auto-Wah, Chorus, Flanger, Phaser, Vibrato, Tremolo, Delay, Hall, Noise Gate, Expander, Kompressor, Limiter, De-Esser, Bit-Crusher |
| Effekt-Plugins | **jeder** interne Effekt auch als eigenes VST3 (21 Stück) plus der „SIS Equalizer“ mit eigener Kurvenanzeige; Presets teilen sich die Ablage mit dem Studio |
| Fremde Plugins | VST3-Effekte suchen, einfügen, eigene Oberfläche öffnen, Zustand im Projekt |
| Bounce | Zone samt Effekten in eine Audiodatei rechnen und die Spuren dadurch ersetzen |
| Undo-Verlauf | Strg+Z / Strg+Y, benannte Schritte, 60 Stufen |
| Export | Projektordner (.sisp + Samples), VST3-Instrument mit eigenem Namen und eigener Kennung, eigenständige App mit der Spiel-Oberfläche |
| Credits | Hilfe → Credits mit Entwickler und Version |
| Handbuch | Hilfe → Handbuch (F1): `docs/Handbuch.html`, ins Programm eingebettet und im Browser geöffnet; die Version wird beim Öffnen eingesetzt |
| Presets je Effektart | sichern, laden, löschen; „Grundstellung“ setzt auf die Werte beim Einfügen zurück |
| Makros | Regler mit der rechten Maustaste einem der vier Makros zuweisen; „Makros zuweisen …“ zeigt und löst die Zuweisungen |
| Effektkette | umsortieren (Pfeile), einklappen (Klick auf den Kopf), scrollen; Abschnitte in beiden Spalten einklappbar |
| Plugin-Ansicht | Kopf mit Preset-Auswahl, Zonenstreifen (bei Drumsets als Pads), vier Drehregler, Klaviatur, Zonen-Dialog mit vollem Sample-Editor, Übergabe ans Studio |

### Offen

**Effekte.** Zweiundzwanzig Einträge im „+“-Menü; die ursprünglich genannte Wunschliste ist
**abgearbeitet**. Ein neuer Effekt braucht jeweils: eine Klasse in `Source/DSP/`, einen Wert in `EffectType`
(**hinten anhängen** — die Zahl steht in den Projektdateien), eine Fabrik in `Effect`, die
Umrechnung der Regler in `buildRenderPlan`, einen Platz in `BusProcessors` und einen Fall in
`SamplerEngine::applyEffects`. Dazu ein Test, der die Eigenschaft prüft, die den Effekt
ausmacht — nicht bloß, dass sich etwas ändert.

**Weiteres.**

- **Loop-Modus** gilt für die ganze Spur, nicht je Clip (Umkehren ist je Clip).
- **Werks-Presets** für die Effekte (im Studio und in den Effekt-Plugins) fehlen — die
  Ablage steht, die Vorlagen gehören ans Ohr.
- **Audio Unit** — nur mit einer Mac-Fassung, siehe Export-Ansicht oben.

### Was beim Weitermachen hilft

- `BUILDING.md` erklärt den Bau (`build.ps1`), die Tests und die Architektur: warum der
  Renderplan eine Momentaufnahme ist, warum Effekt-Koeffizienten außerhalb des Audio-Threads
  entstehen, wie die Kennung im Binary gepatcht wird und wie der Undo-Verlauf hängt.
- Drei Testprogramme laufen ohne Fenster und ohne Audiogerät: `SisClipGeometryTests`
  (Clip-Gesten, Raster, Schere, Crossfade, Überdeckung), `SisEngineTests` (Engine, Effekte,
  Kit, Schleifen, Transponieren, Kopieren/Einfügen/Verschieben über Spuren, Normalisieren),
  `SisProcessorTests` (Prozessor, Export, Hosting, Bounce, Undo).
  Sie sind der schnellste Weg, eine Änderung zu prüfen.
- Texte mit Umlauten oder Sonderzeichen brauchen `"…"_u` oder `juce::String::fromUTF8`,
  sonst erscheinen sie verstümmelt; Großbuchstaben über `upperCase()` aus `Text.h`, weil
  `toUpperCase` unter Windows Umlaute klein lässt.
- Alles, was die Maus braucht, lässt sich nur von Hand prüfen — synthetische Mausereignisse
  erreichen das Fenster nicht. Screenshots gehen; gepostete F-Tasten auch.

## Lizenz

Copyright © 2026 WiskundeKnobbel (Philippe Nix)

Sample Instrument Studio steht unter der **GNU Affero General Public License v3.0** (siehe
`LICENSE`). Das passt zu JUCE, das ohne kommerzielle Lizenz nur unter AGPLv3 verwendet
werden darf. Die mitgelieferten IBM-Plex-Schriften stehen unter der SIL Open Font License
(`assets/fonts/OFL.txt`).
