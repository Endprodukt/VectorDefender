// The vector designs: see designs.hpp. Each one follows the original
// picture (tools/pictures.py draws them all): its outline, its parts in the
// original's colour indices, its animation frames.
#include "scene/designs.hpp"

#include "gen/defender_syms.hpp"

#include <cmath>
#include <unordered_map>

namespace df {

namespace {

using S = DStroke;

Design mirror(const Design& d, float width)
{
    Design m = d;
    for(DStroke& s : m.strokes)
        for(DPoint& p : s.pts) p.x = width - p.x;
    return m;
}

Design shift(const Design& d, float dx, float dy)
{
    Design m = d;
    for(DStroke& s : m.strokes)
        for(DPoint& p : s.pts)
        {
            p.x += dx;
            p.y += dy;
        }
    return m;
}

// --- the enemies ------------------------------------------------------------

// Lander, 10 x 8: a dome (index 4), the body with its windows (3), three
// legs. The three frames: the lights move; the last without the dome.
Design lander(int frame)
{
    Design d;
    if(frame < 2) d.strokes.push_back(S{4, {{3.f, 0.4f}, {6.f, 0.4f}, {7.f, 1.6f}, {2.f, 1.6f}, {3.f, 0.4f}}});
    d.strokes.push_back(S{3, {{2.f, 1.6f}, {7.f, 1.6f}, {8.f, 3.3f}, {7.f, 5.f}, {2.f, 5.f}, {1.f, 3.3f}, {2.f, 1.6f}}});
    if(frame < 2)
    {
        const float a = frame == 0 ? 3.5f : 4.5f, b = frame == 0 ? 5.5f : 6.5f;
        d.strokes.push_back(S{4, {{a - 0.3f, 3.3f}, {a + 0.3f, 3.3f}}});
        d.strokes.push_back(S{4, {{b - 0.3f, 3.3f}, {b + 0.3f, 3.3f}}});
    }
    else
    {
        d.strokes.push_back(S{3, {{4.5f, 2.3f}, {4.5f, 4.3f}}});
    }
    d.strokes.push_back(S{3, {{3.f, 5.f}, {0.5f, 7.8f}}});
    d.strokes.push_back(S{3, {{4.5f, 5.f}, {4.5f, 7.8f}}});
    d.strokes.push_back(S{3, {{6.f, 5.f}, {8.5f, 7.8f}}});
    return d;
}

// Mutant ("schitzo"), 10 x 8: the lander gone wrong: a head (C), a core (8)
// on a stalk (7), the green limbs bent out.
Design mutant()
{
    Design d;
    d.strokes.push_back(S{12, {{3.2f, 0.2f}, {4.8f, 0.2f}, {4.8f, 2.2f}, {3.2f, 2.2f}, {3.2f, 0.2f}}});
    d.strokes.push_back(S{8, {{4.f, 2.4f}, {5.4f, 3.6f}, {4.f, 4.8f}, {2.6f, 3.6f}, {4.f, 2.4f}}});
    d.strokes.push_back(S{7, {{4.f, 4.8f}, {4.f, 7.8f}}});
    d.strokes.push_back(S{3, {{2.6f, 1.2f}, {1.4f, 2.6f}, {1.4f, 3.6f}, {2.6f, 4.6f}, {2.6f, 5.4f}, {0.4f, 7.8f}}});
    d.strokes.push_back(S{3, {{6.4f, 1.2f}, {7.6f, 2.6f}, {7.6f, 3.6f}, {6.4f, 4.6f}, {6.4f, 5.4f}, {8.6f, 7.8f}}});
    return d;
}

// Baiter (UFO), 12 x 4: a flat saucer (3), windows (7) and lights (4) that
// run round as the three frames go.
Design baiter(int frame)
{
    Design d;
    d.strokes.push_back(S{3, {{2.5f, 0.4f}, {8.5f, 0.4f}, {10.6f, 2.4f}, {9.5f, 3.6f}, {1.5f, 3.6f}, {0.4f, 2.4f}, {2.5f, 0.4f}}});
    static const float windows[3][3] = {{2.5f, 5.5f, 8.5f}, {4.5f, 7.5f, -1.f}, {3.5f, 6.5f, -1.f}};
    static const float lights[3][4] = {{1.5f, 4.5f, 7.5f, -1.f}, {2.5f, 5.5f, 8.5f, -1.f}, {1.3f, 3.5f, 6.5f, 9.3f}};
    for(float x : windows[frame])
        if(x > 0) d.strokes.push_back(S{7, {{x - 0.3f, 1.5f}, {x + 0.3f, 1.5f}}});
    for(float x : lights[frame])
        if(x > 0) d.strokes.push_back(S{4, {{x - 0.4f, 2.5f}, {x + 0.4f, 2.5f}}});
    return d;
}

// Bomber ("tie"), 8 x 8: a turning box: the front square (D) with its
// inner square (E) and light (F), the back edges (8) moving round.
Design bomber(int frame)
{
    Design d;
    d.strokes.push_back(S{13, {{0.3f, 2.3f}, {4.9f, 2.3f}, {4.9f, 6.9f}, {0.3f, 6.9f}, {0.3f, 2.3f}}});
    d.strokes.push_back(S{14, {{1.4f, 3.4f}, {3.8f, 3.4f}, {3.8f, 5.8f}, {1.4f, 5.8f}, {1.4f, 3.4f}}});
    d.strokes.push_back(S{15, {{2.4f, 4.6f}, {2.8f, 4.6f}}});
    switch(frame)
    {
        case 0:
            d.strokes.push_back(S{8, {{0.3f, 2.3f}, {1.4f, 0.3f}, {5.9f, 0.3f}, {5.9f, 4.9f}, {4.9f, 6.9f}}});
            d.strokes.push_back(S{8, {{4.9f, 2.3f}, {5.9f, 0.3f}}});
            break;
        case 1:
            d.strokes.push_back(S{8, {{0.3f, 2.3f}, {1.4f, 1.2f}, {5.9f, 1.2f}, {5.9f, 5.8f}, {4.9f, 6.9f}}});
            d.strokes.push_back(S{8, {{4.9f, 2.3f}, {5.9f, 1.2f}}});
            break;
        case 2:
            d.strokes.push_back(S{8, {{4.9f, 2.3f}, {5.9f, 2.3f}, {5.9f, 6.9f}, {4.9f, 6.9f}}});
            break;
        default:
            d.strokes.push_back(S{8, {{4.9f, 2.3f}, {5.9f, 3.3f}, {5.9f, 7.8f}, {1.3f, 7.8f}, {0.3f, 6.9f}}});
            d.strokes.push_back(S{8, {{4.9f, 6.9f}, {5.9f, 7.8f}}});
            break;
    }
    return d;
}

// Pod ("probe"), 8 x 8: a star: its core (C), the cross (8), the tips
// (F up and down, D left and right) and the diagonal sparks (E).
Design pod()
{
    Design d;
    d.strokes.push_back(S{12, {{3.5f, 2.2f}, {4.8f, 3.5f}, {3.5f, 4.8f}, {2.2f, 3.5f}, {3.5f, 2.2f}}});
    d.strokes.push_back(S{8, {{3.5f, 1.f}, {3.5f, 6.f}}});
    d.strokes.push_back(S{8, {{1.f, 3.5f}, {6.f, 3.5f}}});
    d.strokes.push_back(S{15, {{3.5f, 0.1f}, {3.5f, 1.f}}});
    d.strokes.push_back(S{15, {{3.5f, 6.f}, {3.5f, 6.9f}}});
    d.strokes.push_back(S{13, {{0.1f, 3.5f}, {1.f, 3.5f}}});
    d.strokes.push_back(S{13, {{6.f, 3.5f}, {6.9f, 3.5f}}});
    d.strokes.push_back(S{14, {{1.2f, 1.2f}, {2.f, 2.f}}});
    d.strokes.push_back(S{14, {{5.8f, 1.2f}, {5.f, 2.f}}});
    d.strokes.push_back(S{14, {{1.2f, 5.8f}, {2.f, 5.f}}});
    d.strokes.push_back(S{14, {{5.8f, 5.8f}, {5.f, 5.f}}});
    return d;
}

// Swarmer, 6 x 4: a red dart (2) with green eyes (3).
Design swarmer()
{
    Design d;
    d.strokes.push_back(S{2, {{2.5f, 0.1f}, {4.9f, 2.5f}, {2.5f, 3.9f}, {0.1f, 2.5f}, {2.5f, 0.1f}}});
    d.strokes.push_back(S{3, {{1.4f, 2.5f}, {1.7f, 2.5f}}});
    d.strokes.push_back(S{3, {{3.3f, 2.5f}, {3.6f, 2.5f}}});
    return d;
}

// Bombs (mines), 4 x 3: an X and a plus, in turn (A).
Design mine(int frame)
{
    Design d;
    if(frame == 0)
    {
        d.strokes.push_back(S{10, {{0.3f, 0.3f}, {2.7f, 2.7f}}});
        d.strokes.push_back(S{10, {{2.7f, 0.3f}, {0.3f, 2.7f}}});
    }
    else
    {
        d.strokes.push_back(S{10, {{1.5f, 0.1f}, {1.5f, 2.9f}}});
        d.strokes.push_back(S{10, {{0.1f, 1.5f}, {2.9f, 1.5f}}});
    }
    return d;
}

// Humanoid (astronaut), 4 x 8, facing left: head (3) with face (4), body
// (7), arms (8), legs (7) together or apart. The right-facing frames are
// the same one pixel on.
Design humanoid(bool apart)
{
    Design d;
    d.strokes.push_back(S{3, {{0.3f, 0.2f}, {1.8f, 0.2f}, {1.8f, 1.7f}, {0.3f, 1.7f}, {0.3f, 0.2f}}});
    d.strokes.push_back(S{4, {{0.4f, 1.1f}, {0.7f, 1.1f}}});
    d.strokes.push_back(S{7, {{1.2f, 1.8f}, {1.2f, 5.f}}});
    d.strokes.push_back(S{8, {{0.4f, 5.f}, {0.4f, 3.f}, {1.2f, 2.4f}, {2.4f, 3.f}, {2.4f, 5.f}}});
    if(apart)
    {
        d.strokes.push_back(S{7, {{1.2f, 5.f}, {0.4f, 7.9f}}});
        d.strokes.push_back(S{7, {{1.2f, 5.f}, {2.f, 7.9f}}});
    }
    else
    {
        d.strokes.push_back(S{7, {{1.2f, 5.f}, {1.2f, 7.9f}}});
    }
    return d;
}

// --- the player ---------------------------------------------------------------

// The ship, 16 x 6, facing right: the fin and hull (6), the orange belly
// (8), the canopy (E), the exhaust (2), the nose tip (3).
Design ship()
{
    Design d;
    d.strokes.push_back(S{6, {{1.1f, 4.6f}, {1.1f, 3.f}, {2.1f, 0.3f}, {3.6f, 0.3f}, {6.f, 3.f}, {12.f, 3.f},
                              {15.f, 4.5f}, {8.2f, 5.f}, {7.6f, 5.8f}, {2.2f, 5.8f}, {1.1f, 4.6f}}});
    d.strokes.push_back(S{8, {{2.2f, 4.3f}, {7.2f, 4.3f}}});
    d.strokes.push_back(S{14, {{9.f, 3.6f}, {11.6f, 3.6f}}});
    d.strokes.push_back(S{9, {{13.f, 4.4f}, {13.8f, 4.4f}}});
    d.strokes.push_back(S{3, {{14.f, 4.5f}, {15.f, 4.5f}}});
    d.strokes.push_back(S{2, {{0.1f, 2.5f}, {0.8f, 2.5f}}});
    d.strokes.push_back(S{2, {{0.1f, 4.5f}, {0.8f, 4.5f}}});
    return d;
}

// The small ship (a man left), 10 x 4.
Design miniShip()
{
    Design d;
    d.strokes.push_back(S{6, {{0.9f, 3.7f}, {0.9f, 1.6f}, {1.3f, 0.3f}, {2.6f, 0.3f}, {4.f, 2.2f}, {7.6f, 2.2f},
                              {9.8f, 3.7f}, {0.9f, 3.7f}}});
    d.strokes.push_back(S{8, {{1.4f, 3.1f}, {2.8f, 3.1f}}});
    d.strokes.push_back(S{14, {{6.3f, 2.8f}, {7.6f, 2.8f}}});
    d.strokes.push_back(S{2, {{0.1f, 1.5f}, {0.5f, 1.5f}}});
    d.strokes.push_back(S{2, {{0.1f, 3.5f}, {0.5f, 3.5f}}});
    return d;
}

// A smart bomb left, 6 x 3: the bomb (9) with its fins and its fuse (C).
Design smartBomb()
{
    Design d;
    d.strokes.push_back(S{9, {{2.2f, 0.2f}, {4.f, 0.2f}, {4.8f, 1.5f}, {4.f, 2.8f}, {2.2f, 2.8f}, {1.5f, 1.5f}, {2.2f, 0.2f}}});
    d.strokes.push_back(S{9, {{0.2f, 0.3f}, {1.5f, 1.5f}, {0.2f, 2.7f}}});
    d.strokes.push_back(S{12, {{4.8f, 1.5f}, {5.9f, 1.5f}}});
    return d;
}

// --- points, explosions ---------------------------------------------------------

// A digit 3 x 5 for the 250 and 500 the game shows where a humanoid is caught.
void digit3x5(Design& d, char ch, float x, uint8_t color)
{
    const float l = x + 0.3f, r = x + 2.7f, t = 0.3f, m = 2.5f, b = 4.7f;
    switch(ch)
    {
        case '2': d.strokes.push_back(S{color, {{l, t}, {r, t}, {r, m}, {l, m}, {l, b}, {r, b}}}); break;
        case '5': d.strokes.push_back(S{color, {{r, t}, {l, t}, {l, m}, {r, m}, {r, b}, {l, b}}}); break;
        default: d.strokes.push_back(S{color, {{l, t}, {r, t}, {r, b}, {l, b}, {l, t}}}); break;
    }
}

Design points(const char* text, const uint8_t* colors)
{
    Design d;
    for(int i = 0; i < 3; ++i) digit3x5(d, text[i], 4.f * static_cast<float>(i), colors[i]);
    return d;
}

void ring(Design& d, uint8_t color, float cx, float cy, float r)
{
    DStroke s{color, {}};
    for(int i = 0; i <= 8; ++i)
    {
        const float a = 0.78539816f * static_cast<float>(i) + 0.3926991f;
        s.pts.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
    }
    d.strokes.push_back(s);
}

// The bomb hitting the ground... the round flash (C), 8 x 8.
Design flash()
{
    Design d;
    ring(d, 12, 4.f, 4.f, 3.9f);
    ring(d, 12, 4.f, 4.f, 2.2f);
    return d;
}

// A humanoid shot (8 x 8): a burst of rays round a ring.
Design humanoidBurst()
{
    Design d;
    ring(d, 12, 4.f, 4.f, 2.f);
    d.strokes.push_back(S{8, {{3.f, 3.f}, {5.f, 5.f}}});
    d.strokes.push_back(S{8, {{5.f, 3.f}, {3.f, 5.f}}});
    d.strokes.push_back(S{6, {{4.f, 0.1f}, {4.f, 1.5f}}});
    d.strokes.push_back(S{6, {{4.f, 6.5f}, {4.f, 7.9f}}});
    d.strokes.push_back(S{6, {{0.1f, 4.f}, {1.5f, 4.f}}});
    d.strokes.push_back(S{6, {{6.5f, 4.f}, {7.9f, 4.f}}});
    d.strokes.push_back(S{14, {{1.2f, 1.2f}, {2.2f, 2.2f}}});
    d.strokes.push_back(S{14, {{6.8f, 6.8f}, {5.8f, 5.8f}}});
    d.strokes.push_back(S{13, {{6.8f, 1.2f}, {5.8f, 2.2f}}});
    d.strokes.push_back(S{13, {{1.2f, 6.8f}, {2.2f, 5.8f}}});
    return d;
}

// A swarmer shot (8 x 7): two rings, red and blue.
Design swarmerBurst()
{
    Design d;
    ring(d, 2, 4.f, 3.5f, 3.5f);
    ring(d, 4, 4.f, 3.5f, 1.9f);
    return d;
}

// --- the font -------------------------------------------------------------------

// The original's letters stand in a 5 x 7 field (pixels 1..5, rows 0..6),
// square with a heavy left stem; these keep the shapes with single strokes
// through the pixel centres.
constexpr float L = 1.5f, R = 5.5f, T = 0.5f, B = 6.5f, M = 3.5f, C = 3.5f;

Design G(std::initializer_list<std::vector<DPoint>> strokes)
{
    Design d;
    for(const auto& s : strokes) d.strokes.push_back(S{1, s});
    return d;
}

std::vector<Design> makeFont()
{
    const std::vector<DPoint> box{{L, T}, {R, T}, {R, B}, {L, B}, {L, T}};
    const std::vector<DPoint> question{{L, 1.5f}, {L, T}, {R, T}, {R, 2.5f}, {C, 3.8f}, {C, 4.6f}};
    std::vector<Design> f(45);
    f[0] = Design{};                                                     // space
    f[1] = G({{{1.5f, T}, {1.5f, 4.6f}}, {{1.5f, 6.3f}, {1.5f, 6.7f}}});  // !
    f[2] = G({{{1.5f, 5.f}, {1.5f, 6.5f}, {0.7f, 7.5f}}});                // ,
    f[3] = G({question, {{C, 6.3f}, {C, 6.7f}}});                         // ? (also " -)
    f[4] = G({{{1.3f, 6.5f}, {1.7f, 6.5f}}});                             // .
    f[5] = f[3];                                                         // / (drawn as ?)
    f[6] = G({box});                                                     // 0
    f[7] = G({{{2.5f, 1.5f}, {4.5f, T}, {4.5f, B}}});                    // 1
    f[8] = G({{{L, T}, {R, T}, {R, 2.5f}, {L, 5.3f}, {L, B}, {R, B}}});  // 2
    f[9] = G({{{L, T}, {R, T}, {R, B}, {L, B}}, {{2.5f, M}, {R, M}}});   // 3
    f[10] = G({{{4.5f, B}, {4.5f, T}, {L, M}, {L, 4.f}, {R, 4.f}}});     // 4
    f[11] = G({{{R, T}, {L, T}, {L, M}, {R, M}, {R, B}, {L, B}}});       // 5
    f[12] = G({{{R, T}, {L, T}, {L, B}, {R, B}, {R, M}, {L, M}}});       // 6
    f[13] = G({{{L, T}, {R, T}, {R, 2.f}, {2.2f, 5.f}, {2.2f, B}}});     // 7
    f[14] = G({box, {{L, M}, {R, M}}});                                  // 8
    f[15] = G({{{R, M}, {L, M}, {L, T}, {R, T}, {R, B}, {L, B}}});       // 9
    f[16] = G({{{1.3f, 2.5f}, {1.7f, 2.5f}}, {{1.3f, 5.5f}, {1.7f, 5.5f}}});   // :
    f[17] = f[3];                                                        // ?
    f[18] = Design{};                                                    // @ (blank)
    f[19] = G({{{L, B}, {L, T}, {R, T}, {R, B}}, {{L, M}, {R, M}}});     // A
    f[20] = G({{{L, B}, {L, T}, {4.6f, T}, {R, 1.4f}, {R, B}, {L, B}}, {{L, M}, {R, M}}});   // B
    f[21] = G({{{R, T}, {L, T}, {L, B}, {R, B}}});                       // C
    f[22] = G({{{L, T}, {4.4f, T}, {R, 1.5f}, {R, 5.5f}, {4.4f, B}, {L, B}, {L, T}}});   // D
    f[23] = G({{{R, T}, {L, T}, {L, B}, {R, B}}, {{L, M}, {4.5f, M}}});  // E
    f[24] = G({{{R, T}, {L, T}, {L, B}}, {{L, M}, {4.5f, M}}});          // F
    f[25] = G({{{R, T}, {L, T}, {L, B}, {R, B}, {R, M}, {C, M}}});       // G
    f[26] = G({{{L, T}, {L, B}}, {{R, T}, {R, B}}, {{L, M}, {R, M}}});   // H
    f[27] = G({{{2.f, T}, {2.f, B}}});                                   // I (4 wide)
    f[28] = G({{{R, T}, {R, B}, {L, B}, {L, 5.f}}});                     // J
    f[29] = G({{{L, T}, {L, B}}, {{R, T}, {2.f, M}, {R, B}}});           // K
    f[30] = G({{{L, T}, {L, B}, {R, B}}});                               // L
    f[31] = G({{{L, B}, {L, T}, {7.5f, T}, {7.5f, B}}, {{4.5f, T}, {4.5f, 3.5f}}});   // M (8 wide)
    f[32] = G({{{L, B}, {L, T}, {R, T}, {R, B}}});                       // N (as the original's)
    f[33] = G({box});                                                    // O
    f[34] = G({{{L, B}, {L, T}, {R, T}, {R, M}, {L, M}}});               // P
    f[35] = G({box, {{4.f, 5.f}, {5.f, 7.5f}}});                         // Q
    f[36] = G({{{L, B}, {L, T}, {R, T}, {R, M}, {L, M}}, {{3.6f, M}, {R, B}}});   // R
    f[37] = f[11];                                                       // S
    f[38] = G({{{0.5f, T}, {5.5f, T}}, {{3.f, T}, {3.f, B}}});           // T
    f[39] = G({{{L, T}, {L, B}, {R, B}, {R, T}}});                       // U
    f[40] = G({{{L, T}, {L, 4.5f}, {C, B}, {R, 4.5f}, {R, T}}});         // V
    f[41] = G({{{L, T}, {L, B}, {6.5f, B}, {6.5f, T}}, {{4.f, T}, {4.f, B}}});   // W (8 wide)
    f[42] = G({{{L, T}, {R, B}}, {{R, T}, {L, B}}});                     // X
    f[43] = G({{{L, T}, {C, M}}, {{R, T}, {C, M}, {C, B}}});             // Y
    f[44] = G({{{L, T}, {R, T}, {L, B}, {R, B}}});                       // Z
    return f;
}

struct Table
{
    std::unordered_map<uint16_t, Design> byDesc;
    std::vector<Design> font;
    Table()
    {
        using namespace sym;
        byDesc[LNDP1] = lander(0);
        byDesc[LNDP2] = lander(1);
        byDesc[LNDP3] = lander(2);
        byDesc[SCZP1] = mutant();
        byDesc[UFOP1] = baiter(0);
        byDesc[UFOP2] = baiter(1);
        byDesc[UFOP3] = baiter(2);
        byDesc[TIEP1] = bomber(0);
        byDesc[TIEP2] = bomber(1);
        byDesc[TIEP3] = bomber(2);
        byDesc[TIEP4] = bomber(3);
        byDesc[PRBP1] = pod();
        byDesc[SWPIC1] = swarmer();
        byDesc[BMBP1] = mine(0);
        byDesc[BMBP2] = mine(1);
        byDesc[ASTP1] = humanoid(false);
        byDesc[ASTP2] = humanoid(true);
        byDesc[ASTP3] = shift(mirror(humanoid(false), 2.6f), 1.f, 0.f);
        byDesc[ASTP4] = shift(mirror(humanoid(true), 2.6f), 1.f, 0.f);
        byDesc[PLAPIC] = ship();
        byDesc[PLBPIC] = mirror(ship(), 16.f);
        byDesc[PLAMIN] = miniShip();
        byDesc[SBPIC] = smartBomb();
        static const uint8_t white[3] = {1, 1, 1}, mixed[3] = {15, 14, 13};
        byDesc[C25P1] = points("250", white);
        byDesc[C5P1] = points("500", mixed);
        byDesc[BXPIC] = flash();
        byDesc[ASXP1] = humanoidBurst();
        byDesc[SWXP1] = swarmerBurst();
        byDesc[NULOB] = Design{};
        byDesc[DEFDSC] = defenderLogo();
        font = makeFont();
    }
};

const Table& table()
{
    static const Table t;
    return t;
}

} // namespace

const Design* designFor(uint16_t descriptor)
{
    const auto& m = table().byDesc;
    const auto it = m.find(descriptor);
    return it == m.end() ? nullptr : &it->second;
}

const Design* glyph(int index)
{
    const auto& f = table().font;
    return index >= 0 && index < static_cast<int>(f.size()) ? &f[static_cast<size_t>(index)] : nullptr;
}

} // namespace df
