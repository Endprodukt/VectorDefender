// The cabinet's settings: save/settings.ini, changed in the service menu
// (F2). The game's own settings (coinage, ships, bonus, difficulty, free
// play) stay in its CMOS (save/cmos.bin), changed on the menu's GAME
// SETTINGS page (adjust.hpp).
#pragma once

#include "vpbind.h"

#include <string>

namespace vp {

// Defender's functions a control can be laid on (vpbind.h: any keys,
// joystick buttons, hats, axes, mouse buttons, several each). START and the
// coins are the cabinet's own.
enum Function
{
    FN_UP, FN_DOWN, FN_THRUST, FN_REVERSE, FN_FIRE, FN_SMART, FN_HYPER,
    FN_LEFT, FN_RIGHT,     // only with STICK STEERING (fly that way)
    FN_COUNT
};

extern const char* const functionNames[FN_COUNT];      // "MOVE UP" ...
extern const char* const functionStandard[FN_COUNT];   // the panel's: AdvanceMAME's defaults

struct Settings
{
    // Game
    int volume{80};               // 0..100
    int stickSteer{0};            // stick left / right: 0 off, 1 turn that way and thrust, 2 only turn
    int view{0};                  // 0 classic (flat), 1 3D, 2 3D with a moving camera, 3 classic with 3D enemies and humanoids

    // Controls: each function's, as vpbind text ("key_Left_Ctrl+joy1_button1").
    char controls[FN_COUNT][VPB_LENGTH]{};

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
