// The game's own settings, kept by the original in its CMOS (save/
// cmos.bin) as BCD, a nibble per CMOS cell: what its audit/adjustment pages
// change (romc8: AUDITG, BMPNUP...), offered as plain options in the
// service menu (GAME SETTINGS) instead. The game reads them as ever: ships
// and bonus level at a game's start, the coinage with each coin.
#pragma once

#include "game/board.hpp"

namespace vp {

struct GameAdjust
{
    int bonusShip{10000};   // a ship more every this many points (0: never); steps of 1000
    int ships{3};           // ships a game, 1..9
    int coinage{3};         // the original's coinage presets 1..8 (CSELCT); 0: as set on the board
    bool freePlay{false};
    int difficulty{5};      // GAME ADJUST 1: the difficulty to start with
    int ceiling{15};        // GAME ADJUST 2: the most it grows to
    int restoreWave{5};     // GAME ADJUST 4: every this many waves the planet comes back
    // bookkeeping (read only): the original's audit counters
    int audits[7]{};
    // actions on SAVE
    bool resetScores{false}, resetAudits{false}, factory{false};
};

extern const char* const auditNames[7];
extern const char* const coinageNames[9];

GameAdjust readAdjust(const df::Board& b);
// Writes the settings (and does the actions) into the CMOS. Returns true
// when the game must be switched off and on for them (high scores reset).
bool writeAdjust(df::Board& b, const GameAdjust& a);

} // namespace vp
