# Stand

Stand 18.10.2026.

## Was geht

- **Spiel aus dem Quelltext übersetzt, kein Emulator.** `tools/translate/asm2cpp.py` macht aus Defenders 6809-Quelltext (Red Label, github.com/mwenge/defender) C++: jede Instruktion ein Block mit Taktzyklen und Originalzeile als Kommentar. Keine ROM-Dateien zur Laufzeit; die Abbilder werden aus dem Quelltext gebaut und per MD5 gegen das Original geprüft (alle 12 stimmen).
- Die Platine drumherum (`src/game`): Speicherkarte, Bankumschaltung, zwei PIAs, Zeilentakt (260 Zeilen à 64 Takte, IRQ auf Zeile 0/64/128/192 wie bei MAME), CMOS als `save/cmos.bin`. Das Spiel läuft damit identisch: Attract-Modus, Hall of Fame, Münzen/Credits, Spiel, Bestenliste, Buchhaltung/Einstellungen.
- **Ton:** die Soundplatine (`vsndrm1.src`, 6800) genauso übersetzt (`snd2cpp.py`), DAC-Schreibzugriffe mit Zeitstempel zu 44,1 kHz.
- **60 Hz:** das Original läuft mit 60,1 Bildern/s; ein Bild pro Update, Board mit 60 Hz.
- **Vektoren** (`src/scene`):
  - Haken auf alle Bild-Schreibroutinen (`ON28` … `ON86`, `CWRIT`) erkennen jedes Sprite, jeden Buchstaben, jede Ziffer mit Position; geprüft gegen das intern weiter gezeichnete Pixelbild (Löschen/Überschreiben wird so automatisch erkannt).
  - Neu entworfene Vektor-Sprites (`designs.cpp`): Lander (3 Phasen), Mutant, Baiter (3), Bomber (4, drehender Würfel), Pod, Swarmer, Minen, Humanoid (4), Schiff links/rechts, Mini-Schiff, Smart Bomb, 250/500, Explosionsbilder; Farben über die Farbindizes des Originals (Farbzyklen und Blinken laufen mit).
  - Vektorschrift nach dem 5×7-Zeichensatz des Originals; DEFENDER-Logo als 3D-Vektorschrift.
  - Gebirge als Linienzug aus der Spaltentabelle `STBL`.
  - Effekte aus den Tabellen des Spiels: Explosionen und Erscheinen (das Vektor-Sprite zerfällt in Stücke, die genau wie die Blöcke des Originals fliegen; Größe und Mittelpunkt aus dem RAM-Block), Spielerexplosion (128 Leuchtspuren entlang der Teilchengeschwindigkeiten, mit dem Farbwechsel des Originals), Smart-Bomb-Blitz (Rahmen in der aufleuchtenden Hintergrundfarbe), Williams-Logo (der Stiftweg aus `LGOTAB` als geglätteter Linienzug, die Schreib-Animation läuft mit).
  - Rest (Laser, Schubflamme, Scanner mit Rahmen/Gebirge/Blips, Sterne, Schüsse): Linienverfolger über die übrigen Pixel (zusammenhängende Pixel → vereinfachte Linienzüge, Einzelpixel → Punkte).
  - Feste Zeichenreihenfolge, kein Sortieren pro Bild.
