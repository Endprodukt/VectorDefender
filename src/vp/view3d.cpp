// The 3D view: see view3d.hpp.
//
// World units are the original's pixels: x right, y up (the screen's y
// turned over), z away from the camera; (0, 0) is the screen's centre
// (pixel 152, 127). The playfield is the plane z = 0. Each model is built
// round its sprite's centre, as big as the sprite, from simple solids
// (prisms, lathed rings, octahedra) and lines, in the original sprite's
// colour indices.
#include "view3d.hpp"

#include "gen/defender_syms.hpp"

#include <vectorpie/vp.h>

#include <algorithm>
#include <cmath>
#include <functional>

namespace vp {

namespace {

constexpr float PI = 3.14159265f;
constexpr float CX = 152.f, CY = 127.f;       // the screen's centre, in pixels
constexpr float HUD_Y = 41.5f;                // above: the scanner and score row, kept flat
constexpr float FOV = 40.f;                   // degrees across
constexpr float TERRAIN_DEPTH = 10.f;         // the terrain band's depth
constexpr float VIS_X0 = 6.f, VIS_X1 = 298.f, VIS_Y0 = 7.f, VIS_Y1 = 247.f;

// Builds one mesh.
struct MB
{
    vp3d_mesh* m{nullptr};
    MB() { vp3d_mesh_create(&m); }
    int v(float x, float y, float z) { return vp3d_mesh_vertex(m, x, y, z); }
    void f(const std::vector<int>& ids) { vp3d_mesh_face(m, ids.data(), static_cast<int>(ids.size())); }
    void l(int a, int b) { vp3d_mesh_line(m, a, b); }
    vp3d_mesh* done(float crease = 40.f)
    {
        vp3d_mesh_finish(m);
        vp3d_mesh_set_crease(m, crease);
        return m;
    }
};

using Pt = std::pair<float, float>;

// A polygon (x, y) made solid: from z = -depth(x) to +depth(x).
void prism(MB& b, const std::vector<Pt>& poly, const std::function<float(float)>& depth)
{
    std::vector<int> fr, bk;
    for(const Pt& p : poly) fr.push_back(b.v(p.first, p.second, -depth(p.first)));
    for(const Pt& p : poly) bk.push_back(b.v(p.first, p.second, depth(p.first)));
    const size_t n = poly.size();
    b.f(fr);
    std::vector<int> back(bk.rbegin(), bk.rend());
    b.f(back);
    for(size_t i = 0; i < n; ++i)
    {
        const size_t j = (i + 1) % n;
        b.f({fr[j], fr[i], bk[i], bk[j]});
    }
}

// A body of revolution round the y axis: rings of (radius, y), n sides;
// the first and last ring closed.
void lathe(MB& b, const std::vector<Pt>& profile, int n, float turn = 0.f)
{
    std::vector<std::vector<int>> rings;
    for(const Pt& r : profile)
    {
        std::vector<int> ring;
        for(int k = 0; k < n; ++k)
        {
            const float a = 2.f * PI * static_cast<float>(k) / static_cast<float>(n) + turn;
            ring.push_back(b.v(r.first * std::cos(a), r.second, r.first * std::sin(a)));
        }
        rings.push_back(ring);
    }
    b.f(rings.front());
    std::vector<int> top(rings.back().rbegin(), rings.back().rend());
    b.f(top);
    for(size_t r = 0; r + 1 < rings.size(); ++r)
        for(int k = 0; k < n; ++k)
        {
            const int k1 = (k + 1) % n;
            b.f({rings[r][k1], rings[r][k], rings[r + 1][k], rings[r + 1][k1]});
        }
}

void octahedron(MB& b, float rx, float ry, float rz, float oy = 0.f)
{
    const int px = b.v(rx, oy, 0), nx = b.v(-rx, oy, 0), py = b.v(0, oy + ry, 0), ny = b.v(0, oy - ry, 0);
    const int pz = b.v(0, oy, rz), nz = b.v(0, oy, -rz);
    b.f({py, pz, px});
    b.f({py, px, nz});
    b.f({py, nz, nx});
    b.f({py, nx, pz});
    b.f({ny, px, pz});
    b.f({ny, nz, px});
    b.f({ny, nx, nz});
    b.f({ny, pz, nx});
}

void box(MB& b, float sx, float sy, float sz, float ox = 0, float oy = 0)
{
    prism(b, {{ox - sx, oy - sy}, {ox + sx, oy - sy}, {ox + sx, oy + sy}, {ox - sx, oy + sy}}, [sz](float) { return sz; });
}

// Lines only (see-through).
struct Lines
{
    MB b;
    void add(float x0, float y0, float z0, float x1, float y1, float z1)
    {
        b.l(b.v(x0, y0, z0), b.v(x1, y1, z1));
    }
};

uint8_t scaled(uint8_t c, int pct)
{
    return static_cast<uint8_t>(c * std::max(0, std::min(100, pct)) / 100);
}

} // namespace

View3D::~View3D()
{
    for(vp3d_mesh* m : meshes_) vp3d_mesh_free(m);
}

void View3D::init()
{
    if(ready_) return;
    ready_ = true;
    using namespace df::sym;
    auto keep = [&](vp3d_mesh* m) { meshes_.push_back(m); return m; };
    auto part = [&](Model& md, vp3d_mesh* m, uint8_t color, bool wire = false)
    {
        md.parts.push_back(Part3{keep(m), color, wire});
    };

    // --- the ship, facing +x: fuselage narrowing to the nose, fin, canopy
    {
        Model md;
        MB hull;
        prism(hull, {{-6.9f, 0.f}, {4.f, 0.f}, {7.f, -1.5f}, {0.2f, -2.f}, {-0.4f, -2.8f}, {-5.8f, -2.8f}, {-6.9f, -1.6f}},
              [](float x) { return x < 0.f ? 2.4f : 2.4f - x * 0.26f; });
        part(md, hull.done(30.f), 6);
        MB fin;
        const int a = fin.v(-6.9f, 0.f, 0), b = fin.v(-2.f, 0.f, 0), c = fin.v(-4.4f, 2.7f, 0), e = fin.v(-5.9f, 2.7f, 0);
        fin.f({a, b, c, e});
        vp3d_mesh* finm = fin.done();
        vp3d_mesh_set_two_sided(finm, 1);
        part(md, finm, 6);
        MB canopy;
        prism(canopy, {{0.4f, -0.1f}, {3.4f, -0.1f}, {2.3f, 1.f}, {1.f, 1.f}}, [](float) { return 0.9f; });
        part(md, canopy.done(), 14);
        Lines belly;
        belly.add(-5.6f, -1.6f, -2.45f, -0.6f, -1.6f, -2.45f);
        belly.add(-5.6f, -1.6f, 2.45f, -0.6f, -1.6f, 2.45f);
        part(md, belly.b.done(), 8, true);
        Lines exhaust;
        exhaust.add(-7.8f, -0.6f, -1.2f, -7.1f, -0.6f, -1.2f);
        exhaust.add(-7.8f, -0.6f, 1.2f, -7.1f, -0.6f, 1.2f);
        exhaust.add(-7.8f, -2.2f, 0.f, -7.1f, -2.2f, 0.f);
        part(md, exhaust.b.done(), 2, true);
        models_[PLAPIC] = md;
        models_[PLBPIC] = md;   // the same ship; the view turns it round
    }

    // --- landers: dome (4; the third frame without), body ring (3), tripod legs
    for(int frame = 0; frame < 3; ++frame)
    {
        Model md;
        md.spinY = 0.25f;
        MB body;
        lathe(body, {{2.f, -1.2f}, {3.6f, 0.f}, {3.6f, 1.6f}, {2.6f, 2.3f}}, 8);
        part(md, body.done(30.f), 3);
        MB dome;
        lathe(dome, {{2.5f, 2.3f}, {1.4f, 4.f}}, 8);
        part(md, dome.done(30.f), frame < 2 ? 4 : 3);
        Lines legs;
        for(int k = 0; k < 3; ++k)
        {
            const float a = PI / 2.f + 2.f * PI * k / 3.f;
            legs.add(2.f * std::cos(a), -1.2f, 2.f * std::sin(a), 4.6f * std::cos(a), -4.f, 4.6f * std::sin(a));
        }
        part(md, legs.b.done(), 3, true);
        models_[frame == 0 ? LNDP1 : frame == 1 ? LNDP2 : LNDP3] = md;
    }

    // --- mutant: a core (8) under a head (C), green limbs bent out, a stalk (7)
    {
        Model md;
        md.spinY = 0.6f;
        MB core;
        octahedron(core, 1.8f, 1.6f, 1.8f, 0.3f);
        part(md, core.done(), 8);
        MB head;
        box(head, 0.9f, 0.9f, 0.9f, 0.f, 3.f);
        part(md, head.done(), 12);
        Lines limbs;
        for(int k = 0; k < 4; ++k)
        {
            const float a = PI / 4.f + PI / 2.f * k, c = std::cos(a), s = std::sin(a);
            limbs.add(1.2f * c, 2.3f, 1.2f * s, 3.4f * c, 0.6f, 3.4f * s);
            limbs.add(3.4f * c, 0.6f, 3.4f * s, 2.2f * c, -1.2f, 2.2f * s);
            limbs.add(2.2f * c, -1.2f, 2.2f * s, 4.4f * c, -4.f, 4.4f * s);
        }
        part(md, limbs.b.done(), 3, true);
        Lines stalk;
        stalk.add(0.f, -1.3f, 0.f, 0.f, -4.f, 0.f);
        part(md, stalk.b.done(), 7, true);
        models_[SCZP1] = md;
    }

    // --- baiter: a flat saucer (3), a ring of windows (7) and lights (4)
    for(int frame = 0; frame < 3; ++frame)
    {
        Model md;
        md.spinY = 0.9f;
        MB hull;
        lathe(hull, {{3.6f, -1.6f}, {6.f, 0.f}, {3.4f, 1.7f}}, 12);
        part(md, hull.done(25.f), 3);
        Lines windows, lights;
        for(int k = 0; k < 6; ++k)
        {
            const float a = 2.f * PI * (k + frame / 3.f) / 6.f;
            windows.add(4.6f * std::cos(a), 0.9f, 4.6f * std::sin(a), 4.4f * std::cos(a + 0.12f), 0.9f, 4.4f * std::sin(a + 0.12f));
            lights.add(6.2f * std::cos(a + 0.5f), 0.f, 6.2f * std::sin(a + 0.5f), 6.2f * std::cos(a + 0.68f), 0.f, 6.2f * std::sin(a + 0.68f));
        }
        part(md, windows.b.done(), 7, true);
        part(md, lights.b.done(), 4, true);
        models_[frame == 0 ? UFOP1 : frame == 1 ? UFOP2 : UFOP3] = md;
    }

    // --- bomber: a tumbling cube (D) with a square on each side (E), a light (F)
    {
        Model md;
        md.spinY = 0.2f;
        md.spinX = 0.13f;
        MB cube;
        box(cube, 2.4f, 2.4f, 2.4f);
        part(md, cube.done(), 13);
        Lines inner;
        const float s = 1.3f, z = 2.45f;
        for(float zz : {-z, z})
        {
            inner.add(-s, -s, zz, s, -s, zz);
            inner.add(s, -s, zz, s, s, zz);
            inner.add(s, s, zz, -s, s, zz);
            inner.add(-s, s, zz, -s, -s, zz);
        }
        part(md, inner.b.done(), 14, true);
        Lines light;
        light.add(-0.25f, 0.f, -2.5f, 0.25f, 0.f, -2.5f);
        light.add(-0.25f, 0.f, 2.5f, 0.25f, 0.f, 2.5f);
        part(md, light.b.done(), 15, true);
        for(uint16_t dsc : {TIEP1, TIEP2, TIEP3, TIEP4}) models_[dsc] = md;
    }

    // --- pod: a star: an octahedron (C) with spikes (8) and tips (F, D)
    {
        Model md;
        md.spinY = 0.25f;
        md.spinX = 0.11f;
        MB core;
        octahedron(core, 2.f, 2.f, 2.f);
        part(md, core.done(), 12);
        Lines spikes, up, side;
        spikes.add(-3.f, 0, 0, 3.f, 0, 0);
        spikes.add(0, -3.f, 0, 0, 3.f, 0);
        spikes.add(0, 0, -3.f, 0, 0, 3.f);
        up.add(0, 3.f, 0, 0, 3.8f, 0);
        up.add(0, -3.f, 0, 0, -3.8f, 0);
        side.add(3.f, 0, 0, 3.8f, 0, 0);
        side.add(-3.f, 0, 0, -3.8f, 0, 0);
        part(md, spikes.b.done(), 8, true);
        part(md, up.b.done(), 15, true);
        part(md, side.b.done(), 13, true);
        models_[PRBP1] = md;
    }

    // --- swarmer: a small red dart (2)
    {
        Model md;
        md.spinY = 1.2f;
        MB dart;
        octahedron(dart, 2.6f, 1.8f, 1.6f);
        part(md, dart.done(), 2);
        models_[SWPIC1] = md;
    }

    // --- mines: a turning jack (A)
    for(uint16_t dsc : {BMBP1, BMBP2})
    {
        Model md;
        md.spinY = 0.8f;
        md.spinX = 0.5f;
        Lines jack;
        jack.add(-1.4f, 0, 0, 1.4f, 0, 0);
        jack.add(0, -1.4f, 0, 0, 1.4f, 0);
        jack.add(0, 0, -1.4f, 0, 0, 1.4f);
        part(md, jack.b.done(), 10, true);
        models_[dsc] = md;
    }

    // --- humanoids: a head (3), body and legs (7), arms (8)
    for(int frame = 0; frame < 4; ++frame)
    {
        Model md;
        MB head;
        box(head, 0.75f, 0.75f, 0.75f, 0.f, 3.f);
        part(md, head.done(), 3);
        Lines body, arms;
        body.add(0, 2.2f, 0, 0, -0.6f, 0);
        const bool apart = frame == 1 || frame == 3;
        body.add(0, -0.6f, 0, apart ? -0.9f : 0.f, -4.f, apart ? -0.4f : 0.f);
        if(apart) body.add(0, -0.6f, 0, 0.9f, -4.f, 0.4f);
        arms.add(-1.f, -0.5f, 0, -1.f, 1.2f, 0);
        arms.add(-1.f, 1.2f, 0, 0, 1.8f, 0);
        arms.add(0, 1.8f, 0, 1.f, 1.2f, 0);
        arms.add(1.f, 1.2f, 0, 1.f, -0.5f, 0);
        part(md, body.b.done(), 7, true);
        part(md, arms.b.done(), 8, true);
        static const uint16_t frames[4] = {ASTP1, ASTP2, ASTP3, ASTP4};
        models_[frames[frame]] = md;
    }
}

void View3D::draw(vp_dvg* d, const df::Board& b, const std::vector<df::Line>& lines,
                  const std::vector<df::Part>& parts, const std::vector<df::SpriteObj>& objs,
                  const Settings& s, float dt)
{
    init();
    time_ += dt;
    const float size = static_cast<float>(s.screenSize) / 100.f;

    // the flat part (scanner, score row): as the classic view maps it
    auto bx = [&](float x) { return 2048.f + (x - (VIS_X0 + VIS_X1) / 2.f) * 4096.f / (VIS_X1 - VIS_X0) * size; };
    auto by = [&](float y) { return 2048.f - (y - (VIS_Y0 + VIS_Y1) / 2.f) * 4096.f / (VIS_Y1 - VIS_Y0) * size; };
    auto clampi = [](float v) { return static_cast<int>(std::lround(std::max(0.f, std::min(4095.f, v)))); };

    // the camera: straight on it would show the field as the classic view
    // does; it looks a little from above and the side, or swings round
    vp3d_view view;
    vp3d_view_init(&view, FOV);
    view.near_z = 1.f;
    view.far_z = 5000.f;
    const float halfW = (VIS_X1 - VIS_X0) / 2.f * (4.f / 3.f) / ((VIS_X1 - VIS_X0) / (VIS_Y1 - VIS_Y0));
    const float dist = halfW / std::tan(FOV / 2.f * PI / 180.f) / size;
    float yaw = -0.28f, pitch = 0.22f;
    if(s.view == 2)
    {
        // 3D DYNAMIC: the camera turns the way the ship flies, the more the
        // faster it goes (how fast the world scrolls, BGL), as if to see
        // further ahead; it tilts a little with the ship's height. Damped,
        // so it swings round smoothly when the ship reverses.
        const uint16_t bgl = b.m.peek16(df::sym::BGL);
        const float speed = haveBgl_ ? std::fabs(static_cast<float>(static_cast<int16_t>(bgl - lastBgl_))) / 32.f : 0.f;
        lastBgl_ = bgl;
        haveBgl_ = true;
        float wantYaw = 0.f, wantPitch = 0.2f;
        for(const df::SpriteObj& o : objs)
            if(o.desc == df::sym::PLAPIC || o.desc == df::sym::PLBPIC)
            {
                const float facing = o.desc == df::sym::PLAPIC ? 1.f : -1.f;
                wantYaw = -facing * (0.12f + 0.38f * std::min(1.f, speed / 6.f));
                const float height = std::max(-1.f, std::min(1.f, (o.y + o.h / 2.f - 144.f) / 100.f));
                wantPitch = 0.2f + 0.12f * height;
            }
        const float k = std::min(1.f, dt * 1.6f);
        camYaw_ += (wantYaw - camYaw_) * k;
        camPitch_ += (wantPitch - camPitch_) * k;
        yaw = camYaw_;
        pitch = camPitch_;
    }
    const vp_vec3 target = vp_v3(0.f, -(144.f - CY), 0.f);
    const vp_vec3 eye = vp_v3(target.x + dist * std::sin(yaw) * std::cos(pitch), target.y + dist * std::sin(pitch),
                              target.z - dist * std::cos(yaw) * std::cos(pitch));
    vp3d_look_at(&view, eye, target, vp_v3(0, 1, 0));
    const float kx = (4.f / 3.f) / ((VIS_X1 - VIS_X0) / (VIS_Y1 - VIS_Y0));   // the classic view's stretch
    auto world = [&](float x, float y, float z) { return vp_v3((x - CX) * kx, CY - y, z); };

    uint8_t pal[16][3];
    for(int i = 0; i < 16; ++i) df::Board::color(b.m.palette[i], pal[i][0], pal[i][1], pal[i][2]);

    // 1. the flat top row
    int lastX = -1, lastY = -1;
    for(size_t i = 0; i < lines.size(); ++i)
    {
        const df::Line& l = lines[i];
        if(std::max(l.y0, l.y1) >= HUD_Y) continue;
        const int pct = parts[i] == df::Part::Text ? s.brightText : parts[i] == df::Part::Rest ? s.brightEffects : s.brightSprites;
        vp_color(d, scaled(l.r, pct), scaled(l.g, pct), scaled(l.b, pct));
        const int x0 = clampi(bx(l.x0)), y0 = clampi(by(l.y0)), x1 = clampi(bx(l.x1)), y1 = clampi(by(l.y1));
        if(x0 != lastX || y0 != lastY) vp_moveto(d, x0, y0);
        if(x0 == x1 && y0 == y1) vp_lineto(d, x1 + 4, y1);
        else vp_lineto(d, x1, y1);
        lastX = x1;
        lastY = y1;
    }

    // the field below it only
    vp_dvg_set_clip(d, 0, 0, 4095, clampi(by(HUD_Y + 0.5f)));

    // 2. the rest of the lines on the field's plane, projected here and
    // drawn as the classic view draws: one stroke on where the last one
    // ended (every jump of the beam is a bright dot on the tube); the
    // terrain as a band: its front, its back, then the struts between
    int bx0 = -1, by0 = -1;
    uint8_t cr = 0, cg = 0, cb = 0;
    bool colorSet = false;
    auto stroke = [&](vp_vec3 a, vp_vec3 e, uint8_t r, uint8_t g, uint8_t bl)
    {
        float ax, ay, ex, ey;
        if(!vp3d_project(&view, a, &ax, &ay) || !vp3d_project(&view, e, &ex, &ey)) return;
        if(!colorSet || r != cr || g != cg || bl != cb)
        {
            vp_color(d, r, g, bl);
            cr = r; cg = g; cb = bl;
            colorSet = true;
        }
        const int x0 = clampi(ax), y0 = clampi(ay), x1 = clampi(ex), y1 = clampi(ey);
        if(x0 != bx0 || y0 != by0) vp_moveto(d, x0, y0);
        if(x0 == x1 && y0 == y1) vp_lineto(d, x1 + 4, y1);
        else vp_lineto(d, x1, y1);
        bx0 = x1;
        by0 = y1;
    };
    auto percent = [&](df::Part p)
    {
        return p == df::Part::Text ? s.brightText : p == df::Part::Terrain ? s.brightTerrain
             : p == df::Part::Rest ? s.brightEffects : s.brightSprites;
    };
    for(int pass = 0; pass < 3; ++pass)   // 0: all but the terrain's back and struts; 1: back; 2: struts
    {
        int terrainIndex = 0;
        for(size_t i = 0; i < lines.size(); ++i)
        {
            const df::Line& l = lines[i];
            if(std::max(l.y0, l.y1) < HUD_Y) continue;
            if(l.obj >= 0 && models_.count(objs[static_cast<size_t>(l.obj)].desc)) continue;   // a model takes its place
            const df::Part p = parts[i];
            if(pass > 0 && p != df::Part::Terrain) continue;
            const int pct = pass == 0 ? percent(p) : percent(p) * 45 / 100;
            const uint8_t r = scaled(l.r, pct), g = scaled(l.g, pct), bl = scaled(l.b, pct);
            if(pass == 0) stroke(world(l.x0, l.y0, 0.f), world(l.x1, l.y1, 0.f), r, g, bl);
            else if(pass == 1) stroke(world(l.x0, l.y0, TERRAIN_DEPTH), world(l.x1, l.y1, TERRAIN_DEPTH), r, g, bl);
            else if(terrainIndex++ % 12 == 0) stroke(world(l.x0, l.y0, 0.f), world(l.x0, l.y0, TERRAIN_DEPTH), r, g, bl);
        }
    }

    // 3. the models
    for(const df::SpriteObj& o : objs)
    {
        const auto it = models_.find(o.desc);
        if(it == models_.end()) continue;
        const Model& md = it->second;
        const float cx = o.x + o.w / 2.f, cy = o.y + o.h / 2.f;
        if(cy < HUD_Y) continue;   // the small ships and bombs of the score row stay flat
        vp_xf turn = vp_xf_identity();
        if(o.desc == df::sym::PLAPIC || o.desc == df::sym::PLBPIC)
        {
            // the ship: it turns round through its side when it reverses,
            // banks a little as it climbs or dives
            const float want = o.desc == df::sym::PLAPIC ? 0.f : PI;
            if(!shipSeen_) shipYaw_ = want;
            const float step = PI * dt / 0.22f;
            if(std::fabs(want - shipYaw_) <= step) shipYaw_ = want;
            else shipYaw_ += want > shipYaw_ ? step : -step;
            const float dy = shipSeen_ ? cy - shipLastY_ : 0.f;
            shipRoll_ += (std::max(-0.6f, std::min(0.6f, dy * 0.35f)) - shipRoll_) * std::min(1.f, dt * 10.f);
            shipLastY_ = cy;
            shipSeen_ = true;
            turn = vp_xf_mul2(vp_xf_rot_y(shipYaw_), vp_xf_rot_x(shipRoll_));
        }
        else
        {
            const float phase = (o.x * 0.013f + o.y * 0.007f) * 2.f * PI;
            turn = vp_xf_mul2(vp_xf_rot_y(time_ * md.spinY * 2.f * PI + phase), vp_xf_rot_x(time_ * md.spinX * 2.f * PI));
        }
        const vp_vec3 at = world(cx, cy, 0.f);
        const vp_xf place = vp_xf_mul2(vp_xf_translate(at.x, at.y, at.z), turn);
        for(const Part3& pt : md.parts)
        {
            const uint8_t* c = pal[pt.color & 15];
            vp_color(d, scaled(c[0], s.brightSprites), scaled(c[1], s.brightSprites), scaled(c[2], s.brightSprites));
            vp3d_draw_mesh(d, &view, pt.mesh, &place, pt.wire ? VP3D_WIREFRAME : VP3D_SOLID);
        }
    }
    if(std::none_of(objs.begin(), objs.end(), [](const df::SpriteObj& o) {
           return o.desc == df::sym::PLAPIC || o.desc == df::sym::PLBPIC;
       }))
        shipSeen_ = false;
    vp_dvg_reset_clip(d);
}

} // namespace vp
