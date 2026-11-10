// save/settings.ini: see settings.hpp. After the Red Alarm port's.
#include "settings.hpp"

#include <SDL.h>
#include <vectorpie/vp.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace vp {

// The functions as the original's instruction card names them.
const char* const functionNames[FN_COUNT] = {
    "MOVE UP", "MOVE DOWN", "THRUST", "REVERSE", "FIRE", "SMART BOMB", "HYPERSPACE", "STICK LEFT", "STICK RIGHT"};

// The original's panel on the cabinet's: the stick up and down, button 1
// fire, 2 thrust, 3 smart bomb, 4 hyperspace, 5 reverse (AdvanceMAME's
// names, made the controls they stand for by vpb_normalize).
const char* const functionStandard[FN_COUNT] = {
    "p1_up", "p1_down", "p1_button2", "p1_button5", "p1_button1", "p1_button3", "p1_button4", "p1_left", "p1_right"};

namespace {

const char* const functionKeys[FN_COUNT] = {
    "up", "down", "thrust", "reverse", "fire", "smart_bomb", "hyperspace", "stick_left", "stick_right"};

std::string path()
{
    return std::string(vp_save_dir()) + "/settings.ini";
}

bool yes(const std::string& v)
{
    return v == "yes" || v == "1" || v == "on" || v == "true";
}

int clampi(int v, int lo, int hi)
{
    return std::max(lo, std::min(hi, v));
}

std::string trim(const std::string& t)
{
    const auto a = t.find_first_not_of(" \t\r");
    const auto b = t.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string{} : t.substr(a, b - a + 1);
}

// The first versions' bindings ("bind_fire = key:224, joy0:button3": an SDL
// scancode, joysticks and their parts from 0) as vpbind text.
std::string fromOldBinding(const std::string& v)
{
    std::string out;
    size_t from = 0;
    while(from <= v.size())
    {
        size_t to = v.find(',', from);
        if(to == std::string::npos) to = v.size();
        const std::string one = trim(v.substr(from, to - from));
        from = to + 1;
        int a = 0, b = 0;
        char way = 0, kind[16] = {};
        std::string as;
        if(std::sscanf(one.c_str(), "key:%d", &a) == 1)
        {
            as = SDL_GetScancodeName(static_cast<SDL_Scancode>(a));
            if(as.empty()) continue;
            for(char& c : as)
                if(c == ' ') c = '_';
            as = "key_" + as;
        }
        else if(std::sscanf(one.c_str(), "joy%d:button%d", &a, &b) == 2)
            as = "joy" + std::to_string(a + 1) + "_button" + std::to_string(b + 1);
        else if(std::sscanf(one.c_str(), "joy%d:axis%d%c", &a, &b, &way) == 3)
            as = "joy" + std::to_string(a + 1) + "_axis" + std::to_string(b + 1) + (way == '-' ? "_neg" : "_pos");
        else if(std::sscanf(one.c_str(), "joy%d:hat%d:%15[a-z]", &a, &b, kind) == 3)
            as = "joy" + std::to_string(a + 1) + "_hat" + std::to_string(b + 1) + "_" + kind;
        else continue;
        if(!out.empty()) out += "+";
        out += as;
    }
    return out;
}

} // namespace

void loadSettings(Settings& s)
{
    // the standard controls, until the file says otherwise
    for(int i = 0; i < FN_COUNT; ++i)
    {
        std::snprintf(s.controls[i], sizeof s.controls[i], "%s", functionStandard[i]);
        vpb_normalize(s.controls[i], sizeof s.controls[i]);
    }
    std::ifstream in(path());
    std::string line;
    while(std::getline(in, line))
    {
        const auto hash = line.find_first_of(";#");
        if(hash != std::string::npos) line.erase(hash);
        const auto eq = line.find('=');
        if(eq == std::string::npos) continue;
        const std::string k = trim(line.substr(0, eq));
        const std::string v = trim(line.substr(eq + 1));
        const int n = std::atoi(v.c_str());

        if(k == "volume") s.volume = clampi(n, 0, 100);
        else if(k == "stick_steers") s.stickSteer = yes(v) ? 1 : 0;   // the first versions' switch
        else if(k == "stick_steer") s.stickSteer = v == "turn_thrust" ? 1 : v == "turn" ? 2 : 0;
        else if(k == "view") s.view = v == "3d" ? 1 : v == "3d_dynamic" ? 2 : v == "3d_enemies" ? 3 : 0;
        else if(k == "bright_sprites") s.brightSprites = clampi(n, 10, 100);
        else if(k == "bright_terrain") s.brightTerrain = clampi(n, 10, 100);
        else if(k == "bright_text") s.brightText = clampi(n, 10, 100);
        else if(k == "bright_effects") s.brightEffects = clampi(n, 10, 100);
        else if(k == "screen_size") s.screenSize = clampi(n, 70, 100);
        else if(k == "frame_hz") s.frameHz = n >= 55 ? 60 : n >= 45 ? 50 : 40;
        else
            for(int i = 0; i < FN_COUNT; ++i)
            {
                std::string value = v;
                if(k == std::string("bind_") + functionKeys[i]) value = fromOldBinding(v);   // the first binder's
                else if(k != functionKeys[i]) continue;
                if(value.empty()) continue;
                std::snprintf(s.controls[i], sizeof s.controls[i], "%s", value.c_str());
                vpb_normalize(s.controls[i], sizeof s.controls[i]);
            }
    }
}

void saveSettings(const Settings& s)
{
    const std::string p = path();
    const std::string tmp = p + ".new";
    {
        std::ofstream out(tmp, std::ios::trunc);
        out << "; Defender: the service menu (F2) writes this file.\n";
        out << "volume = " << s.volume << '\n';
        out << "stick_steer = " << (s.stickSteer == 1 ? "turn_thrust" : s.stickSteer == 2 ? "turn" : "off") << '\n';
        out << "view = " << (s.view == 1 ? "3d" : s.view == 2 ? "3d_dynamic" : s.view == 3 ? "3d_enemies" : "classic") << '\n';
        out << "bright_sprites = " << s.brightSprites << '\n';
        out << "bright_terrain = " << s.brightTerrain << '\n';
        out << "bright_text = " << s.brightText << '\n';
        out << "bright_effects = " << s.brightEffects << '\n';
        out << "screen_size = " << s.screenSize << '\n';
        out << "frame_hz = " << s.frameHz << '\n';
        for(int i = 0; i < FN_COUNT; ++i) out << functionKeys[i] << " = " << s.controls[i] << '\n';
    }
    std::remove(p.c_str());
    std::rename(tmp.c_str(), p.c_str());
}

} // namespace vp
