// The controls and what triggers them: every one freely bound.
//
// Each action follows the cabinet's advmame.rc (the VectorPie way) until it
// is bound otherwise in the service menu: up to two inputs each, any key,
// joystick button, joystick axis direction or hat direction. After the
// Vectortron port's bind.c.
#pragma once

#include <cstddef>
#include <cstdio>
#include <string>

typedef struct vp_input vp_input;

namespace vp {

enum Action
{
    ACT_UP, ACT_DOWN, ACT_THRUST, ACT_REVERSE, ACT_FIRE, ACT_SMART, ACT_HYPER,
    ACT_LEFT, ACT_RIGHT,                 // STICK STEERS only
    ACT_START1, ACT_START2,
    ACT_COIN1, ACT_COIN2, ACT_COIN3,     // the original's left, centre, right slot
    ACT_COUNT
};

constexpr int BIND_SLOTS = 2;

void bindInit();
// Once per step, after the SDK has read the controls.
void bindUpdate(vp_input* in);

bool actHeld(Action a);
bool actPressed(Action a);       // down since the last step

const char* actName(Action a);   // "THRUST"
// What a slot is bound to: "KEY SPACE", "JOY 1 BUTTON 3", "JOY 1 AXIS 2 +",
// "JOY 1 HAT UP", "-"; an unbound action shows "CABINET" in slot 0.
std::string bindDescribe(Action a, int slot);

// Binding from the menu: start waiting for an input for (a, slot), then call
// bindCapture() each step: 0 still waiting, 1 bound, -1 gave up (5 seconds).
// Inputs held when it starts count only once released and pressed again.
void bindCaptureBegin(Action a, int slot);
int bindCapture();
void bindClear(Action a);        // back to the cabinet's
void bindClearAll();

// Kept while the service menu is open, put back on CANCEL.
void bindSnapshot();
void bindRestore();

// The settings file: "bind_<action> = <input>[, <input>]".
bool bindParse(const std::string& key, const std::string& value);
void bindSave(std::FILE* f);

// The service menu's own keys: the bound stick and buttons, and always the
// arrow keys, Return, the first joystick's stick, hat and button 1, so no
// binding can lock the owner out. Up / down / left / right repeat while held.
struct MenuKeys
{
    bool up, down, left, right, ok;
};
void bindMenu(vp_input* in, MenuKeys& m);

} // namespace vp
