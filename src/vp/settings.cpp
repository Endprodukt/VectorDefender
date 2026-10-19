// save/settings.ini: see settings.hpp. After the Red Alarm port's.
#include "settings.hpp"

#include "bind.hpp"

#include <vectorpie/vp.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace vp {

namespace {

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

} // namespace

void loadSettings(Settings& s)
{
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
        else if(k == "view") s.view = v == "3d" ? 1 : v == "3d_dynamic" ? 2 : 0;
        else if(k == "bright_sprites") s.brightSprites = clampi(n, 10, 100);
        else if(k == "bright_terrain") s.brightTerrain = clampi(n, 10, 100);
        else if(k == "bright_text") s.brightText = clampi(n, 10, 100);
        else if(k == "bright_effects") s.brightEffects = clampi(n, 10, 100);
        else if(k == "screen_size") s.screenSize = clampi(n, 70, 100);
        else if(k == "frame_hz") s.frameHz = n >= 55 ? 60 : n >= 45 ? 50 : 40;
        else bindParse(k, v);
    }
}

void saveSettings(const Settings& s)
{
    const std::string p = path();
    const std::string tmp = p + ".new";
    {
        std::ofstream out(tmp, std::ios::trunc);
        out << "; Vector Defender: the service menu (F2) writes this file.\n";
        out << "volume = " << s.volume << '\n';
        out << "stick_steer = " << (s.stickSteer == 1 ? "turn_thrust" : s.stickSteer == 2 ? "turn" : "off") << '\n';
        out << "view = " << (s.view == 1 ? "3d" : s.view == 2 ? "3d_dynamic" : "classic") << '\n';
        out << "bright_sprites = " << s.brightSprites << '\n';
        out << "bright_terrain = " << s.brightTerrain << '\n';
        out << "bright_text = " << s.brightText << '\n';
        out << "bright_effects = " << s.brightEffects << '\n';
        out << "screen_size = " << s.screenSize << '\n';
        out << "frame_hz = " << s.frameHz << '\n';
    }
    // the bindings after the rest (bind.cpp writes them)
    if(std::FILE* f = std::fopen(tmp.c_str(), "a"))
    {
        bindSave(f);
        std::fclose(f);
    }
    std::remove(p.c_str());
    std::rename(tmp.c_str(), p.c_str());
}

} // namespace vp
