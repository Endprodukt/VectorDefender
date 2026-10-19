// Drawing on the board. The scene (src/scene) is in the original's pixels;
// its visible part (MAME's: x 6..297, y 7..246) fills the 4:3 tube as the
// original's monitor did. The service menu and messages use a 384 x 224
// layout (as the owner's other ports), mapped the same way.
#pragma once

#include "scene/scene.hpp"
#include "settings.hpp"

#include <cstdint>
#include <vector>

typedef struct vp_dvg vp_dvg;

namespace vp {

struct Rgb
{
    uint8_t r, g, b;
};

class Renderer
{
public:
    void setup(const Settings& s) { set_ = s; }
    // The scene's lines, joined where one ends where the next begins, in
    // their order.
    void scene(vp_dvg* d, const std::vector<df::Line>& lines, const std::vector<df::Part>& parts);

    float bx(float x) const;   // board x of an original pixel x
    float by(float y) const;
    float ux(float x) const;   // board x of a 384 x 224 layout x
    float uy(float y) const;

private:
    Settings set_;
};

namespace ui {

extern Renderer* R;

void line(vp_dvg* d, float x0, float y0, float x1, float y1);
// In the 384 x 224 layout: `y` the baseline, `h` a capital's height.
void text(vp_dvg* d, const char* s, float x, float y, float h, int align, Rgb c);
// Centred at x; longer than maxWidth: two rows, the second on y.
void textWrapped(vp_dvg* d, const char* s, float x, float y, float h, float maxWidth, Rgb c);
Rgb scale(Rgb c, int percent);

} // namespace ui

} // namespace vp
