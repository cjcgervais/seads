#pragma once
// ★★★ L3 — THE SPAWN MENU
// (docs/PLAN_20260901_game_loop_millwright.md §4.3, Chad's ruling R1).
//
// ★★★ THIS IS THE ONE NON-DIEGETIC SCREEN IN THE GAME, AND IT IS RULED, NOT
// SLIPPED IN. `WINTER_LAW.md` §1 says "never a menu" and `app/main.cpp` carries
// the line "do not grow a menu on it"; both are about IN-WORLD mode switching,
// which is what teleporting a rider between bodies would be. A spawn is not in
// the world -- a respawn is already a non-diegetic event, the player is not
// anywhere yet -- and Chad ruled the menu explicitly on 2026-09-01: "So a menu
// for spawn of vehicle selection for the first time needs to be built."
// Mounting, dismounting, repairing and manning the gun all stay diegetic: a key
// AT A POSITION (app/interact.h). Nothing else may join this screen.
//
// ⚠ THE ONLY FILE IN app/ THAT TOUCHES raylib BESIDES main.cpp. `seads_tests`
// bans raylib across its whole include closure (tools/graph/layer_rules.toml),
// so NOTHING testable may live here: the policy, the gate and the geometry are
// all in `app/spawn_policy.h`, which this file does not even need. What is left
// here is a panel, two rectangles and a keypress -- and the rectangles are
// computed by ONE function that both the drawing and the hit test call, because
// a menu whose click target is laid out twice is a menu that becomes
// unclickable at some window size nobody tested.

#include <raylib.h>

namespace app {

// The two options, in the order they are drawn and in the order the number keys
// select them. The int IS the row index -- `PlayerSpawnChoice` is the policy's
// enum and lives in the policy header; keeping the menu's own row index
// separate is what stops this file from needing to include it.
constexpr int kSpawnMenuAircraft = 0;
constexpr int kSpawnMenuSnowmachine = 1;
constexpr int kSpawnMenuRows = 2;

// ★★★ THE SIDE STAGE (Chad 2026-09-10: "we need to allow player to choose
// 'central city' or 'valley' spawn for the teams").
//
// The menu now asks TWO questions on the first birth of a session -- WHICH SIDE
// first, then WHICH RIDE -- and ONE question at every respawn after it. It is
// the same panel, the same two rectangles and the same number keys: a second
// screen would be a second non-diegetic surface, and the banner above says
// there is exactly one. `stage` is which question is on screen.
//
// ⚠ THE SIDE IS ASKED ONCE PER SESSION, NOT ONCE PER LIFE. You do not change
// armies by dying. app/main.cpp opens the respawn menu straight on the ride
// stage, so the choice persists exactly as long as the process does.
//
// The row ints are deliberately the FACTION ints (render/team_kit.h
// PlayerTeam, combat::CqFaction): VALLEY = 0 is the shipped default and is
// drawn first, so "press 1" on the first frame is the behaviour that shipped.
constexpr int kSpawnMenuTeamValley = 0;
constexpr int kSpawnMenuTeamCentralCity = 1;
constexpr int kSpawnStageTeam = 0;
constexpr int kSpawnStageRide = 1;

struct SpawnMenuState {
    // Which question is on screen: kSpawnStageTeam or kSpawnStageRide. The
    // DEFAULT IS THE RIDE STAGE so every existing opener (the respawn gate at
    // app/main.cpp's death seam) keeps asking exactly the one question it
    // always asked; only the first-spawn arming raises the side stage.
    int stage = kSpawnStageRide;
    // The answer, kept for the session. Fed to render::set_player_team() and
    // to combat::ConquestState::player_faction the frame it is given.
    int team = kSpawnMenuTeamValley;
    // The game is PAUSED while this is true (app/main.cpp feeds step_frame a
    // zero dt and skips the interact keys) -- "the game is paused under it",
    // plan §4.3.
    bool open = false;
    // The first birth of the session says one thing; a respawn says another.
    bool first_spawn = true;
    // Greyed out when the world cannot place a machine (no conquest, no own
    // surface pump). The row is still DRAWN -- a silently missing option reads
    // as a broken menu -- and pressing 2 does nothing.
    bool machine_offered = true;
    // The row under the mouse, -1 for none. Cosmetic.
    int hover = -1;
};

// ★ ONE LAYOUT, TWO READERS. The draw and the hit test call this; neither one
// computes a rectangle of its own.
inline void spawn_menu_rects(int sw, int sh, Rectangle out[kSpawnMenuRows]) {
    const float w = static_cast<float>(sw);
    const float h = static_cast<float>(sh);
    const float bw = w < 720.0f ? w * 0.86f : 620.0f;
    const float bh = 96.0f;
    const float x = (w - bw) * 0.5f;
    const float y0 = h * 0.5f - bh - 12.0f;
    for (int i = 0; i < kSpawnMenuRows; ++i)
        out[i] =
            Rectangle{x, y0 + static_cast<float>(i) * (bh + 18.0f), bw, bh};
}

// Poll the choice. Returns the selected row, or -1 for "still waiting".
// Number keys 1/2 or a left click in the row's rectangle -- the plan's exact
// pair, and the number keys work whether or not the cursor is showing.
//
// ⚠ THE UNAVAILABLE ROW REFUSES BOTH INPUTS. One gate, checked once, so the key
// and the mouse cannot disagree about what is on offer.
inline int spawn_menu_poll(SpawnMenuState& m) {
    if (!m.open) return -1;
    Rectangle r[kSpawnMenuRows];
    spawn_menu_rects(GetScreenWidth(), GetScreenHeight(), r);
    const Vector2 mp = GetMousePosition();
    m.hover = -1;
    for (int i = 0; i < kSpawnMenuRows; ++i)
        if (CheckCollisionPointRec(mp, r[i])) m.hover = i;
    // Both sides are ALWAYS on offer; only the ride stage can have a dead row.
    const bool offered[kSpawnMenuRows] = {
        true, m.stage == kSpawnStageTeam ? true : m.machine_offered};
    int want = -1;
    if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1))
        want = 0;
    else if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2))
        want = 1;
    else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m.hover >= 0)
        want = m.hover;
    if (want < 0 || !offered[want]) return -1;
    return want;
}

