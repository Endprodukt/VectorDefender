// The service menu: see service.hpp. The structure is the Red Alarm port's
// service.cpp (after the Open Hexagon and Tempest 2000 ports'); the free
// bindings and their page after the Vectortron port's.
#include "service.hpp"

#include "bind.hpp"
#include "render.hpp"

#include <SDL.h>
#include <vectorpie/vp.h>

#include <cstdio>
#include <string>

namespace vp {

namespace {

enum Row
{
    R_CONTROLS, R_VIDEO, R_GAME, R_VOLUME, R_STEER, R_SAVE, R_CANCEL,
    R_ACT, R_ACT_LAST = R_ACT + ACT_COUNT - 1, R_RESETALL,
    R_VIEW, R_BSPRITES, R_BTERRAIN, R_BTEXT, R_BEFFECTS, R_SIZE, R_HZ,
    R_BONUS, R_SHIPS, R_COINAGE, R_FREE, R_DIFF, R_CEIL, R_RESTORE, R_AUDITS,
    R_RESETSCORES, R_RESETAUDITS, R_FACTORY,
    R_BACK, ROWS
};

enum { PAGE_MAIN, PAGE_CONTROLS, PAGE_VIDEO, PAGE_GAME, PAGE_AUDITS };

const int main_rows[] = {R_CONTROLS, R_VIDEO, R_GAME, R_VOLUME, R_CANCEL, R_SAVE};
const int video_rows[] = {R_VIEW, R_BSPRITES, R_BTERRAIN, R_BTEXT, R_BEFFECTS, R_SIZE, R_HZ, R_BACK};
const int game_rows[] = {R_BONUS, R_SHIPS, R_COINAGE, R_FREE, R_DIFF, R_CEIL, R_RESTORE, R_AUDITS,
    R_RESETSCORES, R_RESETAUDITS, R_FACTORY, R_BACK};
const int audit_rows[] = {R_BACK};
int control_rows[ACT_COUNT + 3];

template <typename T, int N>
constexpr int count(const T (&)[N])
{
    return N;
}

const char* row_name(int row)
{
    static const char* const names[] = {
        "CONTROLS", "VIDEO", "GAME SETTINGS", "VOLUME", "STICK STEERING", "SAVE", "CANCEL"};
    if(row < R_ACT) return names[row];
    if(row <= R_ACT_LAST) return actName(static_cast<Action>(row - R_ACT));
    switch(row)
    {
        case R_RESETALL: return "RESET ALL TO THE CABINET";
        case R_VIEW: return "VIEW";
        case R_BSPRITES: return "SPRITE BRIGHTNESS";
        case R_BTERRAIN: return "TERRAIN BRIGHTNESS";
        case R_BTEXT: return "TEXT BRIGHTNESS";
        case R_BEFFECTS: return "EFFECT BRIGHTNESS";
        case R_SIZE: return "SCREEN SIZE";
        case R_HZ: return "FRAME RATE";
        case R_BONUS: return "BONUS SHIP EVERY";
        case R_SHIPS: return "SHIPS PER GAME";
        case R_COINAGE: return "COINAGE";
        case R_FREE: return "FREE PLAY";
        case R_DIFF: return "DIFFICULTY";
        case R_CEIL: return "DIFFICULTY CEILING";
        case R_RESTORE: return "PLANET BACK EVERY";
        case R_AUDITS: return "BOOKKEEPING";
        case R_RESETSCORES: return "RESET HIGH SCORES";
        case R_RESETAUDITS: return "RESET BOOKKEEPING";
        case R_FACTORY: return "FACTORY SETTINGS";
    }
    return "BACK";
}

// What a row says about itself at the foot of the page.
const char* row_hint(int row)
{
    switch(row)
    {
        case R_STEER: return "STICK LEFT / RIGHT FLIES THE SHIP THAT WAY. THE REVERSE BUTTON STILL WORKS";
        case R_VIEW: return "3D SHOWS THE SHIP AND THE ENEMIES AS MODELS WHILE A GAME IS ON. THE ATTRACT MODE STAYS CLASSIC";
        case R_BONUS: return "0 MEANS NO BONUS SHIPS";
        case R_COINAGE: return "THE ORIGINAL'S COINAGE PRESETS. AS SET KEEPS THE BOARD'S OWN COIN VALUES";
        case R_DIFF: return "THE DIFFICULTY THE GAME STARTS WITH (GAME ADJUST 1)";
        case R_CEIL: return "THE MOST THE DIFFICULTY GROWS TO (GAME ADJUST 2)";
        case R_RESTORE: return "THE PLANET AND ITS HUMANOIDS COME BACK EVERY THIS MANY WAVES (GAME ADJUST 4)";
        case R_RESETSCORES: return "THE HALL OF FAME BACK TO THE ORIGINAL'S, WHEN YOU SAVE";
        case R_RESETAUDITS: return "ALL BOOKKEEPING COUNTERS TO 0, WHEN YOU SAVE";
        case R_FACTORY: return "ALL GAME SETTINGS AND THE HIGH SCORES AS THE ORIGINAL LEFT THE FACTORY, WHEN YOU SAVE";
    }
    return nullptr;
}

struct State
{
    Settings now;
    GameAdjust game;
    int page{PAGE_MAIN}, cursor{0}, row{R_CONTROLS};
    int col{0};                // controls: 0 input 1, 1 input 2, 2 clear
    bool capturing{false};
    std::string note;
    int noteTime{0};
    bool key{false};
    unsigned frames{0};
} S;

const int* page_rows(int* n)
{
    if(S.page == PAGE_CONTROLS) { *n = count(control_rows); return control_rows; }
    if(S.page == PAGE_VIDEO) { *n = count(video_rows); return video_rows; }
    if(S.page == PAGE_GAME) { *n = count(game_rows); return game_rows; }
    if(S.page == PAGE_AUDITS) { *n = count(audit_rows); return audit_rows; }
    *n = count(main_rows);
    return main_rows;
}

// A number row: its value, limits and step.
int* row_number(int row, int* lo, int* hi, int* step)
{
    *lo = 10;
    *hi = 100;
    *step = 5;
    switch(row)
    {
        case R_VOLUME: *lo = 0; return &S.now.volume;
        case R_BSPRITES: return &S.now.brightSprites;
        case R_BTERRAIN: return &S.now.brightTerrain;
        case R_BTEXT: return &S.now.brightText;
        case R_BEFFECTS: return &S.now.brightEffects;
        case R_SIZE: *lo = 70; return &S.now.screenSize;
        case R_BONUS: *lo = 0; *hi = 99000; *step = 1000; return &S.game.bonusShip;
        case R_SHIPS: *lo = 1; *hi = 9; *step = 1; return &S.game.ships;
        case R_DIFF: *lo = 0; *hi = 99; *step = 1; return &S.game.difficulty;
        case R_CEIL: *lo = 0; *hi = 99; *step = 1; return &S.game.ceiling;
        case R_RESTORE: *lo = 0; *hi = 99; *step = 1; return &S.game.restoreWave;
    }
    return nullptr;
}

bool* row_switch(int row)
{
    switch(row)
    {
        case R_FREE: return &S.game.freePlay;
        case R_RESETSCORES: return &S.game.resetScores;
        case R_RESETAUDITS: return &S.game.resetAudits;
        case R_FACTORY: return &S.game.factory;
    }
    return nullptr;
}

bool is_action(int row)
{
    return row >= R_ACT && row <= R_ACT_LAST;
}

void change(int row, int steps)
{
    int lo, hi, step;
    if(!steps) return;
    if(bool* b = row_switch(row)) { *b = !*b; return; }
    if(row == R_STEER) { S.now.stickSteer = (S.now.stickSteer + (steps > 0 ? 1 : 2)) % 3; return; }
    if(row == R_VIEW) { S.now.view = (S.now.view + (steps > 0 ? 1 : 2)) % 3; return; }
    if(row == R_COINAGE) { S.game.coinage = (S.game.coinage + (steps > 0 ? 1 : 8)) % 9; return; }
    if(row == R_HZ)
    {
        static const int rates[] = {40, 50, 60};
        int i = S.now.frameHz >= 60 ? 2 : S.now.frameHz >= 50 ? 1 : 0;
        i = (i + (steps > 0 ? 1 : 2)) % 3;
        S.now.frameHz = rates[i];
        return;
    }
    int* number = row_number(row, &lo, &hi, &step);
    if(!number) return;
    *number += steps * step;
    if(*number < lo) *number = lo;
    if(*number > hi) *number = hi;
}

void turn_to(int page)
{
    int n;
    S.page = page;
    S.cursor = 0;
    S.col = 0;
    S.row = page_rows(&n)[0];
}

void note(const char* text)
{
    S.note = text;
    S.noteTime = 120;
}

const Rgb YELLOW{255, 255, 0};
const Rgb WHITE{255, 255, 255};
const Rgb BLUE{60, 110, 255};
const Rgb GREEN{0, 200, 0};
const Rgb RED{220, 60, 60};

const char* page_title()
{
    switch(S.page)
    {
        case PAGE_CONTROLS: return "CONTROLS";
        case PAGE_VIDEO: return "VIDEO";
        case PAGE_GAME: return "GAME SETTINGS";
        case PAGE_AUDITS: return "BOOKKEEPING";
    }
    return "SERVICE MENU";
}

} // namespace

bool serviceHotkey()
{
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const bool key = keys && keys[SDL_SCANCODE_F2];
    const bool hit = key && !S.key;
    S.key = key;
    return hit;
}

void serviceOpen(const Settings& now, const GameAdjust& game)
{
    const bool key = S.key;
    S = State{};
    S.key = key;
    S.now = now;
    S.game = game;
    S.row = main_rows[0];
    control_rows[0] = R_STEER;
    for(int i = 0; i < ACT_COUNT; ++i) control_rows[i + 1] = R_ACT + i;
    control_rows[ACT_COUNT + 1] = R_RESETALL;
    control_rows[ACT_COUNT + 2] = R_BACK;
    bindSnapshot();
}

const Settings& serviceGet()
{
    return S.now;
}

const GameAdjust& serviceGame()
{
    return S.game;
}

int serviceUpdate(vp_input* in, bool hotkey)
{
    int n;
    const int* rows = page_rows(&n);

    ++S.frames;
    if(S.noteTime > 0) --S.noteTime;
    if(hotkey)
    {
        bindRestore();
        return SERVICE_CANCELLED;
    }
    if(S.capturing)
    {
        const int r = bindCapture();
        if(r != 0) S.capturing = false;
        if(r < 0) note("NOTHING PRESSED - LEFT AS IT WAS");
        return SERVICE_OPEN;
    }

    MenuKeys k{};
    bindMenu(in, k);
    if(S.page == PAGE_CONTROLS && is_action(S.row))
    {
        if(k.left && S.col > 0) --S.col;
        if(k.right && S.col < 2) ++S.col;
    }
    else if(k.left || k.right) change(S.row, k.right ? 1 : -1);

    if(k.down && S.cursor < n - 1) ++S.cursor;
    if(k.up && S.cursor > 0) --S.cursor;
    S.row = rows[S.cursor];
    if(!k.ok) return SERVICE_OPEN;

    if(S.row == R_SAVE) return SERVICE_SAVED;
    if(S.row == R_CANCEL)
    {
        bindRestore();
        return SERVICE_CANCELLED;
    }
    if(S.row == R_CONTROLS) turn_to(PAGE_CONTROLS);
    else if(S.row == R_VIDEO) turn_to(PAGE_VIDEO);
    else if(S.row == R_GAME) turn_to(PAGE_GAME);
    else if(S.row == R_AUDITS) turn_to(PAGE_AUDITS);
    else if(S.row == R_BACK) turn_to(S.page == PAGE_AUDITS ? PAGE_GAME : PAGE_MAIN);
    else if(S.row == R_RESETALL)
    {
        bindClearAll();
        note("ALL CONTROLS AS THE CABINET'S");
    }
    else if(is_action(S.row))
    {
        const Action a = static_cast<Action>(S.row - R_ACT);
        if(S.col == 2)
        {
            bindClear(a);
            note("BACK TO THE CABINET'S");
        }
        else
        {
            bindCaptureBegin(a, S.col);
            S.capturing = true;
        }
    }
    else change(S.row, 1);
    return SERVICE_OPEN;
}

void serviceDraw(vp_dvg* d)
{
    int n;
    const int* rows = page_rows(&n);
    char value[64];

    ui::text(d, page_title(), 192.f, 14.f, 9.f, VP_TEXT_CENTER, YELLOW);

    if(S.page == PAGE_CONTROLS)
    {
        static const char* const COLS[3] = {"INPUT 1", "INPUT 2", "CLEAR"};
        static const float CX[3] = {150.f, 248.f, 340.f};
        for(int c = 0; c < 3; ++c) ui::text(d, COLS[c], CX[c], 28.f, 4.f, VP_TEXT_LEFT, GREEN);
        const float step = 160.f / static_cast<float>(n);
        for(int i = 0; i < n; ++i)
        {
            const int row = rows[i];
            const bool here = row == S.row;
            const float y = 40.f + step * static_cast<float>(i);
            const float h = 4.2f;
            if(here) ui::text(d, ">", 14.f, y, h, VP_TEXT_RIGHT, YELLOW);
            ui::text(d, row_name(row), 18.f, y, h, VP_TEXT_LEFT, here ? WHITE : BLUE);
            if(row == R_STEER)
            {
                static const char* const steer[3] = {"OFF", "TURN THAT WAY AND THRUST", "TURN THAT WAY"};
                ui::text(d, steer[S.now.stickSteer], CX[0], y, h * 0.85f, VP_TEXT_LEFT, here ? WHITE : BLUE);
            }
            if(!is_action(row)) continue;
            const Action a = static_cast<Action>(row - R_ACT);
            for(int c = 0; c < 3; ++c)
            {
                const bool on = here && S.col == c;
                Rgb col = on ? WHITE : BLUE;
                std::string t = c == 2 ? (on ? "CLEAR" : "") : bindDescribe(a, c);
                if(on && S.capturing)
                {
                    col = YELLOW;
                    t = (S.frames / 15) & 1 ? "PRESS A CONTROL" : "";
                }
                if(!t.empty()) ui::text(d, t.c_str(), CX[c], y, h * 0.85f, VP_TEXT_LEFT, col);
            }
        }
    }
    else if(S.page == PAGE_AUDITS)
    {
        for(int k = 0; k < 7; ++k)
        {
            const float y = 40.f + 16.f * static_cast<float>(k);
            ui::text(d, auditNames[k], 60.f, y, 5.f, VP_TEXT_LEFT, BLUE);
            std::snprintf(value, sizeof value, k == 5 ? "%d MIN" : "%d", S.game.audits[k]);
            ui::text(d, value, 300.f, y, 5.f, VP_TEXT_RIGHT, WHITE);
        }
        ui::text(d, "> BACK", 192.f, 168.f, 5.f, VP_TEXT_CENTER, WHITE);
    }
    else
    {
        const float rowStep = n > 10 ? 150.f / static_cast<float>(n) : 15.f;
        const float h = n > 10 ? 4.5f : 5.f;
        for(int i = 0; i < n; ++i)
        {
            const int row = rows[i];
            const bool here = row == S.row;
            const float y = 36.f + rowStep * static_cast<float>(i);
            Rgb c = here ? WHITE : BLUE;
            int lo, hi, step;
            int* number = row_number(row, &lo, &hi, &step);
            bool* onoff = row_switch(row);

            if(row == R_SAVE || row == R_CANCEL) continue;   // side by side, below
            if(here) ui::text(d, ">", 22.f, y, h, VP_TEXT_RIGHT, YELLOW);
            ui::text(d, row_name(row), 28.f, y, h, VP_TEXT_LEFT, c);

            const float vx = 236.f;
            const char* v = nullptr;
            if(row == R_CONTROLS || row == R_VIDEO || row == R_GAME || row == R_AUDITS) v = "OPEN";
            else if(row == R_VIEW)
            {
                static const char* const views[3] = {"CLASSIC", "3D", "3D DYNAMIC"};
                v = views[S.now.view];
            }
            else if(row == R_COINAGE) v = coinageNames[S.game.coinage];
            else if(row == R_HZ) { std::snprintf(value, sizeof value, "%d HZ", S.now.frameHz); v = value; }
            else if(row == R_BONUS)
            {
                if(S.game.bonusShip) std::snprintf(value, sizeof value, "%d", S.game.bonusShip);
                v = S.game.bonusShip ? value : "NEVER";
            }
            else if(row == R_RESTORE) { std::snprintf(value, sizeof value, "%d WAVES", S.game.restoreWave); v = value; }
            else if(row == R_RESETSCORES || row == R_RESETAUDITS || row == R_FACTORY) v = *onoff ? "ON SAVE" : "NO";
            else if(number && step == 5) { std::snprintf(value, sizeof value, "%d%%", *number); v = value; }
            else if(number) { std::snprintf(value, sizeof value, "%d", *number); v = value; }
            else if(onoff) v = *onoff ? "YES" : "NO";
            if(v) ui::text(d, v, vx, y, h, VP_TEXT_LEFT, c);
        }
        if(S.page == PAGE_MAIN)
        {
            ui::text(d, "CANCEL", 140.f, 196.f, S.row == R_CANCEL ? 8.f : 6.f, VP_TEXT_CENTER, S.row == R_CANCEL ? WHITE : RED);
            ui::text(d, "SAVE", 244.f, 196.f, S.row == R_SAVE ? 8.f : 6.f, VP_TEXT_CENTER, S.row == R_SAVE ? WHITE : GREEN);
        }
    }

    // the foot: a note, the capture, the row's own words, or the keys
    const char* foot = nullptr;
    Rgb footColor = GREEN;
    if(S.noteTime > 0) { foot = S.note.c_str(); footColor = YELLOW; }
    else if(S.capturing) foot = "ANY KEY, BUTTON OR STICK - 5 SECONDS";
    else if(const char* h = row_hint(S.row)) foot = h;
    else if(S.page == PAGE_CONTROLS) foot = "LEFT / RIGHT: INPUT 1, INPUT 2, CLEAR. F2 - LEAVE WITHOUT SAVING";
    else foot = "F2 - LEAVE WITHOUT SAVING";
    ui::textWrapped(d, foot, 192.f, 222.f, 4.f, 330.f, footColor);
}

} // namespace vp
