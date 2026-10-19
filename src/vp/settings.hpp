// The cabinet's settings: save/settings.ini, changed in the service menu
// (F2). The game's own settings (coinage, ships, bonus, difficulty, free
// play) stay the original's: its adjustment pages, reached from the service
// menu (GAME ADJUSTMENTS), keep them in its CMOS (save/cmos.bin).
#pragma once

#include <string>

namespace vp {

struct Settings
{
    // Game
    int volume{80};               // 0..100
    int stickSteer{0};            // stick left / right: 0 off, 1 turn that way and thrust, 2 only turn
    int view{0};                  // 0 classic (flat), 1 3D, 2 3D with a moving camera
    // The controls' bindings are kept by bind.cpp (in the same file).

    // Video
    int brightSprites{100};       // percent
    int brightTerrain{80};
    int brightText{90};
    int brightEffects{100};       // lasers, explosions, stars, scanner
    int screenSize{96};           // percent of the board the picture fills
    int frameHz{60};              // 60, 50 or 40 on the board
};

void loadSettings(Settings& s);
void saveSettings(const Settings& s);

} // namespace vp
