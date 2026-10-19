# Vector Defender

Williams' **Defender** (1980, Eugene Jarvis und Sam Dicker, Red Label) für den VectorPie-Automaten: das Spiel ist das Original, aus seinem 6809-Quelltext automatisch nach C++ übersetzt – kein Emulator, keine ROM-Dateien. Nur das Bild ist neu: Jedes Sprite, die Schrift, das Logo, das Gebirge und alle Effekte werden als Farbvektoren gezeichnet, mit 60 Bildern pro Sekunde wie das Original.

- Spielablauf, Gegner, Punkte, Zufall, Timing, Attract-Modus, Bestenliste, Münzen und Einstellungen: exakt wie das Original (es ist dessen Programm).
- Startet direkt in den Attract-Modus (der Einschalttest des Originals läuft unsichtbar durch).
- Ton: die Soundplatine, ebenfalls aus ihrem Quelltext übersetzt.
- F2: Service-Menü mit völlig freier Belegung (jede Funktion auf zwei beliebige Tasten, Joystick-Knöpfe, Achsen oder Hats), Helligkeiten, Bildgröße und die Spieleinstellungen des Originals (Schiffe, Bonus, Münzen, Free Play, Schwierigkeit) als normale Optionen.

Bauen und Testen: [BUILD.md](BUILD.md). Stand und offene Punkte: [STATUS.md](STATUS.md). Zum Weiterarbeiten: [RESUME.md](RESUME.md).

## Steuerung (Vorgabe = advmame.rc des Automaten, im F2-Menü frei belegbar)

| Funktion | Automat | PC |
|---|---|---|
| hoch / runter | Stick P1 | Pfeiltasten |
| Feuer | Knopf 1 | Linke Strg |
| Schub | Knopf 2 | Alt |
| Smart Bomb | Knopf 3 | Leertaste |
| Hyperspace | Knopf 4 | Shift |
| Reverse | Knopf 5 | Z |
| Münze / Start | coin1, coin2 / start1, start2 | 5, 6 / 1, 2 |

Mit STICK STEERING (F2 → CONTROLS) fliegt das Schiff mit Stick links/rechts in diese Richtung (Drehen + Schub, oder nur Drehen); der Reverse-Knopf geht weiterhin.

## Ansicht

F2 → VIDEO → VIEW: CLASSIC (flach wie das Original), 3D (Schiff und Gegner als 3D-Modelle, leicht schräger Blick, Gebirge mit Tiefe) oder 3D DYNAMIC (die Kamera dreht sich in die Flugrichtung, je schneller desto mehr, als sähe man weiter voraus). 3D gilt für das Spielfeld – im Spiel und in der Attract-Demo; Titel, Hall of Fame und Texttafeln bleiben klassisch. Das Spiel selbst ist in allen Ansichten dasselbe.

Quelltext des Originals: github.com/mwenge/defender (aus historicalsource/defender).