- **3D-Ansicht** (VIDEO → VIEW: CLASSIC / 3D / 3D DYNAMIC, `src/vp/view3d.cpp`): Schiff, Lander, Mutant, Baiter, Bomber, Pod, Swarmer, Minen und Humanoiden als 3D-Modelle mit verdeckten Linien (SDK `vp3d`), in den Farbindizes des Originals; das Schiff dreht sich beim Wenden räumlich um und legt sich beim Steigen/Sinken in die Kurve, Gegner rotieren; Gebirge als räumliches Band. Kamera leicht von oben/seitlich; bei 3D DYNAMIC dreht sie sich in die Flugrichtung des Schiffs, je schneller desto mehr (als sähe man weiter voraus), und neigt sich leicht mit der Flughöhe, gedämpft. Gilt, solange das Spielfeld zu sehen ist (Spiel und Attract-Demo, erkannt an Gebirge oder Schiff, mit kurzer Nachlaufzeit); Titel, Hall of Fame und Texttafeln bleiben klassisch. Linien werden wie im 2D-Modus als zusammenhängende Züge gezeichnet (keine Punkte an jeder Fuge). Scanner und Punkteleiste bleiben flach. Nur Darstellung: Positionen und Ablauf sind die des Originals. Kosten am PC ~0,2 ms pro Bild.
- **Joystick-Steuerung** (CONTROLS → STICK STEERING: OFF / TURN THAT WAY AND THRUST / TURN THAT WAY): Stick links/rechts dreht das Schiff in diese Richtung (drückt für dich den Reverse-Knopf des Originals) und gibt auf Wunsch Schub; der Reverse-Knopf geht weiter.
- **Freie Belegung** (`src/vp/bind.cpp`, nach Vectortron): jede Funktion – hoch, runter, Schub, Reverse, Feuer, Smart Bomb, Hyperspace, Stick links/rechts, Start 1/2, drei Münzschächte – auf bis zu zwei beliebige Eingaben: Taste, Joystick-Knopf, Achsrichtung oder Hat. Unbelegt gilt die advmame.rc des Automaten. Esc und F2 sind nie belegbar; das Menü bleibt immer mit Pfeiltasten/Return/Joystick 1 bedienbar.
- **F2-Service-Menü** (wie Red Alarm/Tempest): CONTROLS (STICK STEERING, je Funktion Eingabe 1, Eingabe 2, CLEAR, RESET ALL TO THE CABINET), VIDEO (VIEW, Helligkeiten Sprites/Gebirge/Text/Effekte, Bildgröße, Bildrate), GAME SETTINGS (die Einstellungen des Originals als normale Optionen: Bonus-Schiff alle …, Schiffe pro Spiel, Münzvorgabe, Free Play, Schwierigkeit, Schwierigkeits-Obergrenze, Planet zurück alle … Wellen; BOOKKEEPING zeigt die Zähler; RESET HIGH SCORES, RESET BOOKKEEPING, FACTORY SETTINGS werden bei SAVE ausgeführt; alles direkt im CMOS des Spiels, `src/vp/adjust.cpp`), Lautstärke. CANCEL stellt auch die Belegung wieder her.
- START-Lampen nach Credits. Marquee (`package/marquee.png`: das Original-DEFENDER-Marquee vom Besitzer, 2000×429). Im Menü heißt das Spiel „Defender“ (Hersteller Midway); Ordner und Kennung bleiben `vdefender`. PC- und Pi-Build, Paket `build/vdefender-0.1.0.vpk`.

## Bekannte Grenzen

- Noch nicht auf dem Automaten getestet (Helligkeiten, Linienlast, Punkte, echtes Panel).
- Das Erscheinen des DEFENDER-Logos (15 Teilstücke ohne eigenen Entwurf) fliegt als Punkte zusammen, wie die Pixelblöcke des Originals.
- Direkter Start ins Spiel (Wunsch des Besitzers): Einschalttest und „INITIAL TESTS INDICATE UNIT OK“ laufen beim Einschalten unsichtbar in Sekundenbruchteilen durch (bis `SINIT`), das erste Bild ist der Attract-Modus. Der Spielcode bleibt dabei unverändert.
- Der kleine „COPYRIGHT ©® 1980" und die Diagnosetexte nutzen eigene Schreibroutinen und erscheinen nachgefahren.
- Beim ersten Start ohne `save/cmos.bin` schreibt das Spiel seine Werkseinstellungen und wird einmal intern neu gestartet.
- Fensteraufnahmen unter Windows sind schwarz (OpenGL); Bilder über `defrun --dump`.

- In der 3D-Ansicht bleiben Explosionen, Punkte-Anzeigen (250/500), Schrift und Effekte flach auf der Spielfeldebene; ohne Modell sind auch die kleinen Symbole der Punkteleiste.

## Offene Aufgaben

1. Auf dem Automaten testen (auch 3D-Ansicht: Linienlast, Kamerawinkel, Lesbarkeit) und die Rückmeldung einarbeiten.
