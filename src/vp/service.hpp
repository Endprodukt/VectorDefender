// The service menu: save/settings.ini, changed on the machine. Opened and
// cancelled with F2. Laid out and worked as the owner's other ports'
// (Tempest 2000, Open Hexagon, Red Alarm): up / down go through the rows,
// START 1 or button 1 takes a row, left / right set a value. A control row
// waits for the cabinet control to lay on it. Nothing is taken over until
// SAVE. GAME SETTINGS holds the original's own settings (its CMOS,
// adjust.hpp) as plain options.
#pragma once

#include "adjust.hpp"
#include "settings.hpp"

typedef struct vp_input vp_input;
typedef struct vp_dvg vp_dvg;

namespace vp {

enum
{
    SERVICE_OPEN = 0,
    SERVICE_SAVED,
    SERVICE_CANCELLED
};

// Once every step, open or not: true when F2 has just been pressed.
bool serviceHotkey();

void serviceOpen(const Settings& now, const GameAdjust& game);
int serviceUpdate(vp_input* in, bool hotkey);
const Settings& serviceGet();
const GameAdjust& serviceGame();
void serviceDraw(vp_dvg* d);

} // namespace vp
