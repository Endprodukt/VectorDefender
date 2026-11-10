# Continuing the work

The owner writes in German; answer them in German. Everything in the repository is English (it is public on GitHub): docs, comments, on-screen text. On the cabinet only copy files; never start or stop anything without being asked. **No emulator**: the owner wants the game translated from its source code (the sound board too).

## Layout

- `reference/defender/`: clone of mwenge/defender with asm6809/vasm built, listings in `bin/`, ROM images in `redlabel/` (from the source, MD5 checked). Read only; not in the repository.
- `tools/translate/asm2cpp.py`: 6809 listings + images → `src/gen/defender_code.cpp` (one function per region: fixed D000-FFFF, banks 1/2/3/7 at C000), `defender_data.inc`, `defender_syms.hpp` (addresses from `symbols.txt`), hooks from `hooks.txt` (a call of `Machine::hook` before the instruction).
- `tools/translate/snd2cpp.py`: the same for the 6800 sound board → `src/gen/sound_code.cpp`.
- `src/game/`: `cpu.hpp` (registers, flags, memory map `Machine`), `machine.cpp` (I/O, PIAs, IRQ, banks), `board.cpp` (a frame = 260 lines, the sound board alongside, CMOS), `sound.*` (sound board, DAC → samples).
- `src/scene/`: `scene.cpp` (evaluates the hooks, checks against the pixel picture, mountains from `STBL`, line tracer for the rest), `designs.cpp` (all vector designs, font), `logos.cpp` (DEFENDER and Williams logos).
- `src/vp/`: the VectorPie side: `app.cpp` (sequence, sound as an SDK stream marked as effects, 3D while mountains or ship are visible with the hold time `FIELD_HOLD`, direct start: power-on test unseen until `SINIT`), `adjust.cpp` (game settings in the CMOS: BCD, one nibble per cell, addresses from `symbols.txt`), `vpbind.c/h` (shared binding library, master in the Tempest 2000 port, copied unchanged), `controls.cpp` (functions → the game's switches; reverse for STICK STEERING in a frame of its own because of `SSCAN`), `settings.cpp` (`save/settings.ini`; bindings as `fire = key_Left_Ctrl+joy1_button1`, old `bind_*` are taken over), `service.cpp` (F2), `render.cpp` (pixels → board, visible x 6..297, y 7..246), `view3d.cpp` (3D view: models per picture descriptor, camera, reverse animation; the scene supplies `Scene::objects()` and tags each sprite's lines with `Line::obj`).
- `vendor/VectorPie/`: the VectorPie SDK 0.9.0 (upstream github.com/Goldfinch2/VectorPie, b00d537) with two additions marked "Vector Defender addition" (frame pacing in `vp_app.cpp`, `vp_stream_set_effects` in `vp_audio`). On an SDK update, copy the new SDK over and re-apply both.
- `tools/defrun/`: test program without a window (pictures, vector pictures, WAV); `dvg_stub.cpp` stands in for the board output so that `defrun --view 1|2` writes the 3D view as `w<frame>.ppm`. Also `tools/pictures.py` (sprite sheet), `tools/montage.py`, `tools/ppm2png.py`, `tools/make_vpk.sh`, `tools/snap.ps1` (black with OpenGL).

## Important places in the original

| Label | What |
|---|---|
| `OPROC` (E39F) | draws all objects, one half of the picture per IRQ |
| `ON28`…`ON86` | sprite writers: Y = descriptor, D = picture address (A column of 2 pixels, B line), Carry = odd phase |
| `CWRIT` (F522) | general writer (text, digits, symbols, logo), data from bank 2 |
| `CHRTBL` (bank 2, C5E3) | 45 characters of 4 bytes (width, height, data pointer) |
| `STBL` (BE20) | per picture column the address of the mountain piece (0x98 entries) |
| `DEFDSC` (B300) | descriptor of the whole DEFENDER logo (RAM) |
| `CMOSCK` (C47F/C480) | check byte 5A = CMOS valid |
| `NPLAD` | the ship's facing (positive = right) |
| `RAMALS`..`RAMEND` | explosion/materialise blocks of 0x40: size 8.8 (bit 15 = materialising), picture, -, centre, corner |
| `BLK71_TABLE` (B300) | player explosion: 128 particles of 10 bytes (address, position, velocity) |
| `LGOTAB` (bank 1) | the Williams logo's pen path |
| `SINIT` | game start after the power-on test (the hook sets `Machine::started`) |

## Conventions

- Never edit `src/gen/` by hand: change the translator or `hooks.txt`/`symbols.txt` and regenerate.
- Never reorder lines per frame (it wobbles on the tube).
- Inputs only by advmame.rc names; service menu only F2; START 1 + START 2 together is the cabinet's exit.
- Do not send Python/C text with backslashes through Bash as a heredoc.
- Deploying to the cabinet: the package's root is `vdefender/`; unpack with `tar xzf <vpk> -C /persistent/games`, never from inside `/persistent/games/vdefender`.

## Next steps

See STATUS.md, "Open tasks".
