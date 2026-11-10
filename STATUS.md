# Status

As of 2026-11-10, version 0.2.0.

## What works

- **The game translated from its source code, no emulator.** `tools/translate/asm2cpp.py` turns Defender's 6809 source (Red Label, github.com/mwenge/defender) into C++: every instruction a block with its clock cycles and the original line as a comment. No ROM files at run time; the images are built from the source and checked by MD5 against the original (all 12 match).
- The board around it (`src/game`): memory map, bank switching, two PIAs, line timing (260 lines of 64 cycles, IRQ on lines 0/64/128/192 as in MAME), CMOS as `save/cmos.bin`. The game runs identically with it: attract mode, Hall of Fame, coins/credits, game, high score entry, bookkeeping/settings.
- **Sound:** the sound board (`vsndrm1.src`, 6800) translated the same way (`snd2cpp.py`), DAC writes time-stamped to 44.1 kHz. Played as an SDK stream marked as sound effects: on the cabinet at the VectorPie menu's SOUND VOLUME (not its MUSIC VOLUME), times the game's own VOLUME (F2).
- **VectorPie SDK 0.9.0** (upstream b00d537) in `vendor/VectorPie`, with two additions of this project, each marked "Vector Defender addition":
  - `src/app/vp_app.cpp`: frames paced in fractions of a millisecond (the whole-number 16 ms made 62.5 frames a second → judder), and exactly one update per frame when update and frame rate are equal.
  - `src/audio/vp_audio.cpp`, `include/vectorpie/vp_audio.h`: `vp_stream_set_effects`, a game's own stream at the cabinet's SOUND VOLUME (upstream streams always follow MUSIC VOLUME).
