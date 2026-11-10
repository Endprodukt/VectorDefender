# Building

## Translating (only after changes to the translator)

The game code in `src/gen/` is generated and checked in; a normal build needs no assembler. To regenerate:

```bash
python tools/translate/asm2cpp.py     # main program: src/gen/defender_code.cpp, defender_data.inc, defender_syms.hpp
python tools/translate/snd2cpp.py     # sound board:  src/gen/sound_code.cpp, sound_data.inc
```

Their inputs are in `reference/defender` (a clone of github.com/mwenge/defender): the listings in `bin/` and the ROM images in `redlabel/`. They were made like this (MSYS2, once `pacman -S bison flex autoconf automake make`):

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

then the `redlabel` steps of the Makefile (with `python ChainFilesToRom.py` / `python PatchROM.py`); each image is checked there by MD5 against the original.

The hooks and addresses the vector side needs are given as label names in `tools/translate/hooks.txt` and `tools/translate/symbols.txt`.

## PC (Windows, native)

Requires MSYS2 with MinGW-w64 GCC and SDL2 in `C:\msys64\mingw64`, CMake, Ninja.

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build/pc
build/pc/vdefender.exe --windowed
```

Settings in `build/pc/save/settings.ini`, the game's CMOS (coinage, high scores) in `build/pc/save/cmos.bin`.

Keys on the PC (AdvanceMAME defaults): arrow up/down = ship, Left Ctrl = fire, Alt = thrust, Space = Smart Bomb, Shift = hyperspace, Z = reverse, 5 = coin, 1 / 2 = start, F2 = service menu, Esc = quit.

`VD_TRACE=1` prints once a second what the scene holds, and every change of the switches that reach the game (to check the bindings). `VP_STATS=1` the cost of a frame (SDK).

## Raspberry Pi (arm64)

```bash
export PATH="/c/Users/ocare/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin:/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build/pi -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/pi
```

Sysroot: `F:/Tempest 2000 Port/build/pi-sysroot` (`VD_PI_SYSROOT` in `cmake/toolchain-pi-clang.cmake`).

## The SDK

`vendor/VectorPie` is the VectorPie SDK 0.9.0 (github.com/Goldfinch2/VectorPie, b00d537) with this project's two additions, marked "Vector Defender addition": frame pacing in fractions of a millisecond (`sdk/src/app/vp_app.cpp`) and `vp_stream_set_effects` (`sdk/include/vectorpie/vp_audio.h`, `sdk/src/audio/vp_audio.cpp`), so the sound board's stream follows the cabinet's SOUND VOLUME. On an SDK update, copy the new SDK over (without `.git` and build folders) and re-apply both.

## Package for the cabinet

```bash
sh tools/make_vpk.sh
```

writes `build/vdefender-<version>.vpk`. It contains no ROM files (the game is the translated code). Installing: put it in `/persistent/games/packages/` or on a USB drive (`games/`), then in the menu Settings, GAMES, INSTALL GAMES; or unpack it directly: `tar xzf vdefender-<version>.vpk -C /persistent/games` (the package's root is `vdefender/`; do **not** unpack it inside `/persistent/games/vdefender`, that replaces the program file with a folder and the game does not start). `save/` is kept on updates.

## Testing without a window

```bash
build/pc/defrun.exe 2800 --cmos build/cmos.bin --press 1400:2:0x10:8 --press 1500:0:0x20:6 --dump 1650,1900 --out build/shots --wav build/test.wav
python tools/montage.py build/shots/m.png 2 build/shots/v1650.ppm build/shots/v1900.ppm
```

- `--press frame:port:mask:duration` holds switches (port 0..2 = IN0..IN2, bits in `src/game/board.hpp`; 2:0x10 = coin, 0:0x20 = start 1, 0:0x01 = fire, 0:0x02 = thrust).
- `--dump` writes per frame `f<frame>.ppm` (the pixel picture the original code still draws internally) and `v<frame>.ppm` (the vector lines, rasterised).
- `--wav` the sound. `--cmos` the CMOS (without: an empty CMOS, the game ends up in bookkeeping after the factory settings).
- `--trace` reports frames with explosions, the player explosion and the flash.
- `--jitter <frame>` measures from that frame how unevenly objects move (exact against whole-number positions); `--watch <address>` reports every change of a RAM byte.
- `--view 1` (3D) or `--view 2` (3D DYNAMIC) also writes `w<frame>.ppm` for every `--dump`, the 3D view as it would go to the board.
- `defrun --logos build/logos.ppm` draws the Williams and DEFENDER logos large (for designing, `src/scene/logos.cpp`).
- `python tools/pictures.py build/pictures.png 8`: a sheet of all the original's sprites.

Window captures (`tools/snap.ps1`) give only black for this OpenGL window; use `defrun --dump` to look at pictures.
