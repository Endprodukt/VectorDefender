// The scene of one picture: see scene.hpp.
#include "scene/scene.hpp"

#include "gen/defender_syms.hpp"
#include "scene/designs.hpp"

#include <algorithm>
#include <cmath>

namespace df {

namespace {

constexpr int W = 304, H = 256;
constexpr int CHARS = 45;
constexpr int NOISE_PIXELS = 4000;   // more unexplained pixels than this: noise
constexpr int NOISE_STEP = 61;

struct Rgb
{
    uint8_t r, g, b;
};

void colors(const Board& b, Rgb out[16])
{
    for(int i = 0; i < 16; ++i) Board::color(b.m.palette[i], out[i].r, out[i].g, out[i].b);
}

int nibble(const Board& b, uint16_t addr, int half)
{
    const uint8_t v = b.m.peekBank(2, addr);
    return half ? (v & 0x0F) : (v >> 4);
}

} // namespace

void Scene::attach(Board& b)
{
    b.m.onHook = &Scene::hookThunk;
    b.m.hookUser = this;
    items_.clear();
}

void Scene::hookThunk(void* user, Machine& m, uint16_t pc)
{
    static_cast<Scene*>(user)->onHook(m, pc);
}

// A picture writer is about to run: Y the descriptor, D the screen address,
// for the object writers the carry the phase. Descriptors and pixels are
// read in bank 2, where the writers read them (the IRQ and CWRIT map it in).
void Scene::onHook(Machine& m, uint16_t pc)
{
    if(pc == sym::BLK71_BGOUT)
    {
        // the terrain is drawn for the scroll position BGL rounded down to
        // a whole pixel: what it lost puts the line where it truly is
        terrainFrac_ = static_cast<float>(m.peek16(sym::BGL) & 0x1F) / 32.f;
        return;
    }
    Item it{};
    it.desc = m.c.y;
    it.x = static_cast<int16_t>(m.c.a * 2);
    it.y = m.c.b;
    it.w = m.peekBank(2, it.desc);
    it.h = m.peekBank(2, static_cast<uint16_t>(it.desc + 1));
    const bool cwrite = pc == sym::CWRIT;
    it.phase = (!cwrite && (m.c.cc & CC_C)) ? 1 : 0;
    const uint16_t p0 = static_cast<uint16_t>((m.peekBank(2, static_cast<uint16_t>(it.desc + 2)) << 8) |
                                              m.peekBank(2, static_cast<uint16_t>(it.desc + 3)));
    const uint16_t p1 = static_cast<uint16_t>((m.peekBank(2, static_cast<uint16_t>(it.desc + 4)) << 8) |
                                              m.peekBank(2, static_cast<uint16_t>(it.desc + 5)));
    it.data = it.phase ? p1 : p0;
    const int idx = (static_cast<int>(it.desc) - sym::ROMS_CHRTBL);
    it.text = cwrite && idx >= 0 && idx < CHARS * 4 && idx % 4 == 0;
    it.glyph = static_cast<int8_t>(it.text ? idx / 4 : -1);
    if(it.w == 0 || it.h == 0 || it.w > 64 || it.h > 64) return;

    // Where it truly is. The original draws in whole pixels; its positions
    // are finer (16 bits: x in 1/32, y in 1/256 of a pixel), so a thing
    // moving 1.5 pixels a picture steps 1, 2, 1, 2 - a judder a sharp
    // vector line shows. Read here, as the picture is written: the ship's
    // PLAX16/PLAY16; an object's OX16/OY16 against the scroll (BGL), the
    // object being in X (OPROC has just stored the address it draws at).
    it.fx = static_cast<float>(it.x + it.phase);
    it.fy = static_cast<float>(it.y);
    if(!cwrite)
    {
        const int a = m.c.a, bline = m.c.b;
        if(it.desc == sym::PLAPIC || it.desc == sym::PLBPIC)
        {
            const uint16_t px16 = m.peek16(sym::PLAX16), py16 = m.peek16(sym::PLAY16);
            if((px16 >> 8) == a && (py16 >> 8) == bline)
            {
                it.fx = static_cast<float>(px16) / 128.f;
                it.fy = static_cast<float>(py16) / 256.f;
            }
        }
        else if(m.c.x >= sym::OLIST && m.c.x < sym::OLEND && m.peek16(static_cast<uint16_t>(m.c.x + 4)) == ((a << 8) | bline))
        {
            const uint16_t ox16 = m.peek16(static_cast<uint16_t>(m.c.x + 0x0A));
            const uint16_t oy16 = m.peek16(static_cast<uint16_t>(m.c.x + 0x0C));
            const float px = static_cast<float>(static_cast<uint16_t>(ox16 - m.peek16(sym::BGL))) / 32.f;
            if(std::fabs(px - it.fx) < 2.f) it.fx = px;
            if((oy16 >> 8) == bline) it.fy = static_cast<float>(oy16) / 256.f;
        }
    }
    // The original writes its pictures whole, background and all: what this
    // one covers of an older picture is gone. An older one it overlaps
    // (most often the same object a step back, not yet found erased) gives
    // way, so a moving object is never seen twice. The same place again
    // keeps its turn in the drawing order.
    const int x0 = it.x, x1 = it.x + it.w * 2, y0 = it.y, y1 = it.y + it.h;
    bool replaced = false;
    for(size_t i = 0; i < items_.size();)
    {
        const Item& o = items_[i];
        const bool same = o.x == it.x && o.y == it.y;
        const bool overlap = o.x < x1 && x0 < o.x + o.w * 2 && o.y < y1 && y0 < o.y + o.h;
        if(same && !replaced)
        {
            items_[i] = it;
            replaced = true;
            ++i;
        }
        else if(overlap) items_.erase(items_.begin() + static_cast<long>(i));
        else ++i;
    }
    if(!replaced) items_.push_back(it);
}

// Is the picture still there? Counts its lit pixels that the screen shows
// as they were written. With mark, claims them (claimed_).
bool Scene::verify(const Board& b, const Item& it, bool mark)
{
    int lit = 0, same = 0;
    for(int cx = 0; cx < it.w; ++cx)
        for(int row = 0; row < it.h; ++row)
        {
            const uint16_t a = static_cast<uint16_t>(it.data + cx * it.h + row);
            for(int half = 0; half < 2; ++half)
            {
                const int v = nibble(b, a, half);
                if(!v) continue;
                ++lit;
                const int x = it.x + cx * 2 + half, y = (it.y + row) & 0xFF;
                if(x < 0 || x >= W) continue;
                if(b.pixel(x, y) == v)
                {
                    ++same;
                    if(mark) claimed_[static_cast<size_t>(y * W + x)] = 1;
                }
            }
        }
    if(lit == 0) return false;
    return same * 4 >= lit;
}

void Scene::build(const Board& b, std::vector<Line>& out, std::vector<Part>* parts)
{
    out.clear();
    if(parts) parts->clear();
    stats_ = SceneStats{};
    objs_.clear();
    curObj_ = -1;
    claimed_.assign(static_cast<size_t>(W * H), 0);
    Rgb pal[16];
    colors(b, pal);
    auto emit = [&](float x0, float y0, float x1, float y1, int c, Part p)
    {
        static const Rgb fixed[2] = {{255, 225, 0}, {225, 20, 10}};   // FIXED: the marquee's yellow and red
        const Rgb& k = (c & FIXED) ? fixed[c & 1] : pal[c & 15];
        if(!k.r && !k.g && !k.b) return;
        out.push_back(Line{x0, y0, x1, y1, k.r, k.g, k.b, curObj_});
        if(parts) parts->push_back(p);
    };
    auto rows = [&](const Item& it, Part part)
    {
        // a picture without design: its rows of pixels
        for(int row = 0; row < it.h; ++row)
        {
            int run = -1, runC = 0;
            const int wpx = it.w * 2;
            for(int px = 0; px <= wpx; ++px)
            {
                int v = 0;
                if(px < wpx) v = nibble(b, static_cast<uint16_t>(it.data + (px >> 1) * it.h + row), px & 1);
                if(run >= 0 && v != runC)
                {
                    const float y = static_cast<float>(it.y + row) + 0.5f;
                    const float x0 = static_cast<float>(it.x + run) + 0.15f, x1 = static_cast<float>(it.x + px) - 0.15f;
                    emit(x0, y, x1, y, runC, part);
                    run = -1;
                }
                if(v && run < 0) { run = px; runC = v; }
            }
        }
    };

    // 1. the pictures still on the screen, in the order they came
    std::vector<Item> kept;
    kept.reserve(items_.size());
    for(const Item& it : items_)
        if(verify(b, it, true)) kept.push_back(it);
    items_.swap(kept);
    for(const Item& it : items_)
    {
        ++stats_.pictures;
        const Design* d = it.text ? glyph(it.glyph) : designFor(it.desc);
        if(!d)
        {
            rows(it, Part::Sprite);
            continue;
        }
        it.text ? ++stats_.glyphs : ++stats_.designed;
        float ox = it.fx, oy = it.fy, sx = 1.f, sy = 1.f;
        int rank[16];
        for(int& r : rank) r = 1;
        if(d->fitW > 0)
        {
            // stretched onto the lit part of the picture; its colours by use
            int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1, used[16] = {};
            for(int cx = 0; cx < it.w * 2; ++cx)
                for(int row = 0; row < it.h; ++row)
                {
                    const int v = nibble(b, static_cast<uint16_t>(it.data + (cx >> 1) * it.h + row), cx & 1);
                    if(!v) continue;
                    ++used[v];
                    x0 = std::min(x0, cx); x1 = std::max(x1, cx + 1);
                    y0 = std::min(y0, row); y1 = std::max(y1, row + 1);
                }
            if(x1 < 0) continue;
            ox += static_cast<float>(x0);
            oy += static_cast<float>(y0);
            sx = static_cast<float>(x1 - x0) / d->fitW;
            sy = static_cast<float>(y1 - y0) / d->fitH;
            for(int k = 0; k < 16; ++k)
            {
                int best = 1, most = -1;
                for(int v = 1; v < 16; ++v)
                {
                    bool taken = false;
                    for(int j = 0; j < k; ++j) taken |= rank[j] == v;
                    if(!taken && used[v] > most) { most = used[v]; best = v; }
                }
                rank[k] = best;
            }
        }
        const Part part = it.text ? Part::Text : Part::Sprite;
        if(!it.text && d->fitW <= 0)
        {
            curObj_ = static_cast<int16_t>(objs_.size());
            objs_.push_back(SpriteObj{it.desc, ox, oy, it.w * 2.f, static_cast<float>(it.h)});
        }
        for(const DStroke& s : d->strokes)
        {
            const int c = (s.color & RANK) ? rank[(s.color & 0x0F)] : s.color;
            if(s.pts.size() == 1)
            {
                const DPoint& p = s.pts[0];
                emit(ox + p.x * sx - 0.2f, oy + p.y * sy, ox + p.x * sx + 0.2f, oy + p.y * sy, c, part);
                continue;
            }
            for(size_t i = 1; i < s.pts.size(); ++i)
                emit(ox + s.pts[i - 1].x * sx, oy + s.pts[i - 1].y * sy, ox + s.pts[i].x * sx, oy + s.pts[i].y * sy, c, part);
        }
        curObj_ = -1;
    }

    // 2. the terrain: each column's piece (two pixels wide, two lines high)
    // where the terrain routine last put it, joined where it is lit
    {
        bool open = false;
        float px = 0, py = 0;
        int pc = 0;
        for(int i = 0; i < 0x98; ++i)
        {
            const uint16_t a = b.m.peek16(static_cast<uint16_t>(sym::BLK71_STBL + i * 2));
            const int cx = a >> 8, cy = a & 0xFF;
            float sx = 0, sy = 0;
            int n = 0, col = 0;
            if(a && cx < W / 2)
                for(int dy = 0; dy < 2; ++dy)
                    for(int half = 0; half < 2; ++half)
                    {
                        const int x = cx * 2 + half, y = (cy + dy) & 0xFF;
                        const int v = b.pixel(x, y);
                        if(!v || claimed_[static_cast<size_t>(y * W + x)]) continue;
                        sx += static_cast<float>(x) + 0.5f;
                        sy += static_cast<float>(y) + 0.5f;
                        col = v;
                        ++n;
                        claimed_[static_cast<size_t>(y * W + x)] = 1;
                    }
            if(!n)
            {
                open = false;
                continue;
            }
            const float x = sx / static_cast<float>(n) - terrainFrac_, y = sy / static_cast<float>(n);
            ++stats_.terrainPoints;
            if(open) emit(px, py, x, y, pc, Part::Terrain);
            px = x;
            py = y;
            pc = col;
            open = true;
        }
    }

    // 3. the effects, from the game's tables
    if(b.m.started)
    {
        penLogo(b, emit);
        explosions(b, emit);
        playerExplosion(b, emit);
        flash(b, emit);
    }

    // 4. the rest: lit and not explained, traced as lines (rest())
    rest(b, [&](const std::vector<DPoint>& pts, int c)
    {
        if(pts.size() == 1)
        {
            emit(pts[0].x - 0.25f, pts[0].y, pts[0].x + 0.25f, pts[0].y, c, Part::Rest);
            ++stats_.restDots;
            return;
        }
        for(size_t i = 1; i < pts.size(); ++i) emit(pts[i - 1].x, pts[i - 1].y, pts[i].x, pts[i].y, c, Part::Rest);
        ++stats_.restRuns;
    });
}

// The smart bomb (and the planet's end) light the background: colour 0
// (COM PCRAM). A tube has no background to light: the playfield's frame
// flashes in that colour instead, three frames deep.
void Scene::flash(const Board& b, const Emit& emit)
{
    if(!b.m.palette[0]) return;
    stats_.flash = 1;
    for(int k = 0; k < 3; ++k)
    {
        const float x0 = 8.f + 6.f * k, x1 = 296.f - 6.f * k, y0 = 44.f + 6.f * k, y1 = 244.f - 6.f * k;
        emit(x0, y0, x1, y0, 0, Part::Rest);
        emit(x1, y0, x1, y1, 0, Part::Rest);
        emit(x1, y1, x0, y1, 0, Part::Rest);
        emit(x0, y1, x0, y0, 0, Part::Rest);
    }
}

// The Williams logo is written by a pen (amode1, LOGO): LGOTAB holds its
// moves, a nibble each (bit 3 left, 2 right, 1 up, 0 down), a pixel set
// after each; FE and a word set the pen down elsewhere, FF is nothing, any
// other byte above AA ends it. The path, decoded once, is drawn where the
// screen shows it lit: the writing appears as the original's does.
void Scene::penLogo(const Board& b, const Emit& emit)
{
    if(!penDecoded_)
    {
        penDecoded_ = true;
        int x = 0, y = 0;
        std::vector<DPoint>* cur = nullptr;
        auto put = [&]()
        {
            if(cur) cur->push_back({static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f});
        };
        for(uint16_t a = sym::LGOTAB; a < 0xD000;)
        {
            uint8_t v = b.m.peekBank(1, a++);
            if(v > 0xAA)
            {
                v = static_cast<uint8_t>(~v);
                if(v == 0) continue;         // FF: nothing
                if(v != 1) break;            // the end
                x = b.m.peekBank(1, a);      // FE: the pen elsewhere
                y = b.m.peekBank(1, static_cast<uint16_t>(a + 1));
                a = static_cast<uint16_t>(a + 2);
                pen_.emplace_back();
                cur = &pen_.back();
                put();
                continue;
            }
            for(int half = 0; half < 2; ++half)
            {
                const int n = half ? (v & 0x0F) : (v >> 4);
                if(half && !n) break;
                if(n & 8) --x;
                if(n & 4) ++x;
                if(n & 2) --y;
                if(n & 1) ++y;
                put();
            }
        }
    }
    // How far the original has written: its pen's pixels lit now. The
    // vector script (williamsLogo, logos.cpp) stands where the original's
    // stands, over the bounds of the pen's whole path, and is drawn as far
    // along its own line as the original is along its path.
    if(penX1_ < penX0_)
        for(const auto& stroke : pen_)
            for(const DPoint& p : stroke)
            {
                penX0_ = std::min(penX0_, p.x - 0.5f);
                penX1_ = std::max(penX1_, p.x + 0.5f);
                penY0_ = std::min(penY0_, p.y - 0.5f);
                penY1_ = std::max(penY1_, p.y + 0.5f);
            }
    int total = 0, lit = 0, col = 0;
    for(const auto& stroke : pen_)
        for(const DPoint& p : stroke)
        {
            const int x = static_cast<int>(p.x), y = static_cast<int>(p.y);
            ++total;
            const int v = (x >= 0 && x < W && y >= 0 && y < H) ? b.pixel(x, y) : 0;
            if(!v || claimed_[static_cast<size_t>(y * W + x)] == 2) continue;
            ++lit;
            col = v;
            claimed_[static_cast<size_t>(y * W + x)] = 1;
        }
    if(!lit || !total) return;
    const Design& d = williamsLogo();
    const float sx = (penX1_ - penX0_) / d.fitW, sy = (penY1_ - penY0_) / d.fitH;
    float length = 0;
    for(const DStroke& s : d.strokes)
        for(size_t i = 1; i < s.pts.size(); ++i)
            length += std::hypot((s.pts[i].x - s.pts[i - 1].x) * sx, (s.pts[i].y - s.pts[i - 1].y) * sy);
    float left = length * static_cast<float>(lit) / static_cast<float>(total);
    for(const DStroke& s : d.strokes)
        for(size_t i = 1; i < s.pts.size() && left > 0; ++i)
        {
            const float ax = penX0_ + s.pts[i - 1].x * sx, ay = penY0_ + s.pts[i - 1].y * sy;
            float bx = penX0_ + s.pts[i].x * sx, by = penY0_ + s.pts[i].y * sy;
            const float seg = std::hypot(bx - ax, by - ay);
            if(seg > left)
            {
                bx = ax + (bx - ax) * left / seg;
                by = ay + (by - ay) * left / seg;
            }
            left -= seg;
            emit(ax, ay, bx, by, col, Part::Text);
        }
}

// Explosions and appearances (samexap7): a RAM block each, RAMSIZ bytes
// from RAMALS: the size (8.8; top bit: appearing), the picture, the centre
// and top left (screen addresses). The original sets the picture's 2 x 2
// blocks `size` times their distance from the centre apart. Here the
// vector picture breaks into pieces of a pixel or two that go where those
// blocks go; a picture without design flies as its pixels.
void Scene::explosions(const Board& b, const Emit& emit)
{
    for(uint16_t blk = sym::RAMALS; blk < sym::RAMEND; blk = static_cast<uint16_t>(blk + sym::RAMSIZ))
    {
        const uint16_t rsize = b.m.peek16(blk);
        if(!rsize) continue;
        ++stats_.explosions;
        const float s = static_cast<float>(rsize & 0x7FFF) / 256.f;
        const uint16_t desc = b.m.peek16(static_cast<uint16_t>(blk + 2));
        const uint16_t centre = b.m.peek16(static_cast<uint16_t>(blk + 6));
        const uint16_t topLeft = b.m.peek16(static_cast<uint16_t>(blk + 8));
        const int w = b.m.peekBank(2, desc), h = b.m.peekBank(2, static_cast<uint16_t>(desc + 1));
        if(w <= 0 || h <= 0 || w > 64 || h > 64) continue;
        const uint16_t data = static_cast<uint16_t>((b.m.peekBank(2, static_cast<uint16_t>(desc + 2)) << 8) |
                                                    b.m.peekBank(2, static_cast<uint16_t>(desc + 3)));
        const float cx = static_cast<float>(((centre >> 8) - (topLeft >> 8)) * 2);
        const float cy = static_cast<float>(static_cast<int>(centre & 0xFF) - static_cast<int>(topLeft & 0xFF));
        const float X0 = static_cast<float>((centre >> 8) * 2), Y0 = static_cast<float>(centre & 0xFF);
        auto mapX = [&](float x) { return X0 + (x - cx) * s; };
        auto mapY = [&](float y) { return Y0 + (y - cy) * s; };
        auto visible = [&](float x, float y) { return x >= 0.f && x < W && y >= 42.f && y < 248.f; };

        // the picture's colours: what the flying blocks may have lit
        bool used[16] = {};
        for(int i = 0; i < w * h; ++i)
        {
            const uint8_t v = b.m.peekBank(2, static_cast<uint16_t>(data + i));
            used[v >> 4] = used[v & 15] = true;
        }
        used[0] = false;

        const Design* d = designFor(desc);
        if(d && d->fitW <= 0)
        {
            for(const DStroke& st : d->strokes)
                for(size_t i = 1; i < st.pts.size(); ++i)
                {
                    const DPoint a = st.pts[i - 1], e = st.pts[i];
                    const float len = std::hypot(e.x - a.x, e.y - a.y);
                    const int pieces = std::max(1, static_cast<int>(std::ceil(len / 1.5f)));
                    for(int k = 0; k < pieces; ++k)
                    {
                        const float t0 = static_cast<float>(k) / pieces, t1 = static_cast<float>(k + 1) / pieces;
                        const float ax = a.x + (e.x - a.x) * t0, ay = a.y + (e.y - a.y) * t0;
                        const float ex = a.x + (e.x - a.x) * t1, ey = a.y + (e.y - a.y) * t1;
                        const float mx = (ax + ex) / 2.f, my = (ay + ey) / 2.f;
                        const float px = mapX(mx), py = mapY(my);
                        if(!visible(px, py)) continue;
                        emit(px + ax - mx, py + ay - my, px + ex - mx, py + ey - my, st.color, Part::Rest);
                    }
                }
        }
        else
        {
            for(int c = 0; c < w * 2; ++c)
                for(int r = 0; r < h; ++r)
                {
                    const int v = nibble(b, static_cast<uint16_t>(data + (c >> 1) * h + r), c & 1);
                    if(!v) continue;
                    const float px = mapX(static_cast<float>(c) + 0.5f), py = mapY(static_cast<float>(r) + 0.5f);
                    if(visible(px, py)) emit(px - 0.3f, py, px + 0.3f, py, v, Part::Rest);
                }
        }

        // what the original lit for it is explained now
        const int x0 = std::max(0, static_cast<int>(mapX(-1.f)) - 2), x1 = std::min(W, static_cast<int>(mapX(w * 2.f + 1.f)) + 3);
        const int y0 = std::max(42, static_cast<int>(mapY(-1.f)) - 2), y1 = std::min(H, static_cast<int>(mapY(h + 1.f)) + 3);
        for(int y = y0; y < y1; ++y)
            for(int x = x0; x < x1; ++x)
            {
                const int v = b.pixel(x, y);
                if(v && used[v] && !claimed_[static_cast<size_t>(y * W + x)]) claimed_[static_cast<size_t>(y * W + x)] = 2;
            }
    }
}

// The player's explosion (blk71, PLEX): PNBITS pieces in TABLE, 10 bytes
// each: screen address, position, velocity (8.8, x in columns of two
// pixels). Each piece is a 2 x 2 block in colour B; here a streak along
// its flight.
void Scene::playerExplosion(const Board& b, const Emit& emit)
{
    for(int i = 0; i < sym::BLK71_PNBITS; ++i)
    {
        const uint16_t e = static_cast<uint16_t>(sym::BLK71_TABLE + i * 10);
        const uint16_t addr = b.m.peek16(e);
        if(!addr || (addr >> 8) >= W / 2) continue;
        const uint8_t v0 = b.m.ram[addr], v1 = b.m.ram[static_cast<uint16_t>(addr + 1)];
        const bool left = v0 == 0xBB && v1 == 0xBB;
        const bool right = v0 == 0x0B && v1 == 0x0B;
        if(!left && !right) continue;
        ++stats_.playerPieces;
        const float x = static_cast<float>((addr >> 8) * 2 + (right ? 1 : 0)) + 1.f;
        const float y = static_cast<float>(addr & 0xFF) + 1.f;
        const float vx = static_cast<float>(static_cast<int16_t>(b.m.peek16(static_cast<uint16_t>(e + 6)))) / 128.f;
        const float vy = static_cast<float>(static_cast<int16_t>(b.m.peek16(static_cast<uint16_t>(e + 8)))) / 256.f;
        emit(x - vx * 2.f, y - vy * 2.f, x, y, 0x0B, Part::Rest);
        for(int dy = -1; dy <= 1; ++dy)
            for(int dx = -1; dx <= 2; ++dx)
            {
                const int px = static_cast<int>(x) + dx - 1, py = static_cast<int>(y) + dy - 1;
                if(px >= 0 && px < W && py >= 0 && py < H && b.pixel(px, py) == 0x0B) claimed_[static_cast<size_t>(py * W + px)] = 2;
            }
    }
}

// The pixels nothing explains, as lines: chains of neighbouring pixels of
// one colour (8 ways) followed from their ends, straightened within 0.6 of
// a pixel; a pixel alone is a dot. Started top to bottom, left to right, so
// the order only changes where the picture does.
void Scene::rest(const Board& b, const std::function<void(const std::vector<DPoint>&, int)>& out)
{
    left_.assign(static_cast<size_t>(W * H), 0);
    int lit = 0;
    for(int y = 0; y < H; ++y)
        for(int x = 0; x < W; ++x)
        {
            const int v = b.pixel(x, y);
            if(v && !claimed_[static_cast<size_t>(y * W + x)])
            {
                left_[static_cast<size_t>(y * W + x)] = static_cast<uint8_t>(v);
                ++lit;
            }
        }
    // A screen full of noise (the power-on RAM test): no lines to find, and
    // tens of thousands of dots no tube draws. A thin, fixed sample of it.
    if(lit > NOISE_PIXELS)
    {
        std::vector<DPoint> dot(1);
        for(int i = 0; i < W * H; i += NOISE_STEP)
            if(left_[static_cast<size_t>(i)])
            {
                dot[0] = {static_cast<float>(i % W) + 0.5f, static_cast<float>(i / W) + 0.5f};
                out(dot, left_[static_cast<size_t>(i)]);
            }
        return;
    }
    static const int DX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    auto at = [&](int x, int y) -> int
    {
        return (x < 0 || y < 0 || x >= W || y >= H) ? 0 : left_[static_cast<size_t>(y * W + x)];
    };
    auto degree = [&](int x, int y, int c)
    {
        int n = 0;
        for(int k = 0; k < 8; ++k) n += at(x + DX[k], y + DY[k]) == c;
        return n;
    };
    std::vector<DPoint> chain, simple;
    for(int pass = 0; pass < 2; ++pass)          // first from the chains' ends, then what is left (loops)
        for(int y = 0; y < H; ++y)
            for(int x = 0; x < W; ++x)
            {
                const int c = at(x, y);
                if(!c || (pass == 0 && degree(x, y, c) > 1)) continue;
                chain.clear();
                int cx = x, cy = y, dir = 0;
                for(;;)
                {
                    chain.push_back({static_cast<float>(cx) + 0.5f, static_cast<float>(cy) + 0.5f});
                    left_[static_cast<size_t>(cy * W + cx)] = 0;
                    int best = -1;
                    for(int t = 0; t < 8; ++t)   // straight on first, then turning ever more
                    {
                        const int k = (dir + (t & 1 ? (t + 1) / 2 : 8 - t / 2)) & 7;
                        if(at(cx + DX[k], cy + DY[k]) == c) { best = k; break; }
                    }
                    if(best < 0) break;
                    dir = best;
                    cx += DX[best];
                    cy += DY[best];
                }
                simplify(chain, simple);
                out(simple, c);
            }
}

// Douglas-Peucker, within `tolerance` pixels.
void Scene::simplify(const std::vector<DPoint>& in, std::vector<DPoint>& out, float tolerance)
{
    out.clear();
    if(in.size() <= 2)
    {
        out = in;
        return;
    }
    std::vector<char> keep(in.size(), 0);
    keep.front() = keep.back() = 1;
    std::vector<std::pair<size_t, size_t>> stack{{0, in.size() - 1}};
    while(!stack.empty())
    {
        const auto [a, e] = stack.back();
        stack.pop_back();
        const float ax = in[a].x, ay = in[a].y, bx = in[e].x, by = in[e].y;
        const float len = std::hypot(bx - ax, by - ay);
        float worst = 0;
        size_t at = a;
        for(size_t i = a + 1; i < e; ++i)
        {
            const float d = len > 1e-3f ? std::fabs((bx - ax) * (ay - in[i].y) - (ax - in[i].x) * (by - ay)) / len
                                        : std::hypot(in[i].x - ax, in[i].y - ay);
            if(d > worst) { worst = d; at = i; }
        }
        if(worst > tolerance)
        {
            keep[at] = 1;
            stack.push_back({a, at});
            stack.push_back({at, e});
        }
    }
    for(size_t i = 0; i < in.size(); ++i)
        if(keep[i]) out.push_back(in[i]);
}

} // namespace df
