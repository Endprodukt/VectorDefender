// Vector Defender for VectorPie: Williams' Defender (Red Label), its
// program translated from the original 6809 source, its picture drawn as
// colour vectors.
//
// Controls, by their advmame.rc names (changed in the service menu, F2):
//   p1 stick up / down        move up / down
//   p1_button1                fire
//   p1_button2                thrust
//   p1_button3                smart bomb
//   p1_button4                hyperspace
//   p1_button5                reverse
//   coin1 / coin2             coins (the original's left and centre slots)
//   start1 / start2           one or two players
//
// The original runs 60.1 pictures a second; the board is drawn at 60 Hz.
#include "app.hpp"

#include <vectorpie/vp.h>

#include <cstdlib>
#include <cstring>

namespace {

void update(vp_game* g, float dt) { vp::appUpdate(g, dt); }
void draw(vp_game* g, vp_dvg* d) { vp::appDraw(g, d); }
int init(vp_game* g) { return vp::appInit(g); }
void shutdown(vp_game* g) { vp::appShutdown(g); }

} // namespace

int main(int argc, char** argv)
{
    // The VectorPie menu starts a game as <name>_dvg or <name>_sdl.
    if(!std::getenv("VP_OUTPUT") && argc > 0)
    {
        const std::size_t n = std::strlen(argv[0]);
        const char* output = n >= 4 && !std::strcmp(argv[0] + n - 4, "_sdl")   ? "sdl"
                             : n >= 4 && !std::strcmp(argv[0] + n - 4, "_dvg") ? "dvg"
                                                                               : nullptr;
#ifdef _WIN32
        if(output) _putenv_s("VP_OUTPUT", output);
#else
        if(output) setenv("VP_OUTPUT", output, 1);
#endif
    }

    const vp::Settings& s = vp::appBootSettings();

    vp_game_desc desc{};
    desc.title = "Defender";
    desc.init = init;
    desc.update = update;
    desc.draw = draw;
    desc.shutdown = shutdown;
    desc.update_hz = 60;
    desc.frame_hz = 60;
    desc.frame_hz_board = s.frameHz;
    return vp_run(&desc, argc, argv);
}
