// The title's logos, drawn anew as vectors: see designs.hpp.
//
// DEFENDER after the cabinet's marquee: heavy upright block letters, their
// inner shapes rounded, yellow, standing deep out of the screen in red,
// in perspective. Of the depth only what shows is drawn: the sides that
// face away from the vanishing point, joined to the face where they turn,
// and cut where the letters' faces cover them.
//
// Williams: the company's script as one smooth pen line through key points
// of each letter (Catmull-Rom), slanted like the original's, the i's dots
// and the (R) apart; in the order a pen writes it, so the original's
// writing animation can be followed (scene.cpp).
#include "scene/designs.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace df {

namespace {

using Poly = std::vector<DPoint>;
constexpr float PI = 3.14159265f;

void arc(Poly& p, float cx, float cy, float r, float a0, float a1, int steps = 6)
{
    for(int i = 0; i <= steps; ++i)
    {
        const float a = (a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(steps)) * PI / 180.f;
        p.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
    }
}

// Inside a letter's face: inside its outline and in none of its holes
// (even-odd over all its contours).
bool inside(const std::vector<Poly>& contours, float x, float y)
{
    bool in = false;
    for(const Poly& c : contours)
        for(size_t i = 0, j = c.size() - 1; i < c.size(); j = i++)
            if((c[i].y > y) != (c[j].y > y) && x < (c[j].x - c[i].x) * (y - c[i].y) / (c[j].y - c[i].y) + c[i].x)
                in = !in;
    return in;
}

struct Letter
{
    std::vector<Poly> contours;   // [0] the outline (clockwise, y down), then holes (clockwise too)
    float width;
};

// A line, without what lies inside any of the faces: as pieces.
void visiblePieces(Design& d, uint8_t color, DPoint a, DPoint b, const std::vector<std::vector<Poly>>& faces)
{
    const float len = std::hypot(b.x - a.x, b.y - a.y);
    const int n = std::max(2, static_cast<int>(len / 0.2f));
    Poly piece;
    for(int i = 0; i <= n; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(n);
        const DPoint p{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
        bool hidden = false;
        for(const auto& f : faces) hidden = hidden || inside(f, p.x, p.y);
        if(!hidden) piece.push_back(p);
        if(hidden || i == n)
        {
            if(piece.size() >= 2) d.strokes.push_back(DStroke{color, {piece.front(), piece.back()}});
            piece.clear();
        }
    }
}

Letter letterD(float w)
{
    Letter l{{}, w};
    Poly o{{0, 0}, {w - 7, 0}};
    arc(o, w - 7, 7, 7, -90, 0);
    o.push_back({w, 13});
    arc(o, w - 7, 13, 7, 0, 90);
    o.push_back({0, 20});
    Poly h{{5, 5}, {w - 8, 5}};
    arc(h, w - 8, 8, 3, -90, 0, 4);
    arc(h, w - 8, 12, 3, 0, 90, 4);
    h.push_back({5, 15});
    l.contours = {o, h};
    return l;
}

Letter letterE(float w, bool f)
{
    Poly o{{0, 0}, {w, 0}, {w, 5}, {5, 5}, {5, 7.5f}, {w - 3, 7.5f}, {w - 3, 12.5f}, {5, 12.5f}};
    if(f) o.insert(o.end(), {{5, 20}, {0, 20}});
    else o.insert(o.end(), {{5, 15}, {w, 15}, {w, 20}, {0, 20}});
    return Letter{{o}, w};
}

Letter letterN(float w)
{
    return Letter{{{{0, 0}, {5, 0}, {w - 5, 11.f}, {w - 5, 0}, {w, 0}, {w, 20}, {w - 5, 20}, {5, 9.f}, {5, 20}, {0, 20}}}, w};
}

Letter letterR(float w)
{
    Poly o{{0, 0}, {w - 6, 0}};
    arc(o, w - 6, 6, 6, -90, 0, 5);
    arc(o, w - 6, 7, 6, 0, 60, 4);
    o.insert(o.end(), {{w + 0.5f, 20}, {w - 5, 20}, {w - 8, 13}, {5, 13}, {5, 20}, {0, 20}});
    Poly h{{5, 5}, {w - 7, 5}};
    arc(h, w - 7, 6.5f, 1.5f, -90, 90, 4);
    h.push_back({5, 8});
    return Letter{{o, h}, w};
}

} // namespace

Design defenderLogo()
{
    // As on the cabinet's marquee: upright block letters, their faces
    // yellow, standing deep out of the screen in red; the depth seen in
    // perspective from a point above the word's middle, so it goes down,
    // and out to the left under the first letters, to the right under the
    // last.
    constexpr float H = 20.f, GAP = 2.2f;
    const std::vector<Letter> word{letterD(15), letterE(13, false), letterE(13, true), letterE(13, false),
                                   letterN(15), letterD(15), letterE(13, false), letterR(15)};
    float total = 0;
    for(const Letter& l : word) total += l.width + GAP;
    total -= GAP;
    const DPoint vp{total / 2.f, -40.f};                    // where the depth points away from
    constexpr float KX = 0.025f, KY = 0.15f;                // how deep: 9 at the foot, 6 at the top, little to the side
    auto back = [&](DPoint p) { return DPoint{vp.x + (p.x - vp.x) * (1 + KX), vp.y + (p.y - vp.y) * (1 + KY)}; };

    std::vector<std::vector<Poly>> faces;
    float x = 0;
    for(const Letter& l : word)
    {
        std::vector<Poly> f = l.contours;
        for(Poly& c : f)
            for(DPoint& p : c) p.x += x;
        faces.push_back(f);
        x += l.width + GAP;
    }

    Design d;
    // the depth first: the sides turned away from the vanishing point
    // (those it can be seen past), and the edges where they turn; cut
    // where a letter's face is in front
    for(const auto& f : faces)
    {
        const Poly& p = f[0];   // the outline only: inside the holes it would show as bits
        const size_t n = p.size();
        auto shows = [&](size_t i)
        {
            const DPoint a = p[i], b = p[(i + 1) % n];
            const DPoint m{(a.x + b.x) / 2.f, (a.y + b.y) / 2.f}, mb = back(m);
            const float nx = b.y - a.y, ny = -(b.x - a.x);   // outward, clockwise with y down
            return nx * (mb.x - m.x) + ny * (mb.y - m.y) > 0.f;
        };
        for(size_t i = 0; i < n; ++i)
        {
            const DPoint a = p[i], b = p[(i + 1) % n];
            if(shows(i)) visiblePieces(d, FIXED + 1, back(a), back(b), faces);
            if(shows(i) != shows((i + n - 1) % n)) visiblePieces(d, FIXED + 1, a, back(a), faces);
        }
    }
    // then the faces
    for(const auto& f : faces)
        for(Poly c : f)
        {
            c.push_back(c.front());
            d.strokes.push_back(DStroke{FIXED + 0, c});
        }
    // the box round all of it, from (0, 0)
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for(const DStroke& s : d.strokes)
        for(const DPoint& q : s.pts)
        {
            x0 = std::min(x0, q.x); y0 = std::min(y0, q.y);
            x1 = std::max(x1, q.x); y1 = std::max(y1, q.y);
        }
    for(DStroke& s : d.strokes)
        for(DPoint& q : s.pts) { q.x -= x0; q.y -= y0; }
    d.fitW = x1 - x0;
    d.fitH = y1 - y0;
    (void)H;
    return d;
}

namespace {

// A smooth line through key points (Catmull-Rom), `per` points a span.
Poly spline(const Poly& k, int per = 8)
{
    Poly out;
    for(size_t i = 0; i + 1 < k.size(); ++i)
    {
        const DPoint p0 = k[i == 0 ? 0 : i - 1], p1 = k[i], p2 = k[i + 1], p3 = k[i + 2 < k.size() ? i + 2 : i + 1];
        for(int s = 0; s < per; ++s)
        {
            const float t = static_cast<float>(s) / static_cast<float>(per), t2 = t * t, t3 = t2 * t;
            out.push_back({0.5f * (2 * p1.x + (-p0.x + p2.x) * t + (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 +
                                   (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3),
                           0.5f * (2 * p1.y + (-p0.y + p2.y) * t + (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 +
                                   (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3)});
        }
    }
    out.push_back(k.back());
    return out;
}

Design makeWilliams()
{
    constexpr float BASE = 20.f, SLANT = 0.32f;
    Design d;
    auto pen = [&](const Poly& keys) { d.strokes.push_back(DStroke{RANK + 0, spline(keys)}); };
    // W: a curl in from the left, then the three strokes down and up
    pen({{9, 10}, {5, 9.5f}, {3.5f, 6}, {6, 2.5f}, {11, 1.5f}, {14, 4}, {14.5f, 11}, {15, 20}, {19, 12}, {22, 6},
         {22.5f, 13}, {24, 20}, {28, 11}, {31, 4}, {33, 1.5f}});
    // i l l i a m s, joined
    pen({{31, 14}, {34, 10}, {34, 16}, {34.5f, 19.5f}, {37, 18}, {41, 11}, {43.5f, 4}, {42.5f, 1}, {40, 3},
         {39, 11}, {39.5f, 19.5f}, {42, 18}, {46, 11}, {48.5f, 4}, {47.5f, 1}, {45, 3}, {44, 11}, {44.5f, 19.5f},
         {47, 18}, {50, 11}, {50, 16}, {50.5f, 19.5f}, {53, 18}, {57, 12}, {60, 10.8f}, {57, 10.3f}, {54.2f, 12.5f},
         {53.6f, 17}, {55.3f, 20}, {58.3f, 17.5f}, {60.3f, 11}, {59.6f, 17}, {60.6f, 20}, {63.5f, 18}, {65.5f, 12}, {66.5f, 10.5f}, {66, 15},
         {65.5f, 20}, {67.5f, 14}, {70, 11}, {71.5f, 12}, {71, 16}, {70.5f, 20}, {72.5f, 14}, {75, 11}, {76.5f, 12},
         {76, 16}, {76, 19.5f}, {78.5f, 18}, {81, 14}, {82.5f, 10.5f}, {82, 14}, {84, 17}, {82.5f, 20}, {78.5f, 20}});
    // the i's dots
    d.strokes.push_back(DStroke{RANK + 0, {{33.6f, 6.2f}, {34.4f, 6.2f}}});
    d.strokes.push_back(DStroke{RANK + 0, {{49.6f, 6.2f}, {50.4f, 6.2f}}});
    // (R)
    {
        Poly ring;
        arc(ring, 88.5f, 4.5f, 3.f, 0, 360, 12);
        d.strokes.push_back(DStroke{RANK + 0, ring});
        d.strokes.push_back(DStroke{RANK + 0, {{87.5f, 6.4f}, {87.5f, 2.6f}, {89.2f, 2.6f}, {89.6f, 3.6f}, {89.2f, 4.5f},
                                               {87.5f, 4.5f}, {89.6f, 6.4f}}});
    }
    for(DStroke& s : d.strokes)
        for(DPoint& p : s.pts) p.x += (BASE - p.y) * SLANT;
    d.fitW = 92.f + BASE * SLANT;
    d.fitH = 20.5f;
    return d;
}

} // namespace

const Design& williamsLogo()
{
    static const Design d = makeWilliams();
    return d;
}

} // namespace df
