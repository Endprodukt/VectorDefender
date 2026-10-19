# Weiterarbeiten

Antworten an den Besitzer auf Deutsch (du). Projektdokumente Deutsch, Code-Kommentare Englisch. Auf dem Automaten nur Dateien kopieren; nichts starten oder beenden ohne ausdrückliche Bitte. **Kein Emulator** – der Besitzer will das Spiel aus dem Quelltext übersetzt (auch die Soundplatine).

## Aufbau

- `reference/defender/`: Klon von mwenge/defender mit gebautem asm6809/vasm, Listings in `bin/`, ROM-Abbilder in `redlabel/` (aus dem Quelltext, MD5 geprüft). Nur lesen.
- `tools/translate/asm2cpp.py`: 6809-Listings + Abbilder → `src/gen/defender_code.cpp` (je Region eine Funktion: fest D000-FFFF, Bänke 1/2/3/7 bei C000), `defender_data.inc`, `defender_syms.hpp` (Adressen aus `symbols.txt`), Haken aus `hooks.txt` (Aufruf von `Machine::hook` vor der Instruktion).
- `tools/translate/snd2cpp.py`: dasselbe für die 6800-Soundplatine → `src/gen/sound_code.cpp`.
- `src/game/`: `cpu.hpp` (Register, Flags, Speicherkarte `Machine`), `machine.cpp` (I/O, PIAs, IRQ, Bänke), `board.cpp` (Bild = 260 Zeilen, Soundplatine daneben, CMOS), `sound.*` (Soundplatine, DAC → Samples).
- `src/scene/`: `scene.cpp` (Haken auswerten, gegen das Pixelbild prüfen, Gebirge aus `STBL`, Linienverfolger für den Rest), `designs.cpp` (alle Vektor-Entwürfe, Schrift, Logo).
- `src/vp/`: VectorPie-Seite: `app.cpp` (Ablauf, Ton, 3D solange Gebirge oder Schiff zu sehen sind, mit Nachlauf `FIELD_HOLD`, direkter Start: Einschalttest unsichtbar bis `SINIT`), `adjust.cpp` (Spieleinstellungen im CMOS lesen/schreiben: BCD, ein Nibble je Zelle, Adressen aus `symbols.txt`), `bind.cpp` (freie Belegung, nach Vectortron), `controls.cpp` (Aktionen → Schalter des Spiels), `settings.cpp` (`save/settings.ini`; Belegungen als `bind_* = key:N, joyJ:buttonB`), `service.cpp` (F2), `render.cpp` (Pixel → Board, sichtbar x 6..297, y 7..246), `view3d.cpp` (3D-Ansicht: Modelle je Bild-Deskriptor, Kamera, Wende-Animation; die Szene liefert dafür `Scene::objects()` und kennzeichnet die Linien jedes Sprites mit `Line::obj`).
- `tools/defrun/dvg_stub.cpp`: Ersatz für die Board-Ausgabe, damit `defrun --view 1|2` die 3D-Ansicht als Bild `w<bild>.ppm` schreibt.
- `tools/defrun/`: Testprogramm ohne Fenster (Bilder, Vektorbilder, WAV), `tools/pictures.py` (Sprite-Bogen), `tools/montage.py`, `tools/ppm2png.py`, `tools/make_vpk.sh`, `tools/snap.ps1` (liefert bei OpenGL schwarz).

## Wichtige Stellen im Original

| Label | Was |
|---|---|
| `OPROC` (E39F) | zeichnet alle Objekte, je Bildhälfte im IRQ |
| `ON28`…`ON86` | Sprite-Schreiber: Y = Deskriptor, D = Bildadresse (A Spalte zu 2 Pixeln, B Zeile), Carry = ungerade Phase |
| `CWRIT` (F522) | allgemeiner Schreiber (Text, Ziffern, Symbole, Logo), Daten aus Bank 2 |
| `CHRTBL` (Bank 2, C5E3) | 45 Zeichen à 4 Byte (Breite, Höhe, Datenzeiger) |
| `STBL` (BE20) | je Bildspalte die Adresse des Gebirgsstücks (0x98 Einträge) |
| `DEFDSC` (B300) | Deskriptor des ganzen DEFENDER-Logos (RAM) |
| `CMOSCK` (C47F/C480) | Prüfbyte 5A = CMOS gültig |
| `NPLAD` | Blickrichtung des Schiffs (positiv = rechts) |
| `RAMALS`..`RAMEND` | Explosions-/Erscheinen-Blöcke à 0x40: Größe 8.8 (Bit 15 = Erscheinen), Bild, -, Mitte, Ecke |
| `BLK71_TABLE` (B300) | Spielerexplosion: 128 Teilchen à 10 Byte (Adresse, Position, Geschwindigkeit) |
| `LGOTAB` (Bank 1) | Stiftweg des Williams-Logos |
| `SINIT` | Spielstart nach dem Einschalttest (Haken setzt `Machine::started`) |

## Konventionen

- `src/gen/` nie von Hand ändern: Übersetzer oder `hooks.txt`/`symbols.txt` ändern und neu erzeugen.
- Linien nie pro Bild umsortieren (wackelt auf der Röhre).
- Eingaben nur über advmame.rc-Namen; Service-Menü nur F2; START 1 + START 2 zusammen ist am Automaten Exit.
- Python-/C-Text nicht als Heredoc mit Backslashes durch die Bash schicken.

## Nächste Schritte

Siehe STATUS.md, „Offene Aufgaben": Paket auf den Automaten kopieren (nach Rückfrage) und Rückmeldung abwarten.
