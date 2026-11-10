// The vector designs: every picture of the original drawn anew as lines.
//
// A design is laid out in the original picture's own box, in its pixels
// (x right, y down, (0,0) the top left corner of the top left pixel). Each
// stroke has the colour index (0..15) the original's pixels have there, so
// it takes the colour the game gives that index at the moment (the colour
// registers change: flashing, the smart bomb, text colours).
#pragma once

#include <cstdint>
#include <vector>

namespace df {

struct DPoint
{
    float x, y;
};

// A stroke's colour: a colour index of the original, or RANK + k: the k-th
// most used colour of the original picture (for pictures built at run time).
constexpr uint8_t RANK = 0x80;
// Or FIXED + k: a colour of its own, not the game's (k 0 the marquee's
// yellow, 1 its red), for the title logo drawn after the cabinet's marquee.
constexpr uint8_t FIXED = 0x40;

struct DStroke
{
    uint8_t color;                 // colour index of the original, or RANK + k
    std::vector<DPoint> pts;       // a polyline; a single point is a dot
};

struct Design
{
    std::vector<DStroke> strokes;
    // Laid out in a box of fitW x fitH and stretched onto the lit part of
    // the original's picture (its pixels' bounds), when fitW > 0.
    float fitW{0}, fitH{0};
};

// The design for a picture descriptor (its address), or nullptr.
const Design* designFor(uint16_t descriptor);

// A letter or digit of the original's character table (CHRTBL, index 0..44)
// in the same box the original's has (its width in pixels varies: 2, 4, 6
// or 8). The strokes' colour is the original's: index 1.
const Design* glyph(int index);

// The title's logos (logos.cpp): DEFENDER, upright block letters with
// depth, as on the cabinet's marquee (FIXED colours); Williams, the script as one smooth
// pen line, in the order it is written.
Design defenderLogo();
const Design& williamsLogo();

} // namespace df
