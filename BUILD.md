# Bauen

## Übersetzen (nur nach Änderungen am Übersetzer)

Der Spielcode in `src/gen/` ist erzeugt und eingecheckt; zum normalen Bauen braucht es keinen Assembler. Neu erzeugen:

```bash
python tools/translate/asm2cpp.py     # Hauptprogramm: src/gen/defender_code.cpp, defender_data.inc, defender_syms.hpp
python tools/translate/snd2cpp.py     # Soundplatine:  src/gen/sound_code.cpp, sound_data.inc
```

Die Eingaben dafür liegen in `reference/defender` (Klon von github.com/mwenge/defender): die Listings in `bin/` und die ROM-Abbilder in `redlabel/`. Erzeugt wurden sie so (MSYS2, einmalig `pacman -S bison flex autoconf automake make`):

```bash
cd reference/defender
git submodule update --init
MSYSTEM=MINGW64 /c/msys64/usr/bin/bash -lc 'cd asm6809 && ./autogen.sh && ./configure && make'
MSYSTEM=MINGW64 /c/msys64/usr/bin/bash -lc 'cd vasm-mirror && make CPU=6800 SYNTAX=oldstyle'
mkdir -p bin
./asm6809/src/asm6809 -B src/phr6.src src/defa7.src src/defb6.src src/amode1.src -l bin/defa7-defb6-amode1.lst -o bin/defa7-defb6-amode1.o
./asm6809/src/asm6809 -B src/phr6.src src/samexap7.src -l bin/samexap7.lst -o bin/samexap7.o
./asm6809/src/asm6809 -B --6309 src/blk71.src -l bin/blk71.lst -o bin/blk71.o
./asm6809/src/asm6809 -B src/mess0.src src/romf8.src src/romc0.src src/romc8.src -l bin/roms.lst -o bin/roms.o
./vasm-mirror/vasm6800_oldstyle -Fbin -ast -unsshift src/vsndrm1.src -L bin/vsndrm1.lst -o bin/vsndrm1.o
```

danach die `redlabel`-Schritte des Makefiles (mit `python ChainFilesToRom.py` / `python PatchROM.py`); jedes Abbild wird dort per MD5 gegen das Original geprüft.

Haken und Adressen, die die Vektorseite braucht, stehen als Label-Namen in `tools/translate/hooks.txt` und `tools/translate/symbols.txt`.

## PC (Windows, nativ)

Voraussetzung: MSYS2 mit MinGW-w64-GCC und SDL2 unter `C:\msys64\mingw64`, CMake, Ninja.

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build/pc
build/pc/vdefender.exe --windowed
```

Einstellungen in `build/pc/save/settings.ini`, das CMOS des Spiels (Münzeinstellungen, Bestenliste) in `build/pc/save/cmos.bin`.

Tasten am PC (AdvanceMAME-Vorgaben): Pfeil hoch/runter = Schiff, Linke Strg = Feuer, Alt = Schub, Leertaste = Smart Bomb, Shift = Hyperspace, Z = Reverse, 5 = Münze, 1 / 2 = Start, F2 = Service-Menü, Esc = Ende.

`VD_TRACE=1` gibt einmal pro Sekunde aus, was die Szene enthält, und jede Änderung der Schalter, die beim Spiel ankommen (zum Prüfen der Belegung). `VP_STATS=1` die Kosten eines Bildes (SDK).

## Raspberry Pi (arm64)

```bash
export PATH="/c/Users/ocare/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin:/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build/pi -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/pi
```

Sysroot: `F:/Tempest 2000 Port/build/pi-sysroot` (`VD_PI_SYSROOT` in `cmake/toolchain-pi-clang.cmake`).

## Paket für den Automaten

```bash
sh tools/make_vpk.sh
```

schreibt `build/vdefender-<version>.vpk`. Es enthält keine ROM-Dateien (das Spiel ist der übersetzte Code). Installieren: in `/persistent/games/packages/` legen oder per USB-Stick (`games/`), dann im Menü Settings, GAMES, INSTALL GAMES; oder direkt nach `/persistent/games/vdefender/` entpacken. `save/` bleibt bei Updates erhalten.

## Testen ohne Fenster

```bash
build/pc/defrun.exe 2800 --cmos build/cmos.bin --press 1400:2:0x10:8 --press 1500:0:0x20:6 --dump 1650,1900 --out build/shots --wav build/test.wav
python tools/montage.py build/shots/m.png 2 build/shots/v1650.ppm build/shots/v1900.ppm
```

- `--press bild:port:maske:dauer` hält Schalter (Port 0..2 = IN0..IN2, Bits in `src/game/board.hpp`; 2:0x10 = Münze, 0:0x20 = Start 1, 0:0x01 = Feuer, 0:0x02 = Schub).
- `--dump` schreibt je Bild `f<bild>.ppm` (das Pixelbild, das der Originalcode intern weiter zeichnet) und `v<bild>.ppm` (die Vektorlinien, gerastert).
- `--wav` den Ton. `--cmos` das CMOS (ohne: leeres CMOS, das Spiel landet nach den Werkseinstellungen in der Buchhaltung).
- `--trace` meldet Bilder mit Explosionen, Spielerexplosion und Blitz.
- `--view 1` (3D) oder `--view 2` (3D DYNAMIC) schreibt zu jedem `--dump` auch `w<bild>.ppm`, die 3D-Ansicht so, wie sie an das Board ginge.
- `defrun --marquee build/marquee.ppm` rendert ein Vektor-Marquee. Nicht mehr verwenden: `package/marquee.png` ist das Marquee des Besitzers.
- `python tools/pictures.py build/pictures.png 8`: Bogen aller Original-Sprites.

Fensteraufnahmen (`tools/snap.ps1`) liefern bei diesem OpenGL-Fenster nur Schwarz; zum Ansehen `defrun --dump` nehmen.
