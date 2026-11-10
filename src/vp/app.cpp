// The cabinet around the game: see app.hpp.
//
// Coins, credits, attract mode, high scores and their initials are the
// original's own, as are its settings (coinage, ships, bonus, free play):
// those live in its CMOS, save/cmos.bin, and are changed as plain options
// of the service menu (GAME SETTINGS, adjust.cpp).
#include "app.hpp"

#include "adjust.hpp"
#include "controls.hpp"
#include "render.hpp"
#include "service.hpp"
#include "view3d.hpp"

#include "game/board.hpp"
#include "gen/defender_syms.hpp"
#include "scene/scene.hpp"

#include <vectorpie/vp.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace vp {

namespace {

constexpr int FIRST_BOOT_FRAMES = 60 * 60;   // at most, to write the factory settings
constexpr int CMOS_SAVE_EVERY = 60;           // updates between CMOS checks
constexpr int AUDIO_RATE = 44100;
constexpr int AUDIO_PER_UPDATE = AUDIO_RATE / 60;   // the board's 60.1 pictures play in 60 updates
constexpr int AUDIO_MAX_QUEUE = AUDIO_RATE / 8;    // more waiting than this is dropped
constexpr int FIELD_HOLD = 30;                     // updates the 3D view stays after the field has gone

struct App
{
    Settings set;
    bool settingsLoaded{false};
    df::Board board;
    df::Scene scene;
    Controls controls;
    Renderer renderer;
    View3D view3d;
    std::vector<df::Line> lines;
    std::vector<df::Part> parts;
    vp_audio* audio{nullptr};
    vp_voice pcm{0};
    std::vector<float> stereo;

    bool service{false};
    int field{0};              // updates left of showing the playfield (3D view)
    unsigned ticks{0};
} A;

std::string cmosPath()
{
    return std::string(vp_save_dir()) + "/cmos.bin";
}

bool fileExists(const std::string& p)
{
    if(FILE* f = std::fopen(p.c_str(), "rb"))
    {
        std::fclose(f);
        return true;
    }
    return false;
}

// The check byte the game writes once its CMOS holds valid settings (CMOSCK).
bool cmosValid(const df::Board& b)
{
    return (((b.m.cmos[0x7F] & 0x0F) << 4) | (b.m.cmos[0x80] & 0x0F)) == 0x5A;
}

// Power on: the original's own tests (unseen, see below), then the game. On
// the very first start (no save/cmos.bin) the game finds its CMOS empty,
// writes the factory settings and shows its audit page, as a new board
// does; the machine is then switched off and on again once, here, quickly.
void powerOn()
{
    const std::string path = cmosPath();
    const bool known = fileExists(path);
    A.scene.clear();
    A.board.powerOn(path);
    if(!known || !cmosValid(A.board))
    {
        for(int i = 0; i < FIRST_BOOT_FRAMES && !cmosValid(A.board) && !A.board.m.broken; ++i)
            A.board.frame(df::Inputs{});
        A.board.m.cmosDirty = true;
        A.board.saveCmos();
        A.scene.clear();
        A.board.powerOn(path);
    }
    // Straight into the game (the owner's wish): the original's power-on
    // tests run as ever, only unseen and at once, until the game starts
    // (SINIT); the first picture is its attract mode.
    for(int i = 0; i < FIRST_BOOT_FRAMES && !A.board.m.started && !A.board.m.broken; ++i)
        A.board.frame(df::Inputs{});
    A.scene.clear();
    A.controls.reset();
}

// The sound board's sound of this picture to the cabinet.
void pushSound()
{
    if(!A.audio) return;
    const std::vector<float>& s = A.board.audio;
    const float vol = static_cast<float>(A.set.volume) / 100.f;
    A.stereo.resize(s.size() * 2);
    for(size_t i = 0; i < s.size(); ++i) A.stereo[2 * i] = A.stereo[2 * i + 1] = s[i] * vol;
    if(!A.pcm)
    {
        // the game's sound effects: on the cabinet at the menu's SOUND VOLUME
        A.pcm = vp_stream_play(A.audio, AUDIO_RATE, 1.f);
        vp_stream_set_effects(A.audio, A.pcm);
    }
    if(vp_stream_queued(A.audio, A.pcm) > AUDIO_MAX_QUEUE) return;
    vp_stream_write(A.audio, A.pcm, A.stereo.data(), static_cast<int>(s.size()));
}

void applySettings()
{
    A.renderer.setup(A.set);
}

// The START lamps: lit while there are credits, as the menu's games do.
void lamps()
{
    const uint8_t bcd = A.board.m.peek(df::sym::CREDIT);
    const int credits = (bcd >> 4) * 10 + (bcd & 0x0F);
    vp_lamps_set(credits >= 2 ? VP_LAMP_START1 | VP_LAMP_START2 : credits >= 1 ? VP_LAMP_START1 : 0);
}

} // namespace

