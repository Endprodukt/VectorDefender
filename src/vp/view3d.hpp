// The 3D view (VIDEO, VIEW: 3D or 3D DYNAMIC): the same game, the same
// scene, but the ship and the enemies as 3D models with hidden lines (the
// SDK's vp3d), the terrain as a band with depth, seen by a camera a little
// off the straight view; in 3D DYNAMIC the camera turns the way the ship
// flies, the more the faster, as if to see further ahead. Positions, timing
// and everything else stay the original's: only the drawing changes. The
// scanner and the score row stay flat on top. Used while the playfield is
// on the screen (a game, the attract mode's demo), not for titles and
// pages of text (app.cpp).
#pragma once

#include "game/board.hpp"
#include "scene/scene.hpp"
#include "settings.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

typedef struct vp_dvg vp_dvg;
typedef struct vp3d_mesh vp3d_mesh;

namespace vp {

class View3D
{
public:
    View3D() = default;
    View3D(const View3D&) = delete;
    View3D& operator=(const View3D&) = delete;
    ~View3D();

    // Builds the models (once).
    void init();
    // One picture. `dt`: seconds since the last (for the camera and spins).
    void draw(vp_dvg* d, const df::Board& b, const std::vector<df::Line>& lines,
              const std::vector<df::Part>& parts, const std::vector<df::SpriteObj>& objs,
              const Settings& s, float dt);

    struct Part3
    {
        vp3d_mesh* mesh;
        uint8_t color;       // the original's colour index
        bool wire;           // drawn see-through (lines, plates)
    };
    struct Model
    {
        std::vector<Part3> parts;
        float spinY{0}, spinX{0};   // turns a second (enemies)
    };

private:
    std::vector<vp3d_mesh*> meshes_;
    std::unordered_map<uint16_t, Model> models_;
    bool ready_{false};
    float time_{0};
    // the ship: its heading turns round when it reverses, it banks when it climbs
    float shipYaw_{0}, shipRoll_{0}, shipLastY_{-1};
    bool shipSeen_{false};
    // 3D DYNAMIC: the camera's turn and tilt, following the flight
    float camYaw_{0}, camPitch_{0.2f};
    uint16_t lastBgl_{0};
    bool haveBgl_{false};
};

} // namespace vp
