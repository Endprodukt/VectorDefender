// defrun - the translated Defender without a window, for testing (PC only).
//
//   defrun <frames> [--dump f1,f2,...] [--out dir] [--press frame:port:mask:count]...
//          [--cmos file] [--trace]
//
// --dump writes the original's picture (which the program still draws into
// its RAM) of those frames as PPM files <out>/f<frame>.ppm, 304 x 256.
// --press holds input bits: port 0..2 (IN0..IN2, board.hpp), for count frames.
#include "game/board.hpp"
#include "scene/designs.hpp"
#include "scene/scene.hpp"
#include "vp/view3d.hpp"
#include "dvg_stub.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

struct Press
{
    long frame;
    int port;
    int mask;
    long count;
};

void dump(const df::Board& b, const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if(!f) return;
    std::fprintf(f, "P6\n304 256\n255\n");
    uint8_t rgb[16][3];
    for(int i = 0; i < 16; ++i) df::Board::color(b.m.palette[i], rgb[i][0], rgb[i][1], rgb[i][2]);
    for(int y = 0; y < 256; ++y)
        for(int x = 0; x < 304; ++x) std::fwrite(rgb[b.pixel(x, y)], 1, 3, f);
    std::fclose(f);
}

// The scene's lines, drawn 4 times the original's size on black, as the
// tube would show them (thin, additive).
void dumpVectors(const std::vector<df::Line>& lines, const std::string& path)
{
    constexpr int S = 4, IW = 304 * S, IH = 256 * S;
    std::vector<float> img(static_cast<size_t>(IW * IH * 3), 0.f);
    auto plot = [&](int x, int y, const df::Line& l, float a)
    {
        if(x < 0 || y < 0 || x >= IW || y >= IH) return;
        float* p = &img[static_cast<size_t>((y * IW + x) * 3)];
        p[0] += l.r * a;
        p[1] += l.g * a;
        p[2] += l.b * a;
    };
    for(const df::Line& l : lines)
    {
        const float x0 = l.x0 * S, y0 = l.y0 * S, x1 = l.x1 * S, y1 = l.y1 * S;
        const float len = std::hypot(x1 - x0, y1 - y0);
        const int n = std::max(1, static_cast<int>(len * 2));
        for(int i = 0; i <= n; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(n);
            const float x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t;
            const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
            for(int dy = -1; dy <= 1; ++dy)
                for(int dx = -1; dx <= 1; ++dx)
                {
                    const float d = std::hypot(static_cast<float>(ix + dx) + 0.5f - x, static_cast<float>(iy + dy) + 0.5f - y);
                    const float a = std::max(0.f, 1.f - d / 1.4f) * 0.5f;
                    if(a > 0) plot(ix + dx, iy + dy, l, a);
                }
        }
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if(!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", IW, IH);
    std::vector<uint8_t> row(static_cast<size_t>(IW * 3));
    for(int y = 0; y < IH; ++y)
    {
        for(int i = 0; i < IW * 3; ++i) row[static_cast<size_t>(i)] = static_cast<uint8_t>(std::min(255.f, img[static_cast<size_t>(y * IW * 3 + i)]));
        std::fwrite(row.data(), 1, row.size(), f);
    }
    std::fclose(f);
}

// What went to the (stand-in) board, on a 4:3 picture 1216 x 912.
void dumpBoard(const std::vector<BoardLine>& lines, const std::string& path)
{
    constexpr int IW = 1216, IH = 912;
    std::vector<float> img(static_cast<size_t>(IW * IH * 3), 0.f);
    for(const BoardLine& l : lines)
    {
        const float x0 = l.x0 * IW / 4096.f, y0 = (4095.f - l.y0) * IH / 4096.f;
        const float x1 = l.x1 * IW / 4096.f, y1 = (4095.f - l.y1) * IH / 4096.f;
        const int n = std::max(1, static_cast<int>(std::hypot(x1 - x0, y1 - y0) * 2));
        for(int i = 0; i <= n; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(n);
            const int x = static_cast<int>(x0 + (x1 - x0) * t), y = static_cast<int>(y0 + (y1 - y0) * t);
            for(int dy = 0; dy <= 1; ++dy)
                for(int dx = 0; dx <= 1; ++dx)
                {
                    const int px = x + dx, py = y + dy;
                    if(px < 0 || py < 0 || px >= IW || py >= IH) continue;
                    float* p = &img[static_cast<size_t>((py * IW + px) * 3)];
                    p[0] += l.r * 0.35f;
                    p[1] += l.g * 0.35f;
                    p[2] += l.b * 0.35f;
                }
        }
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if(!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", IW, IH);
    for(float v : img)
    {
        const uint8_t b = static_cast<uint8_t>(std::min(255.f, v));
        std::fwrite(&b, 1, 1, f);
    }
    std::fclose(f);
}

// The title's logos as designed (logos.cpp), big: Williams over DEFENDER,
// glowing lines on black, 1920 x 900 - to look at them while drawing them.
void logos(const std::string& path)
{
    constexpr int IW = 1920, IH = 900;
    std::vector<float> img(static_cast<size_t>(IW * IH * 3), 0.f);
    auto line = [&](float x0, float y0, float x1, float y1, const float c[3], float width)
    {
        const float len = std::hypot(x1 - x0, y1 - y0);
        const int n = std::max(1, static_cast<int>(len * 2));
        const int r = static_cast<int>(std::ceil(width * 3));
        for(int i = 0; i <= n; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(n);
            const float x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t;
            for(int dy = -r; dy <= r; ++dy)
                for(int dx = -r; dx <= r; ++dx)
                {
                    const int px = static_cast<int>(x) + dx, py = static_cast<int>(y) + dy;
                    if(px < 0 || py < 0 || px >= IW || py >= IH) continue;
                    const float d = std::hypot(static_cast<float>(dx), static_cast<float>(dy));
                    const float a = d < width ? 0.5f : 0.06f * std::exp(-(d - width) / width);   // core and glow
                    float* p = &img[static_cast<size_t>((py * IW + px) * 3)];
                    for(int k = 0; k < 3; ++k) p[k] += c[k] * a / static_cast<float>(n) * len / 2.f;
                }
        }
    };
    auto design = [&](const df::Design& d, float ox, float oy, float scale, const float* const cols[2])
    {
        for(const df::DStroke& s : d.strokes)
        {
            const float* c = cols[(s.color & (df::RANK | df::FIXED)) ? (s.color & 1) : 0];
            for(size_t i = 1; i < s.pts.size(); ++i)
                line(ox + s.pts[i - 1].x * scale, oy + s.pts[i - 1].y * scale, ox + s.pts[i].x * scale,
                     oy + s.pts[i].y * scale, c, 2.2f);
        }
    };
    static const float yellow[3] = {255, 225, 0}, red[3] = {225, 20, 10};
    const float* logoCols[2] = {yellow, red};
    const float* redCols[2] = {red, red};
    const df::Design& w = df::williamsLogo();
    const float ws = 1500.f / w.fitW;
    design(w, (IW - w.fitW * ws) / 2.f, 40.f, ws, redCols);
    if(const df::Design* logo = df::designFor(0xB300))   // DEFDSC
    {
        const float scale = 1600.f / logo->fitW;
        design(*logo, (IW - logo->fitW * scale) / 2.f, 440.f, scale, logoCols);
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if(!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", IW, IH);
    for(float v : img)
    {
        const uint8_t b = static_cast<uint8_t>(std::min(255.f, v));
        std::fwrite(&b, 1, 1, f);
    }
    std::fclose(f);
}

// 16-bit mono WAV at 44.1 kHz.
void writeWav(const std::string& path, const std::vector<float>& s)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if(!f) return;
    const uint32_t n = static_cast<uint32_t>(s.size()), bytes = n * 2;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(1); u32(44100); u32(88200); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    for(float v : s)
    {
        const int16_t x = static_cast<int16_t>(std::max(-1.f, std::min(1.f, v)) * 32767.f);
        std::fwrite(&x, 2, 1, f);
    }
    std::fclose(f);
}

} // namespace

int main(int argc, char** argv)
{
    if(argc == 3 && !std::strcmp(argv[1], "--logos"))
    {
        logos(argv[2]);
        return 0;
    }
    if(argc < 2)
    {
        std::fprintf(stderr, "usage: defrun <frames> [--dump f,..] [--out dir] [--press f:port:mask:n] [--cmos file]\n");
        return 1;
    }
    const long frames = std::atol(argv[1]);
    std::set<long> dumps;
    std::string out = ".", cmos;
    std::vector<Press> presses;
    bool trace = false;
    std::string wav;
    int view = 0;
    int watch = -1;
    long jitterFrom = -1;
    std::vector<float> sound;
    for(int i = 2; i < argc; ++i)
    {
        if(!std::strcmp(argv[i], "--dump") && i + 1 < argc)
        {
            std::string s = argv[++i];
            size_t p = 0;
            while(p < s.size())
            {
                dumps.insert(std::atol(s.c_str() + p));
                p = s.find(',', p);
                if(p == std::string::npos) break;
                ++p;
            }
        }
        else if(!std::strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if(!std::strcmp(argv[i], "--cmos") && i + 1 < argc) cmos = argv[++i];
        else if(!std::strcmp(argv[i], "--trace")) trace = true;
        else if(!std::strcmp(argv[i], "--wav") && i + 1 < argc) wav = argv[++i];
        else if(!std::strcmp(argv[i], "--view") && i + 1 < argc) view = std::atoi(argv[++i]);
        else if(!std::strcmp(argv[i], "--watch") && i + 1 < argc) watch = static_cast<int>(std::strtol(argv[++i], nullptr, 16));
        else if(!std::strcmp(argv[i], "--jitter") && i + 1 < argc) jitterFrom = std::atol(argv[++i]);
        else if(!std::strcmp(argv[i], "--press") && i + 1 < argc)
        {
            Press p{};
            if(std::sscanf(argv[++i], "%ld:%d:%i:%ld", &p.frame, &p.port, &p.mask, &p.count) == 4) presses.push_back(p);
        }
    }

    static df::Board board;
    static df::Scene scene;
    static vp::View3D view3d;
    vp::Settings settings;
    std::vector<df::Part> parts;
    std::vector<df::Line> lines;
    scene.attach(board);
    board.powerOn(cmos);
    for(long f = 0; f < frames; ++f)
    {
        df::Inputs in;
        for(const Press& p : presses)
            if(f >= p.frame && f < p.frame + p.count)
            {
                uint8_t* port = p.port == 0 ? &in.in0 : p.port == 1 ? &in.in1 : &in.in2;
                *port |= static_cast<uint8_t>(p.mask);
            }
        board.frame(in, wav.empty() ? 0 : 735);
        if(jitterFrom >= 0 && f >= jitterFrom)
        {
            // --jitter from: how unevenly things move, from that frame on: per
            // object the change of its step from one picture to the next
            // (0 for an even move), at the true position and rounded down to
            // the pixel as the original draws
            struct Seen { uint16_t desc; float x, y, dx, rx, rdx; };
            static std::vector<Seen> before;
            static double sumTrue = 0, sumRound = 0;
            static long count = 0;
            std::vector<Seen> now;
            scene.build(board, lines);
            for(const df::SpriteObj& o : scene.objects())
            {
                Seen s{o.desc, o.x, o.y, 0, std::floor(o.x), 0};
                for(const Seen& p : before)
                    if(p.desc == o.desc && std::fabs(p.x - o.x) < 6 && std::fabs(p.y - o.y) < 6)
                    {
                        s.dx = o.x - p.x;
                        s.rdx = s.rx - p.rx;
                        if(p.dx != 0 && std::fabs(s.dx) > 0.2f)
                        {
                            sumTrue += std::fabs(s.dx - p.dx);
                            sumRound += std::fabs(s.rdx - p.rdx);
                            ++count;
                        }
                        break;
                    }
                now.push_back(s);
            }
            before.swap(now);
            if(f == frames - 1 && count)
                std::fprintf(stderr, "jitter over %ld steps: true positions %.3f px, whole pixels %.3f px\n", count,
                             sumTrue / count, sumRound / count);
        }
        if(watch >= 0)
        {
            // --watch addr: the byte there (hex), whenever it changes
            static int last = -1;
            const int v = board.m.peek(static_cast<uint16_t>(watch));
            if(v != last) std::fprintf(stderr, "f=%ld %04X=%02X in0=%02X\n", f, watch, v, in.in0);
            last = v;
        }
        sound.insert(sound.end(), board.audio.begin(), board.audio.end());
        if(board.m.broken)
        {
            std::fprintf(stderr, "stopped at frame %ld\n", f);
            break;
        }
        if(trace)
        {
            scene.build(board, lines);
            const df::SceneStats& s = scene.stats();
            if(s.explosions || s.playerPieces || s.flash)
                std::fprintf(stderr, "f=%ld explosions=%d pieces=%d flash=%d\n", f, s.explosions, s.playerPieces, s.flash);
        }
        if(trace && f % 600 == 0)
            std::fprintf(stderr, "f=%ld pc=%04X bank=%d cyc=%lld\n", f, board.m.c.pc, board.m.bank,
                         static_cast<long long>(board.m.c.cyc));
        if(view)
        {
            settings.view = view;
            scene.build(board, lines, &parts);
            stubLines().clear();
            stubJumps() = 0;
            view3d.draw(stubDvg(), board, lines, parts, scene.objects(), settings, 1.f / 60.f);
            if(dumps.count(f))
            {
                dumpBoard(stubLines(), out + "/w" + std::to_string(f) + ".ppm");
                std::fprintf(stderr, "frame %ld 3D: %zu lines, %d jumps\n", f, stubLines().size(), stubJumps());
            }
        }
        if(dumps.count(f))
        {
            dump(board, out + "/f" + std::to_string(f) + ".ppm");
            scene.build(board, lines);
            dumpVectors(lines, out + "/v" + std::to_string(f) + ".ppm");
            const df::SceneStats& s = scene.stats();
            // the same picture twice, its boxes overlapping: a ghost
            const auto& ob = scene.objects();
            int doubles = 0;
            for(size_t i = 0; i < ob.size(); ++i)
                for(size_t j = i + 1; j < ob.size(); ++j)
                    if(ob[i].desc == ob[j].desc && ob[i].x < ob[j].x + ob[j].w && ob[j].x < ob[i].x + ob[i].w &&
                       ob[i].y < ob[j].y + ob[j].h && ob[j].y < ob[i].y + ob[i].h)
                    {
                        ++doubles;
                        std::fprintf(stderr, "  double %04X at %.0f,%.0f and %.0f,%.0f\n", ob[i].desc, ob[i].x, ob[i].y, ob[j].x, ob[j].y);
                    }
            std::fprintf(stderr, "frame %ld: %d overlapping doubles\n", f, doubles);
            std::fprintf(stderr, "frame %ld: %zu lines, pictures %d (designed %d, letters %d), terrain %d, rest %d runs %d dots\n",
                         f, lines.size(), s.pictures, s.designed, s.glyphs, s.terrainPoints, s.restRuns, s.restDots);
        }
    }
    board.saveCmos();
    if(!wav.empty()) writeWav(wav, sound);
    return board.m.broken ? 2 : 0;
}
