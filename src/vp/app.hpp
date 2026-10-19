// The cabinet around the game: the translated Defender (src/game) one
// picture per update, its picture as vectors (src/scene), the controls, the
// sound, the service menu (F2) and the original's own adjustment pages.
#pragma once

#include "settings.hpp"

typedef struct vp_game vp_game;
typedef struct vp_dvg vp_dvg;

namespace vp {

const Settings& appBootSettings();
int appInit(vp_game* g);
void appUpdate(vp_game* g, float dt);
void appDraw(vp_game* g, vp_dvg* d);
void appShutdown(vp_game* g);

} // namespace vp