const Settings& appBootSettings()
{
    if(!A.settingsLoaded)
    {
        loadSettings(A.set);
        A.settingsLoaded = true;
    }
    return A.set;
}

int appInit(vp_game* g)
{
    A.audio = vp_game_audio(g);
    appBootSettings();
    ui::R = &A.renderer;
    applySettings();
    A.scene.attach(A.board);
    powerOn();
    return VP_OK;
}

void appUpdate(vp_game* g, float)
{
    vp_input* in = vp_game_input(g);
    const bool hotkey = serviceHotkey();
    ++A.ticks;

    if(A.service)
    {
        const int r = serviceUpdate(in, hotkey);
        if(r == SERVICE_SAVED)
        {
            A.set = serviceGet();
            saveSettings(A.set);
            applySettings();
            // the game's own settings into its CMOS; a reset of the high
            // scores needs the machine switched off and on
            const bool restart = writeAdjust(A.board, serviceGame());
            A.board.saveCmos();
            if(restart) powerOn();
        }
        if(r != SERVICE_OPEN) A.service = false;
        return;
    }
    if(hotkey)
    {
        serviceOpen(A.set, readAdjust(A.board));
        A.service = true;
        vp_lamps_set(0);
        return;
    }
    if(A.board.m.broken) return;

    const df::Inputs inputs = A.controls.read(in, A.set, A.board);
    A.board.frame(inputs, AUDIO_PER_UPDATE);
    pushSound();
    A.scene.build(A.board, A.lines, &A.parts);
    lamps();

    // The 3D view is for the playfield, in a game and in the attract
    // mode's demo alike: while the terrain or the ship is on the screen
    // (and a moment after, so a blink does not switch it). Titles, the hall
    // of fame and the other pages of text stay as the original drew them.
    bool fieldShown = A.scene.stats().terrainPoints > 0;
    for(const df::SpriteObj& o : A.scene.objects())
        if(o.desc == df::sym::PLAPIC || o.desc == df::sym::PLBPIC) fieldShown = true;
    if(fieldShown) A.field = FIELD_HOLD;
    else if(A.field > 0) --A.field;
    if(A.ticks % CMOS_SAVE_EVERY == 0) A.board.saveCmos();

    // VD_TRACE=1: once a second, what the scene holds; and every change of
    // the game's switches.
    static const bool trace = std::getenv("VD_TRACE") != nullptr;
    static df::Inputs last;
    if(trace && (inputs.in0 != last.in0 || inputs.in1 != last.in1 || inputs.in2 != last.in2))
        std::fprintf(stderr, "trace t=%u switches in0=%02X in1=%02X in2=%02X\n", A.ticks, inputs.in0, inputs.in1, inputs.in2);
    last = inputs;
    if(trace && A.ticks % 60 == 0)
    {
        const df::SceneStats& s = A.scene.stats();
        std::fprintf(stderr, "trace t=%u lines=%zu pictures=%d designed=%d letters=%d terrain=%d rest=%d/%d\n",
            A.ticks / 60, A.lines.size(), s.pictures, s.designed, s.glyphs, s.terrainPoints, s.restRuns, s.restDots);
    }
}

void appDraw(vp_game*, vp_dvg* d)
{
    if(A.service)
    {
        serviceDraw(d);
        return;
    }
    if(A.board.m.broken)
    {
        char line[64];
        std::snprintf(line, sizeof line, "PROGRAM STOPPED AT %04X", A.board.m.badPc);
        ui::text(d, "DEFENDER", 192.f, 90.f, 10.f, VP_TEXT_CENTER, Rgb{255, 0, 0});
        ui::text(d, line, 192.f, 120.f, 5.f, VP_TEXT_CENTER, Rgb{255, 255, 255});
        return;
    }
    if(A.set.view && A.field > 0) A.view3d.draw(d, A.board, A.lines, A.parts, A.scene.objects(), A.set, 1.f / 60.f);
    else A.renderer.scene(d, A.lines, A.parts);
}

void appShutdown(vp_game*)
{
    A.board.saveCmos();
    vp_lamps_set(0);
    if(A.audio && A.pcm) vp_voice_stop(A.audio, A.pcm, 0.f);
}

} // namespace vp