// Draw it. Called from inside the frame's draw pass (render::FrameInfo::overlay
// -- the app's own last overlay, so the panel lands over the finished world and
// under nothing).
inline void spawn_menu_draw(const SpawnMenuState& m) {
    if (!m.open) return;
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    // The world goes dim, not black: you should be able to see WHERE you are
    // being born while you choose.
    DrawRectangle(0, 0, sw, sh, Color{6, 8, 12, 190});
    Rectangle r[kSpawnMenuRows];
    spawn_menu_rects(sw, sh, r);
    const bool team_stage = m.stage == kSpawnStageTeam;
    const char* title = team_stage ? "CHOOSE YOUR SIDE"
                                   : (m.first_spawn ? "CHOOSE YOUR RIDE"
                                                    : "RESPAWN AS");
    const int title_px = 34;
    DrawText(title, (sw - MeasureText(title, title_px)) / 2,
             static_cast<int>(r[0].y) - 78, title_px,
             Color{236, 240, 244, 240});
    const char* sub =
        team_stage
            ? "1 / 2  or click -- this is for the whole session"
            : (m.first_spawn
                   ? "1 / 2  or click"
                   : (m.machine_offered ? "a pump of yours is down -- go fix it"
                                        : "1 / 2  or click"));
    DrawText(sub, (sw - MeasureText(sub, 18)) / 2,
             static_cast<int>(r[0].y) - 36, 18, Color{170, 186, 200, 220});

    const char* ride_label[kSpawnMenuRows] = {"1   AIRCRAFT",
                                              "2   SNOWMACHINE"};
    const char* ride_note[kSpawnMenuRows] = {
        "airborne over the field, at speed",
        "on the snow, a kilometre from the pump"};
    const char* team_label[kSpawnMenuRows] = {"1   VALLEY",
                                              "2   CENTRAL CITY"};
    // The notes NAME the colours, because the colours are the coding: a player
    // who picks a side has to know what his own trail looks like from behind.
    const char* team_note[kSpawnMenuRows] = {
        "blue plane, blue scarf, orange smoke, orange helmet",
        "slag-orange plane, orange scarf, blue smoke, blue helmet"};
    const char** label = team_stage ? team_label : ride_label;
    const char** note = team_stage ? team_note : ride_note;
    const bool offered[kSpawnMenuRows] = {
        true, team_stage ? true : m.machine_offered};
    for (int i = 0; i < kSpawnMenuRows; ++i) {
        const bool live = offered[i];
        const bool hot = live && m.hover == i;
        const Color fill =
            hot ? Color{38, 62, 84, 230} : Color{18, 24, 32, 220};
        const Color edge =
            live ? (hot ? Color{150, 205, 240, 245} : Color{110, 130, 148, 200})
                 : Color{70, 76, 84, 160};
        const Color text =
            live ? Color{240, 244, 248, 245} : Color{120, 126, 134, 190};
        DrawRectangleRec(r[i], fill);
        DrawRectangleLinesEx(r[i], hot ? 3.0f : 2.0f, edge);
        DrawText(label[i], static_cast<int>(r[i].x) + 26,
                 static_cast<int>(r[i].y) + 20, 30, text);
        if (live) {
            DrawText(note[i], static_cast<int>(r[i].x) + 28,
                     static_cast<int>(r[i].y) + 58, 18,
                     Color{168, 184, 198, 225});
        } else {
            // Chad 2026-09-04: the dead row has to SAY how it comes alive --
            // "only available by landing the plane or by respawn after a
            // friendly pump is damaged". Two 15 px lines in the 96 px row.
            const Color dim{104, 110, 118, 180};
            DrawText("only by landing the aircraft and pressing J,",
                     static_cast<int>(r[i].x) + 28,
                     static_cast<int>(r[i].y) + 54, 15, dim);
            DrawText("or at a respawn while a pump of yours is damaged",
                     static_cast<int>(r[i].x) + 28,
                     static_cast<int>(r[i].y) + 72, 15, dim);
        }
    }
}

}  // namespace app
