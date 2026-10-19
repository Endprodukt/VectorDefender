// The controls: the game's switches from the bound actions (bind.hpp).
#pragma once

#include "game/board.hpp"
#include "settings.hpp"

namespace vp {

class Controls
{
public:
    // The game's switches for this picture (bindUpdate has run).
    df::Inputs read(const Settings& s, const df::Board& b);
    void reset() { *this = Controls{}; }

private:
    int coin_[3]{};               // pictures a coin switch stays closed
    int revHeld_{0}, revGap_{0};  // STICK STEERS: the reverse press it makes
};

} // namespace vp
