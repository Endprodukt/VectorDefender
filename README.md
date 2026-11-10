# Vector Defender

Williams' **Defender** (1980, Eugene Jarvis and Sam Dicker, Red Label) for the VectorPie cabinet. The game is the original, translated automatically from its 6809 source code to C++: no emulator, no ROM files. Only the picture is new: every sprite, the lettering, the logos, the mountains and all effects are drawn as colour vectors, at 60 frames a second like the original.

- Gameplay, enemies, scoring, randomness, timing, attract mode, high scores, coinage and settings: exactly as the original (it is the original's program).
- Boots straight into the attract mode (the original's power-on test runs unseen).
- Sound: the sound board, also translated from its source code. On the cabinet it plays at the VectorPie menu's SOUND VOLUME, with the game's own VOLUME on top.
- F2: service menu with completely free bindings (every function on any number of keys, joystick buttons, hats, axes or mouse buttons), brightness, picture size, and the original's game settings (ships, bonus, coinage, free play, difficulty) as plain options.

Building and testing: [BUILD.md](BUILD.md). State and open items: [STATUS.md](STATUS.md). For continuing the work: [RESUME.md](RESUME.md).

## Controls (defaults = the cabinet's advmame.rc, freely bindable in the F2 menu)

| Function | Cabinet | PC |
|---|---|---|
| up / down | Stick P1 | arrow keys |
| Fire | Button 1 | Left Ctrl |
| Thrust | Button 2 | Alt |
| Smart Bomb | Button 3 | Space |
| Hyperspace | Button 4 | Shift |
| Reverse | Button 5 | Z |
| Coin / Start | coin1, coin2 / start1, start2 | 5, 6 / 1, 2 |

With STICK STEERING (F2 → CONTROLS) stick left/right flies the ship that way (turn and thrust, or turn only); the Reverse button still works.

## View

F2 → VIDEO → VIEW: CLASSIC (flat like the original), 3D (ship and enemies as 3D models, a slightly tilted view, mountains with depth), 3D DYNAMIC (the camera turns the way you fly, the faster the more, as if looking further ahead) or 3D ENEMIES (the classic picture, only enemies and humanoids as 3D models). 3D applies to the playfield, in the game and in the attract demo; title, Hall of Fame and text pages stay classic. The game itself is the same in every view.

## Installing

Download `vdefender-<version>.vpk` from the releases and copy it to a USB drive (its `games` folder) or over the network to `/persistent/games/packages/` on the cabinet, then Settings → GAMES → INSTALL GAMES. The game shows under Midway.

Source code of the original: github.com/mwenge/defender (from historicalsource/defender).