- **60 Hz:** the original runs at 60.1 frames/s; one frame per update, the board at 60 Hz.
- **Smooth movement:** sprites are placed at the game's exact 16-bit positions (ship `PLAX16/PLAY16`, objects `OX16/OY16` against `BGL`, read while drawing; mountains with the fraction of `BGL` at `BGOUT`) instead of rounded to whole pixels as in the original. Measured (`defrun --jitter`): step-size variation 0.06 instead of 0.40 pixels per frame.
- **Vectors** (`src/scene`):
  - Hooks on all picture writers (`ON28` … `ON86`, `CWRIT`) recognise every sprite, letter and digit with its position, checked against the pixel picture the original code still draws internally (erasing/overwriting is detected that way). A newly drawn picture replaces every older one it covers (the original writes opaquely), so a moving object is never drawn twice (before, a "veil" trailed ship and enemies). `defrun` reports leftover doubles with `--dump`.
  - Newly designed vector sprites (`designs.cpp`): Lander (3 phases), Mutant, Baiter (3), Bomber (4, spinning cube), Pod, Swarmer, mines, Humanoid (4), ship left/right, mini ship, Smart Bomb, 250/500, explosion pictures; colours through the original's colour indices (colour cycles and blinking run along).
  - Vector font after the original's 5×7 character set. Logos redrawn (`src/scene/logos.cpp`, preview `defrun --logos`): DEFENDER as on the marquee, upright block letters, yellow face, red depth in perspective downwards (vanishing point above the middle of the word; only visible side edges, covered parts cut away); Williams as a smooth script (a spline through key points per letter, with ®), fitted where the original's stands.
  - Mountains as a polyline from the column table `STBL`.
  - Effects from the game's tables: explosions and materialising (the vector sprite breaks into pieces that fly exactly like the original's blocks; size and centre from the RAM block), the player explosion (128 trails along the particle velocities, with the original's colour changes), the Smart Bomb flash (a frame in the flashing background colour); the Williams logo's writing animation: the new script is drawn as far as the original's pen path (`LGOTAB`) has written.
  - The rest (laser, thrust flame, scanner with frame/mountains/blips, stars, shots): a line tracer over the remaining pixels (connected pixels → simplified polylines, single pixels → dots).
  - Fixed drawing order, no sorting per frame.
- **3D view** (VIDEO → VIEW: CLASSIC / 3D / 3D DYNAMIC / 3D ENEMIES; 3D ENEMIES = straight like classic, flat mountains, ship in its 2D design, only enemies and humanoids as 3D models; `src/vp/view3d.cpp`): ship, Lander, Mutant, Baiter, Bomber, Pod, Swarmer, mines and humanoids as 3D models with hidden lines (SDK `vp3d`), in the original's colour indices; the ship turns round in space when reversing and banks when climbing/diving, enemies rotate; the mountains as a band in space. Camera slightly from above/the side; with 3D DYNAMIC it turns into the ship's direction of flight, the faster the more (as if looking further ahead), and tilts slightly with the altitude, damped. Applies while the playfield is visible (game and attract demo, detected by mountains or ship, with a short hold time); title, Hall of Fame and text pages stay classic. Lines are drawn as connected strokes as in 2D (no dots at every joint). Scanner and score bar stay flat. Presentation only: positions and sequence are the original's. Cost on the PC ~0.2 ms per frame.
- **Joystick steering** (CONTROLS → STICK STEERING: OFF / TURN THAT WAY AND THRUST / TURN THAT WAY): stick left/right turns the ship that way (presses the original's Reverse button for you) and thrusts if wanted; the Reverse button still works. The original's switch scan (`SSCAN`) takes only one newly closed switch per frame (the lowest bit): the reverse press therefore gets a frame of its own, thrust follows one frame later, so the ship turns at once.
- **Free bindings** with the shared `vpbind` library of the cabinet games (`src/vp/vpbind.c/h`, a copy of the master file from the Tempest 2000 port, do not change): every function (up, down, thrust, reverse, fire, Smart Bomb, hyperspace, stick left/right) on any number of keys, joystick buttons, hats, axes, mouse buttons (up to 4 per function). CONTROLS page as in Tempest 2000: one column; START 1/Enter on the row, then the input (the first replaces, more are added); left/right NONE or default; Del default. Start and coins are the cabinet's. Old entries `bind_* = key:N` are taken over when loading.
- **F2 service menu** (as Red Alarm/Tempest): CONTROLS (STICK STEERING, the functions), VIDEO (VIEW, brightness of sprites/mountains/text/effects, picture size, frame rate), GAME SETTINGS (the original's settings as plain options: bonus ship every …, ships per game, coinage, free play, difficulty, difficulty ceiling, planet restored every … waves; BOOKKEEPING shows the counters; RESET HIGH SCORES, RESET BOOKKEEPING, FACTORY SETTINGS are carried out on SAVE; everything directly in the game's CMOS, `src/vp/adjust.cpp`), VOLUME (the game's own, on top of the cabinet's SOUND VOLUME; the hint says so). CANCEL also restores the bindings.
- START lamps after credits. Marquee (`package/marquee.png`: the original DEFENDER marquee from the owner, 2000×429). In the menu the game is called "Defender" (manufacturer Midway); folder and id stay `vdefender`. PC and Pi build, package `build/vdefender-0.2.0.vpk`.

## Known limitations

- The cabinet's SOUND VOLUME is read once when the game starts (as the SDK does): a change in the VectorPie menu applies from the next start.
- The DEFENDER logo's materialising (15 pieces without a design of their own) flies together as dots, like the original's pixel blocks.
- Direct start into the game (the owner's wish): the power-on test and "INITIAL TESTS INDICATE UNIT OK" run invisibly in a fraction of a second at power-on (until `SINIT`), the first picture is the attract mode. The game code stays unchanged.
- The small "COPYRIGHT ©® 1980" and the diagnostic texts use their own writing routines and appear traced.
- At the first start without `save/cmos.bin` the game writes its factory settings and is restarted once internally.
- Window captures under Windows are black (OpenGL); pictures via `defrun --dump`.
- In the 3D view, explosions, score displays (250/500), lettering and effects stay flat on the playfield plane; so do the small symbols of the score bar, which have no model.

## Open tasks

1. Test 0.2.0 on the cabinet (SDK 0.9, SOUND VOLUME; 3D view: line load, camera angle, readability) and work in the feedback.
