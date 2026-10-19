// A stand-in for the SDK's drawing calls, for defrun: what the 3D view
// draws (through the SDK's vp3d) is kept as lines in board units, to be
// drawn into a picture (dvg_stub.hpp).
#include "dvg_stub.hpp"

#include <vectorpie/vp.h>

#include <algorithm>

struct vp_dvg
{
    int x{0}, y{0};
    uint8_t r{255}, g{255}, b{255};
    int cx0{0}, cy0{0}, cx1{4095}, cy1{4095};
};

namespace {
vp_dvg stub;
std::vector<BoardLine> drawn;
int jumps = 0;

// Liang-Barsky against the clip rectangle.
bool clip(float& x0, float& y0, float& x1, float& y1)
{
    float t0 = 0, t1 = 1;
    const float dx = x1 - x0, dy = y1 - y0;
    const float p[4] = {-dx, dx, -dy, dy};
    const float q[4] = {x0 - stub.cx0, stub.cx1 - x0, y0 - stub.cy0, stub.cy1 - y0};
    for(int i = 0; i < 4; ++i)
    {
        if(p[i] == 0) { if(q[i] < 0) return false; continue; }
        const float t = q[i] / p[i];
        if(p[i] < 0) t0 = std::max(t0, t);
        else t1 = std::min(t1, t);
        if(t0 > t1) return false;
    }
    const float ax = x0 + t0 * dx, ay = y0 + t0 * dy, bx = x0 + t1 * dx, by = y0 + t1 * dy;
    x0 = ax; y0 = ay; x1 = bx; y1 = by;
    return true;
}
} // namespace

vp_dvg* stubDvg()
{
    return &stub;
}

std::vector<BoardLine>& stubLines()
{
    return drawn;
}

int& stubJumps()
{
    return jumps;
}

extern "C" {

void vp_color(vp_dvg* d, uint8_t r, uint8_t g, uint8_t b)
{
    d->r = r;
    d->g = g;
    d->b = b;
}

void vp_get_color(const vp_dvg* d, uint8_t* r, uint8_t* g, uint8_t* b)
{
    *r = d->r;
    *g = d->g;
    *b = d->b;
}

void vp_moveto(vp_dvg* d, int x, int y)
{
    if(x != d->x || y != d->y) ++jumps;
    d->x = x;
    d->y = y;
}

void vp_lineto(vp_dvg* d, int x, int y)
{
    float x0 = static_cast<float>(d->x), y0 = static_cast<float>(d->y), x1 = static_cast<float>(x), y1 = static_cast<float>(y);
    if((d->r || d->g || d->b) && clip(x0, y0, x1, y1)) drawn.push_back(BoardLine{x0, y0, x1, y1, d->r, d->g, d->b});
    d->x = x;
    d->y = y;
}

void vp_dvg_set_clip(vp_dvg* d, int x0, int y0, int x1, int y1)
{
    d->cx0 = x0; d->cy0 = y0; d->cx1 = x1; d->cy1 = y1;
}

void vp_dvg_reset_clip(vp_dvg* d)
{
    d->cx0 = 0; d->cy0 = 0; d->cx1 = 4095; d->cy1 = 4095;
}

} // extern "C"
