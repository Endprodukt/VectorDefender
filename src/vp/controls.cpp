// The controls: see controls.hpp.
#include "controls.hpp"

#include "bind.hpp"
#include "gen/defender_syms.hpp"

namespace vp {

namespace {

constexpr int COIN_FRAMES = 8;       // the game debounces its coin switches
constexpr int REVERSE_FRAMES = 3;    // STICK STEERS: hold reverse this long,
constexpr int REVERSE_GAP = 8;       // then let go at least this long (REV waits for it)

} // namespace

df::Inputs Controls::read(const Settings& s, const df::Board& b)
{
    df::Inputs r;
    static const Action coins[3] = {ACT_COIN1, ACT_COIN2, ACT_COIN3};
    static const uint8_t coinBits[3] = {df::IN2_COIN1, df::IN2_COIN2, df::IN2_COIN3};
    for(int i = 0; i < 3; ++i)
    {
        if(actPressed(coins[i])) coin_[i] = COIN_FRAMES;
        if(coin_[i] > 0)
        {
            --coin_[i];
            r.in2 |= coinBits[i];
        }
    }
    if(actHeld(ACT_START1)) r.in0 |= df::IN0_START1;
    if(actHeld(ACT_START2)) r.in0 |= df::IN0_START2;

    const bool up = actHeld(ACT_UP), down = actHeld(ACT_DOWN);
    if(up && !down) r.in1 |= df::IN1_UP;
    if(down && !up) r.in0 |= df::IN0_DOWN;
    if(actHeld(ACT_FIRE)) r.in0 |= df::IN0_FIRE;
    if(actHeld(ACT_THRUST)) r.in0 |= df::IN0_THRUST;
    if(actHeld(ACT_SMART)) r.in0 |= df::IN0_SMART;
    if(actHeld(ACT_HYPER)) r.in0 |= df::IN0_HYPER;
    bool reverse = actHeld(ACT_REVERSE);

    // STICK STEERING: left or right turns the ship that way (the original's
    // reverse button, pressed for the player), and thrusts unless set to
    // turn only.
    if(s.stickSteer)
    {
        const bool left = actHeld(ACT_LEFT), right = actHeld(ACT_RIGHT);
        if(left != right)
        {
            if(s.stickSteer == 1) r.in0 |= df::IN0_THRUST;
            const bool facingRight = !(b.m.peek(df::sym::NPLAD) & 0x80);
            if(facingRight == left && revHeld_ == 0 && revGap_ == 0) revHeld_ = REVERSE_FRAMES;
        }
        if(revHeld_ > 0)
        {
            reverse = true;
            if(--revHeld_ == 0) revGap_ = REVERSE_GAP;
        }
        else if(revGap_ > 0) --revGap_;
    }
    if(reverse) r.in0 |= df::IN0_REVERSE;
    return r;
}

} // namespace vp
