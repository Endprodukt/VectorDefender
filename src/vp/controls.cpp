// The controls: see controls.hpp.
#include "controls.hpp"

#include "gen/defender_syms.hpp"
#include "vpbind.h"

#include <vectorpie/vp.h>

namespace vp {

namespace {

constexpr int COIN_FRAMES = 8;       // the game debounces its coin switches
constexpr int REVERSE_FRAMES = 3;    // STICK STEERS: hold reverse this long,
constexpr int REVERSE_GAP = 8;       // then let go at least this long (REV waits for it)

} // namespace

df::Inputs Controls::read(vp_input* in, const Settings& s, const df::Board& b)
{
    df::Inputs r;
    // the cabinet's own: START and the coins (never laid out, vpbind.h)
    static const char* const coins[3] = {"coin1", "coin2", "coin3"};
    static const uint8_t coinBits[3] = {df::IN2_COIN1, df::IN2_COIN2, df::IN2_COIN3};
    for(int i = 0; i < 3; ++i)
    {
        if(vp_input_pressed(in, coins[i])) coin_[i] = COIN_FRAMES;
        if(coin_[i] > 0)
        {
            --coin_[i];
            r.in2 |= coinBits[i];
        }
    }
    if(vp_input_held(in, "start1")) r.in0 |= df::IN0_START1;
    if(vp_input_held(in, "start2")) r.in0 |= df::IN0_START2;
    auto held = [&](Function f) { return vpb_held(s.controls[f]) != 0; };

    const bool up = held(FN_UP), down = held(FN_DOWN);
    if(up && !down) r.in1 |= df::IN1_UP;
    if(down && !up) r.in0 |= df::IN0_DOWN;
    if(held(FN_FIRE)) r.in0 |= df::IN0_FIRE;
    if(held(FN_THRUST)) r.in0 |= df::IN0_THRUST;
    if(held(FN_SMART)) r.in0 |= df::IN0_SMART;
    if(held(FN_HYPER)) r.in0 |= df::IN0_HYPER;
    bool reverse = held(FN_REVERSE);

    // STICK STEERING: left or right turns the ship that way (the original's
    // reverse button, pressed for the player), and thrusts unless set to
    // turn only. The original's switch scan (SSCAN, once a picture) takes
    // only ONE newly closed switch a picture, the lowest bit first: a
    // reverse (bit 6) closed together with thrust or fire would be lost. So
    // the reverse gets a picture of its own: the stick's thrust starts the
    // picture after, and when another switch closes just then the reverse
    // waits a picture.
    if(s.stickSteer)
    {
        const bool left = held(FN_LEFT), right = held(FN_RIGHT);
        bool stickThrust = false;
        if(left != right)
        {
            stickThrust = s.stickSteer == 1;
            const bool facingRight = !(b.m.peek(df::sym::NPLAD) & 0x80);
            if(facingRight == left && revHeld_ == 0 && revGap_ == 0) revHeld_ = REVERSE_FRAMES;
        }
        if(revHeld_ > 0)
        {
            const bool first = revHeld_ == REVERSE_FRAMES;
            const bool othersClose = (r.in0 & ~prevIn0_ & 0x3F) != 0;
            if(!(first && othersClose))
            {
                reverse = true;
                if(--revHeld_ == 0) revGap_ = REVERSE_GAP;
            }
            if(first) stickThrust = false;
        }
        else if(revGap_ > 0) --revGap_;
        if(stickThrust) r.in0 |= df::IN0_THRUST;
    }
    if(reverse) r.in0 |= df::IN0_REVERSE;
    prevIn0_ = r.in0;
    return r;
}

} // namespace vp
