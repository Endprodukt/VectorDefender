// The controls and what triggers them: see bind.hpp. Inputs are read
// straight from SDL (keys, joystick buttons, axes, hats).
#include "bind.hpp"

#include <SDL.h>
#include <vectorpie/vp.h>

#include <cstring>

namespace vp {

namespace {

enum InType { IN_NONE, IN_KEY, IN_BUTTON, IN_AXIS, IN_HAT };

struct Input
{
    InType type{IN_NONE};
    int joy{0};     // joystick number, 0 = the first
    int code{0};    // scancode, button, axis or hat number
    int dir{0};     // axis: -1 / +1; hat: SDL_HAT_* bit
};

// the cabinet's advmame.rc names for each action, tried in turn
const char* const CABINET[ACT_COUNT][3] = {
    {"p1_up", "p1_doubleleft_up", nullptr},
    {"p1_down", "p1_doubleleft_down", nullptr},
    {"p1_button2", nullptr, nullptr},
    {"p1_button5", nullptr, nullptr},
    {"p1_button1", nullptr, nullptr},
    {"p1_button3", nullptr, nullptr},
    {"p1_button4", nullptr, nullptr},
    {"p1_left", "p1_doubleleft_left", nullptr},
    {"p1_right", "p1_doubleleft_right", nullptr},
    {"start1", nullptr, nullptr},
    {"start2", nullptr, nullptr},
    {"coin1", nullptr, nullptr},
    {"coin2", nullptr, nullptr},
    {"coin3", nullptr, nullptr},
};

// analog sticks the cabinet names: past half travel counts as the direction
struct CabinetAxis
{
    Action a;
    const char* axis;
    int dir;
};
const CabinetAxis CABINET_AXES[] = {
    {ACT_UP, "p1_sticky", -1}, {ACT_DOWN, "p1_sticky", 1},
    {ACT_LEFT, "p1_stickx", -1}, {ACT_RIGHT, "p1_stickx", 1},
};

const char* const NAMES[ACT_COUNT] = {
    "MOVE UP", "MOVE DOWN", "THRUST", "REVERSE", "FIRE", "SMART BOMB", "HYPERSPACE",
    "STICK LEFT", "STICK RIGHT", "START 1", "START 2", "COIN LEFT", "COIN CENTER", "COIN RIGHT"};

const char* const KEYS[ACT_COUNT] = {
    "bind_up", "bind_down", "bind_thrust", "bind_reverse", "bind_fire", "bind_smart_bomb", "bind_hyperspace",
    "bind_stick_left", "bind_stick_right", "bind_start1", "bind_start2", "bind_coin1", "bind_coin2", "bind_coin3"};

constexpr int MAX_JOYS = 8;

Input B[ACT_COUNT][BIND_SLOTS];
Input saved[ACT_COUNT][BIND_SLOTS];
bool heldNow[ACT_COUNT], heldBefore[ACT_COUNT];
SDL_Joystick* J[MAX_JOYS];
int njoys = -1;

struct Capture
{
    bool active{false};
    int slot{0};
    Action a{ACT_UP};
    Uint32 start{0};
    unsigned char keys[SDL_NUM_SCANCODES];
    unsigned char buttons[MAX_JOYS][32];
    signed char axes[MAX_JOYS][16];
    unsigned char hats[MAX_JOYS][4];
} CAP;

void openJoysticks()
{
    const int n = SDL_NumJoysticks();
    if(n == njoys) return;
    for(SDL_Joystick*& j : J)
        if(j) { SDL_JoystickClose(j); j = nullptr; }
    for(int i = 0; i < n && i < MAX_JOYS; ++i) J[i] = SDL_JoystickOpen(i);
    njoys = n;
}

bool active(const Input& in)
{
    switch(in.type)
    {
        case IN_KEY:
        {
            const Uint8* k = SDL_GetKeyboardState(nullptr);
            return in.code > 0 && in.code < SDL_NUM_SCANCODES && k[in.code];
        }
        case IN_BUTTON: return in.joy < MAX_JOYS && J[in.joy] && SDL_JoystickGetButton(J[in.joy], in.code);
        case IN_AXIS:
        {
            if(in.joy >= MAX_JOYS || !J[in.joy]) return false;
            const int v = SDL_JoystickGetAxis(J[in.joy], in.code);
            return in.dir < 0 ? v < -16000 : v > 16000;
        }
        case IN_HAT: return in.joy < MAX_JOYS && J[in.joy] && (SDL_JoystickGetHat(J[in.joy], in.code) & in.dir);
        default: return false;
    }
}

bool bound(Action a)
{
    return B[a][0].type != IN_NONE || B[a][1].type != IN_NONE;
}

std::string describe(const Input& in)
{
    char buf[48];
    switch(in.type)
    {
        case IN_KEY:
        {
            const char* n = SDL_GetScancodeName(static_cast<SDL_Scancode>(in.code));
            std::snprintf(buf, sizeof buf, "KEY %s", n && *n ? n : "?");
            for(char* p = buf; *p; ++p)
                if(*p >= 'a' && *p <= 'z') *p = static_cast<char>(*p - 32);
            break;
        }
        case IN_BUTTON: std::snprintf(buf, sizeof buf, "JOY %d BUTTON %d", in.joy + 1, in.code + 1); break;
        case IN_AXIS: std::snprintf(buf, sizeof buf, "JOY %d AXIS %d %c", in.joy + 1, in.code + 1, in.dir < 0 ? '-' : '+'); break;
        case IN_HAT:
        {
            const char* d = in.dir == SDL_HAT_UP ? "UP" : in.dir == SDL_HAT_DOWN ? "DOWN" : in.dir == SDL_HAT_LEFT ? "LEFT" : "RIGHT";
            std::snprintf(buf, sizeof buf, "JOY %d HAT %s", in.joy + 1, d);
            break;
        }
        default: std::snprintf(buf, sizeof buf, "-"); break;
    }
    return buf;
}

void snapshotInputs()
{
    int nk = 0;
    const Uint8* k = SDL_GetKeyboardState(&nk);
    std::memset(CAP.keys, 0, sizeof CAP.keys);
    for(int i = 0; i < nk && i < SDL_NUM_SCANCODES; ++i) CAP.keys[i] = k[i];
    std::memset(CAP.buttons, 0, sizeof CAP.buttons);
    std::memset(CAP.axes, 0, sizeof CAP.axes);
    std::memset(CAP.hats, 0, sizeof CAP.hats);
    for(int j = 0; j < MAX_JOYS; ++j)
    {
        if(!J[j]) continue;
        for(int b = 0; b < SDL_JoystickNumButtons(J[j]) && b < 32; ++b) CAP.buttons[j][b] = SDL_JoystickGetButton(J[j], b);
        for(int x = 0; x < SDL_JoystickNumAxes(J[j]) && x < 16; ++x)
        {
            const int v = SDL_JoystickGetAxis(J[j], x);
            CAP.axes[j][x] = static_cast<signed char>(v < -16000 ? -1 : v > 16000 ? 1 : 0);
        }
        for(int h = 0; h < SDL_JoystickNumHats(J[j]) && h < 4; ++h) CAP.hats[j][h] = SDL_JoystickGetHat(J[j], h);
    }
}

bool parseInput(const char* s, Input& in)
{
    char kind[16];
    int a = 0, b = 0;
    char d = 0;
    in = Input{};
    if(std::sscanf(s, " key:%d", &a) == 1) { in.type = IN_KEY; in.code = a; return true; }
    if(std::sscanf(s, " joy%d:button%d", &a, &b) == 2) { in.type = IN_BUTTON; in.joy = a; in.code = b; return true; }
    if(std::sscanf(s, " joy%d:axis%d%c", &a, &b, &d) == 3)
    {
        in.type = IN_AXIS; in.joy = a; in.code = b; in.dir = d == '-' ? -1 : 1;
        return true;
    }
    if(std::sscanf(s, " joy%d:hat%d:%15[a-z]", &a, &b, kind) == 3)
    {
        in.type = IN_HAT; in.joy = a; in.code = b;
        in.dir = !std::strcmp(kind, "up") ? SDL_HAT_UP : !std::strcmp(kind, "down") ? SDL_HAT_DOWN
               : !std::strcmp(kind, "left") ? SDL_HAT_LEFT : SDL_HAT_RIGHT;
        return true;
    }
    return false;
}

std::string formatInput(const Input& in)
{
    char buf[48];
    switch(in.type)
    {
        case IN_KEY: std::snprintf(buf, sizeof buf, "key:%d", in.code); break;
        case IN_BUTTON: std::snprintf(buf, sizeof buf, "joy%d:button%d", in.joy, in.code); break;
        case IN_AXIS: std::snprintf(buf, sizeof buf, "joy%d:axis%d%c", in.joy, in.code, in.dir < 0 ? '-' : '+'); break;
        case IN_HAT:
            std::snprintf(buf, sizeof buf, "joy%d:hat%d:%s", in.joy, in.code,
                          in.dir == SDL_HAT_UP ? "up" : in.dir == SDL_HAT_DOWN ? "down" : in.dir == SDL_HAT_LEFT ? "left" : "right");
            break;
        default: std::snprintf(buf, sizeof buf, "none"); break;
    }
    return buf;
}

bool rawKey(int sc)
{
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    return k && k[sc];
}

bool joy0Dir(int axis, int sign, int hatbit)
{
    if(!J[0]) return false;
    const int v = SDL_JoystickNumAxes(J[0]) > axis ? SDL_JoystickGetAxis(J[0], axis) : 0;
    if(sign < 0 ? v < -16000 : v > 16000) return true;
    return SDL_JoystickNumHats(J[0]) > 0 && (SDL_JoystickGetHat(J[0], 0) & hatbit);
}

bool joy0Button(int b)
{
    return J[0] && SDL_JoystickNumButtons(J[0]) > b && SDL_JoystickGetButton(J[0], b);
}

} // namespace

void bindInit()
{
    openJoysticks();
}

void bindUpdate(vp_input* in)
{
    openJoysticks();
    for(int a = 0; a < ACT_COUNT; ++a)
    {
        heldBefore[a] = heldNow[a];
        bool h = false;
        if(bound(static_cast<Action>(a)))
        {
            for(const Input& s : B[a]) h = h || active(s);
        }
        else
        {
            for(int k = 0; k < 3 && CABINET[a][k]; ++k) h = h || vp_input_held(in, CABINET[a][k]);
            for(const CabinetAxis& ax : CABINET_AXES)
            {
                if(ax.a != a) continue;
                const float v = vp_input_axis(in, ax.axis);
                if(ax.dir < 0 ? v < -0.5f : v > 0.5f) h = true;
            }
        }
        heldNow[a] = h;
    }
}

bool actHeld(Action a) { return a < ACT_COUNT && heldNow[a]; }
bool actPressed(Action a) { return a < ACT_COUNT && heldNow[a] && !heldBefore[a]; }
const char* actName(Action a) { return a < ACT_COUNT ? NAMES[a] : "?"; }

std::string bindDescribe(Action a, int slot)
{
    if(!bound(a)) return slot == 0 ? "CABINET" : "-";
    return describe(B[a][slot]);
}

void bindCaptureBegin(Action a, int slot)
{
    CAP.active = true;
    CAP.a = a;
    CAP.slot = slot;
    CAP.start = SDL_GetTicks();
    snapshotInputs();
}

int bindCapture()
{
    if(!CAP.active) return -1;
    if(SDL_GetTicks() - CAP.start > 5000) { CAP.active = false; return -1; }
    Input got;
    int nk = 0;
    const Uint8* k = SDL_GetKeyboardState(&nk);
    for(int i = 1; i < nk && i < SDL_NUM_SCANCODES && got.type == IN_NONE; ++i)
    {
        // Esc is the cabinet's exit, F2 the service menu: never bound
        if(i == SDL_SCANCODE_ESCAPE || i == SDL_SCANCODE_F2) continue;
        if(k[i] && !CAP.keys[i]) { got.type = IN_KEY; got.code = i; }
        if(!k[i]) CAP.keys[i] = 0;   // released: may be taken next time
    }
    for(int j = 0; j < MAX_JOYS && got.type == IN_NONE; ++j)
    {
        if(!J[j]) continue;
        for(int b = 0; b < SDL_JoystickNumButtons(J[j]) && b < 32 && got.type == IN_NONE; ++b)
        {
            const int v = SDL_JoystickGetButton(J[j], b);
            if(v && !CAP.buttons[j][b]) { got.type = IN_BUTTON; got.joy = j; got.code = b; }
            if(!v) CAP.buttons[j][b] = 0;
        }
        for(int x = 0; x < SDL_JoystickNumAxes(J[j]) && x < 16 && got.type == IN_NONE; ++x)
        {
            const int v = SDL_JoystickGetAxis(J[j], x);
            const int d = v < -20000 ? -1 : v > 20000 ? 1 : 0;
            if(d && d != CAP.axes[j][x]) { got.type = IN_AXIS; got.joy = j; got.code = x; got.dir = d; }
            if(v > -8000 && v < 8000) CAP.axes[j][x] = 0;
        }
        for(int h = 0; h < SDL_JoystickNumHats(J[j]) && h < 4 && got.type == IN_NONE; ++h)
        {
            const int v = SDL_JoystickGetHat(J[j], h);
            static const int DIRS[4] = {SDL_HAT_UP, SDL_HAT_DOWN, SDL_HAT_LEFT, SDL_HAT_RIGHT};
            for(int q = 0; q < 4 && got.type == IN_NONE; ++q)
                if((v & DIRS[q]) && !(CAP.hats[j][h] & DIRS[q])) { got.type = IN_HAT; got.joy = j; got.code = h; got.dir = DIRS[q]; }
            CAP.hats[j][h] = static_cast<unsigned char>(CAP.hats[j][h] & v);
        }
    }
    if(got.type == IN_NONE) return 0;
    if(!bound(CAP.a)) for(Input& s : B[CAP.a]) s = Input{};
    B[CAP.a][CAP.slot] = got;
    CAP.active = false;
    // the input just bound must not also count as a press this step
    heldNow[CAP.a] = heldBefore[CAP.a] = true;
    return 1;
}

void bindClear(Action a)
{
    for(Input& s : B[a]) s = Input{};
}

void bindClearAll()
{
    for(auto& a : B)
        for(Input& s : a) s = Input{};
}

void bindSnapshot()
{
    std::memcpy(saved, B, sizeof B);
}

void bindRestore()
{
    std::memcpy(B, saved, sizeof B);
}

bool bindParse(const std::string& key, const std::string& value)
{
    for(int a = 0; a < ACT_COUNT; ++a)
    {
        if(key != KEYS[a]) continue;
        for(Input& s : B[a]) s = Input{};
        int slot = 0;
        size_t from = 0;
        while(from <= value.size() && slot < BIND_SLOTS)
        {
            size_t to = value.find(',', from);
            if(to == std::string::npos) to = value.size();
            if(parseInput(value.substr(from, to - from).c_str(), B[a][slot])) ++slot;
            from = to + 1;
        }
        return true;
    }
    return false;
}

void bindSave(std::FILE* f)
{
    for(int a = 0; a < ACT_COUNT; ++a)
    {
        if(!bound(static_cast<Action>(a))) continue;   // the cabinet's: nothing to write
        const std::string s0 = formatInput(B[a][0]), s1 = formatInput(B[a][1]);
        if(B[a][1].type != IN_NONE) std::fprintf(f, "%s = %s, %s\n", KEYS[a], s0.c_str(), s1.c_str());
        else std::fprintf(f, "%s = %s\n", KEYS[a], s0.c_str());
    }
}

void bindMenu(vp_input* in, MenuKeys& m)
{
    static bool prev[5];
    static int hold[4];
    bool now[5];
    now[0] = actHeld(ACT_UP) || vp_input_held(in, "p1_up") || rawKey(SDL_SCANCODE_UP) || joy0Dir(1, -1, SDL_HAT_UP);
    now[1] = actHeld(ACT_DOWN) || vp_input_held(in, "p1_down") || rawKey(SDL_SCANCODE_DOWN) || joy0Dir(1, 1, SDL_HAT_DOWN);
    now[2] = actHeld(ACT_LEFT) || vp_input_held(in, "p1_left") || rawKey(SDL_SCANCODE_LEFT) || joy0Dir(0, -1, SDL_HAT_LEFT);
    now[3] = actHeld(ACT_RIGHT) || vp_input_held(in, "p1_right") || rawKey(SDL_SCANCODE_RIGHT) || joy0Dir(0, 1, SDL_HAT_RIGHT);
    now[4] = actHeld(ACT_FIRE) || actHeld(ACT_START1) || vp_input_held(in, "start1") || vp_input_held(in, "p1_button1") ||
             rawKey(SDL_SCANCODE_RETURN) || rawKey(SDL_SCANCODE_KP_ENTER) || joy0Button(0);
    bool* out[5] = {&m.up, &m.down, &m.left, &m.right, &m.ok};
    for(int i = 0; i < 5; ++i)
    {
        bool fire = now[i] && !prev[i];
        if(i < 4)
        {
            // held: repeat after 0.4 s, then 8 times a second
            hold[i] = now[i] ? hold[i] + 1 : 0;
            if(hold[i] > 24 && (hold[i] - 24) % 7 == 0) fire = true;
        }
        *out[i] = fire;
        prev[i] = now[i];
    }
}

} // namespace vp
