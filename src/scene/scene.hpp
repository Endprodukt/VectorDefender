// What the game shows, as lines: the scene of one picture.
//
// The translated program still draws the original's picture into its RAM
// (0000-97FF), and that picture is the truth of what is on the screen. The
// scene reads it three ways:
//
//  - pictures: the hooks on the original's picture writers (hooks.txt) tell
//    which picture went where (descriptor, position, phase). Each one stays
//    as long as its pixels are still in the picture (so every way the game
//    erases, overwrites or clears is followed without knowing it), and is
//    drawn as its vector design (designs.hpp), a letter of the vector font,
//    or, for a picture without design, as its pixels' rows;
//  - the terrain: the line through the screen addresses the terrain routine
//    keeps for each column (STBL), where its pixels are lit;
//  - the effects, from the game's own tables: explosions and appearances
//    (the vector picture breaks into pieces that fly as the original's
//    blocks do), the player's explosion (its pieces as streaks), the smart
//    bomb's flash (frames in the background colour), the Williams logo (the
//    pen path the original writes it with);
//  - the rest (lasers, the scanner, stars, shots): what is lit and not
//    explained by the above, traced as lines; single pixels as dots.
//
// Everything is in the original's pixels: x 0..304 right, y 0..256 down.
// Strokes come out in a fixed order (pictures in the order they appeared,
// the terrain left to right, the rest top to bottom): nothing is sorted per
// picture (the beam would wobble).
#pragma once

#include "game/board.hpp"
#include "scene/designs.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace df {

struct Line
{
    float x0, y0, x1, y1;
    uint8_t r, g, b;
    int16_t obj{-1};    // the sprite (Scene::objects()) it draws, or -1
};

// A sprite on the screen, for a 3D view to put a model in its place: its
// picture (descriptor) and box in the original's pixels.
struct SpriteObj
{
    uint16_t desc;
    float x, y, w, h;   // x, y the top left (the odd phase already added)
};

enum class Part : uint8_t
{
    Sprite, Text, Terrain, Rest
};

struct SceneStats
{
    int pictures{0}, designed{0}, glyphs{0}, terrainPoints{0}, restRuns{0}, restDots{0};
    int explosions{0}, playerPieces{0}, flash{0};
};

class Scene
{
public:
    // Hook into the board's program (Machine::onHook).
    void attach(Board& b);
    // The lines of the picture as it stands now.
    void build(const Board& b, std::vector<Line>& out, std::vector<Part>* parts = nullptr);
    const SceneStats& stats() const { return stats_; }
    // The designed sprites of the last build, in drawing order.
    const std::vector<SpriteObj>& objects() const { return objs_; }
    void clear() { items_.clear(); }

private:
    struct Item
    {
        uint16_t desc;     // picture descriptor
        uint16_t data;     // the pixels written (even or odd phase)
        int16_t x, y;      // where the data went: x of its first column's left pixel
        uint8_t w, h;      // in bytes (two pixels) and lines
        uint8_t phase;     // 1: drawn one pixel to the right (odd phase)
        bool text;         // a character of CHRTBL
        int8_t glyph;
    };

    static void hookThunk(void* user, Machine& m, uint16_t pc);
    void onHook(Machine& m, uint16_t pc);
    bool verify(const Board& b, const Item& it, bool mark);
    using Emit = std::function<void(float, float, float, float, int, Part)>;
    void flash(const Board& b, const Emit& emit);
    void penLogo(const Board& b, const Emit& emit);
    void explosions(const Board& b, const Emit& emit);
    void playerExplosion(const Board& b, const Emit& emit);
    void rest(const Board& b, const std::function<void(const std::vector<DPoint>&, int)>& out);
    static void simplify(const std::vector<DPoint>& in, std::vector<DPoint>& out, float tolerance = 0.6f);

    std::vector<Item> items_;
    std::vector<SpriteObj> objs_;
    int16_t curObj_{-1};
    std::vector<uint8_t> claimed_, left_;
    std::vector<std::vector<DPoint>> pen_;   // the Williams logo's pen strokes (LGOTAB), pixel centres
    bool penDecoded_{false};
    SceneStats stats_;
};

} // namespace df
