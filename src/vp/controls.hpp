// The controls: the game's switches from the functions' laid out controls
// (vpbind.h, Settings::controls); START and the coins are the cabinet's.
#pragma once

#include "game/board.hpp"
#include "settings.hpp"

typedef struct vp_input vp_input;

namespace vp {

class Controls
{
public:
    // The game's switches for this picture (vpb_update has run).
    df::Inputs read(vp_input* in, const Settings& s, const df::Board& b);
    void reset() { *this = Controls{}; }

private:
    int coin_[3]{};               // pictures a coin switch stays closed
    int revHeld_{0}, revGap_{0};  // STICK STEERING: the reverse press it makes
    uint8_t prevIn0_{0};          // the last picture's IN0, to see what closes now
};

} // namespace vp
