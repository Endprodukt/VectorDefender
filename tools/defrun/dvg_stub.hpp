// defrun's stand-in for the board: see dvg_stub.cpp.
#pragma once

#include <cstdint>
#include <vector>

typedef struct vp_dvg vp_dvg;

struct BoardLine
{
    float x0, y0, x1, y1;   // board units, y up
    uint8_t r, g, b;
};

vp_dvg* stubDvg();
std::vector<BoardLine>& stubLines();
int& stubJumps();   // moves of the beam to somewhere else
