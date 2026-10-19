// Drawing on the board: see render.hpp.
#include "render.hpp"

#include <vectorpie/vp.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace vp {

namespace {

constexpr float VIS_X0 = 6.f, VIS_X1 = 298.f, VIS_Y0 = 7.f, VIS_Y1 = 247.f;

int clampCoord(float v)
{
    return static_cast<int>(std::lround(std::max(0.f, std::min(4095.f, v))));
}

} // namespace

float Renderer::bx(float x) const
{
    const float k = 4096.f / (VIS_X1 - VIS_X0) * static_cast<float>(set_.screenSize) / 100.f;
    return 2048.f + (x - (VIS_X0 + VIS_X1) / 2.f) * k;
}

float Renderer::by(float y) const
{
    const float k = 4096.f / (VIS_Y1 - VIS_Y0) * static_cast<float>(set_.screenSize) / 100.f;
    return 2048.f - (y - (VIS_Y0 + VIS_Y1) / 2.f) * k;
}

float Renderer::ux(float x) const
{
    return 2048.f + (x - 192.f) * 4096.f / 384.f * static_cast<float>(set_.screenSize) / 100.f;
}

float Renderer::uy(float y) const
{
    return 2048.f - (y - 112.f) * 4096.f / 224.f * static_cast<float>(set_.screenSize) / 100.f;
}

void Renderer::scene(vp_dvg* d, const std::vector<df::Line>& lines, const std::vector<df::Part>& parts)
{
    int bx0 = -1, by0 = -1;
    uint8_t cr = 0, cg = 0, cb = 0;
    bool haveColor = false;
    for(size_t i = 0; i < lines.size(); ++i)
    {
        const df::Line& l = lines[i];
        int pct = set_.brightSprites;
        switch(i < parts.size() ? parts[i] : df::Part::Sprite)
        {
            case df::Part::Text: pct = set_.brightText; break;
            case df::Part::Terrain: pct = set_.brightTerrain; break;
            case df::Part::Rest: pct = set_.brightEffects; break;
            default: break;
        }
        const uint8_t r = static_cast<uint8_t>(l.r * pct / 100), g = static_cast<uint8_t>(l.g * pct / 100),
                      b = static_cast<uint8_t>(l.b * pct / 100);
        if(!haveColor || r != cr || g != cg || b != cb)
        {
            vp_color(d, r, g, b);
            cr = r;
            cg = g;
            cb = b;
            haveColor = true;
        }
        const int x0 = clampCoord(bx(l.x0)), y0 = clampCoord(by(l.y0));
        const int x1 = clampCoord(bx(l.x1)), y1 = clampCoord(by(l.y1));
        if(x0 != bx0 || y0 != by0) vp_moveto(d, x0, y0);
        // a dot: give it a length the beam shows
        if(x0 == x1 && y0 == y1) vp_lineto(d, x1 + 4, y1);
        else vp_lineto(d, x1, y1);
        bx0 = x1;
        by0 = y1;
    }
}

namespace ui {

Renderer* R = nullptr;

void line(vp_dvg* d, float x0, float y0, float x1, float y1)
{
    vp_moveto(d, clampCoord(R->ux(x0)), clampCoord(R->uy(y0)));
    vp_lineto(d, clampCoord(R->ux(x1)), clampCoord(R->uy(y1)));
}

void text(vp_dvg* d, const char* s, float x, float y, float h, int align, Rgb c)
{
    vp_color(d, c.r, c.g, c.b);
    vp_text_font(VP_FONT_ASTEROIDS);
    const float hb = R->uy(0.f) - R->uy(h);
    vp_text(d, s, R->ux(x), R->uy(y), hb, align);
}

void textWrapped(vp_dvg* d, const char* s, float x, float y, float h, float maxWidth, Rgb c)
{
    vp_text_font(VP_FONT_ASTEROIDS);
    const float hb = R->uy(0.f) - R->uy(h);
    const float kx = (R->ux(1.f) - R->ux(0.f));   // board units a layout unit
    const std::string t = s;
    if(vp_text_width(s, hb) / kx <= maxWidth)
    {
        text(d, s, x, y, h, VP_TEXT_CENTER, c);
        return;
    }
    // split at the space nearest the middle
    size_t best = std::string::npos;
    for(size_t i = 0; i < t.size(); ++i)
        if(t[i] == ' ' && (best == std::string::npos || std::abs(static_cast<int>(i) - static_cast<int>(t.size() / 2)) <
                                                         std::abs(static_cast<int>(best) - static_cast<int>(t.size() / 2))))
            best = i;
    if(best == std::string::npos)
    {
        text(d, s, x, y, h, VP_TEXT_CENTER, c);
        return;
    }
    text(d, t.substr(0, best).c_str(), x, y - h * 1.7f, h, VP_TEXT_CENTER, c);
    text(d, t.substr(best + 1).c_str(), x, y, h, VP_TEXT_CENTER, c);
}

Rgb scale(Rgb c, int percent)
{
    percent = std::max(0, std::min(100, percent));
    return Rgb{static_cast<uint8_t>(c.r * percent / 100), static_cast<uint8_t>(c.g * percent / 100),
               static_cast<uint8_t>(c.b * percent / 100)};
}

} // namespace ui

} // namespace vp
