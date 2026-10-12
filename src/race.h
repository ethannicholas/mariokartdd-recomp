// What this game's own memory says about the race, for the headset hooks (vr_mkdd.cpp).
// See docs/dev/vr.md for how each value was found.
#pragma once
#include <cstdint>

namespace mkdd {

// RaceDirector's phase. Only the values seen so far are named.
enum RacePhase : int32_t {
    kCourseDemo = 1,  // the course flyover before a race
    kSafety = 16,
    kRunning = 17,    // from the camera's drop onto the grid, through the countdown, to the finish
    kWait = 18,
    kEnding = 19,     // after the finish: the winning run, the results
};

// RaceInfo's mode.
enum RaceMode : int32_t {
    kTimeAttack = 1, kGrandPrix = 2, kVersus = 3, kBalloonBattle = 4, kRobberyBattle = 5,
    kBombBattle = 6, kEscapeBattle = 7, kAwardDemo = 8, kStaffRoll = 9,
};

struct Race {
    bool exists = false;   // a race scene is up (RaceMgr::sRaceManager is set)
    int32_t phase = 0;
    int32_t run_state = 0; // within kRunning: 0 the grid, 1 the countdown, 2 racing
    int32_t mode = 0;
    int players = 0;       // human players
    int screens = 0;       // the split-screen count
    bool demo = false;     // the title's attract-mode race
    bool paused = false;   // the pause menu
};

Race race_read();

// How far the first player's camera looks down, in radians: atan2 of the world's up in the
// camera's frame (KartCam's view matrix). False when there is no camera.
bool camera_pitch(float* out);

// The first player's kart in their camera's view space: the kart body's world matrix
// (KartBody+0x110; its columns are the kart's left, up and forward, the fourth its position)
// through the camera's view matrix. GX 3x4, by rows. False when there is no camera.
bool kart_in_view(float out[3][4]);

// A race the player drives on one screen, from the grid to the finish, and not paused.
inline bool race_live(const Race& r) {
    return r.exists && !r.demo && r.players >= 1 && r.screens == 1 && r.mode >= kTimeAttack &&
           r.mode <= kEscapeBattle && r.phase == kRunning && !r.paused;
}

}  // namespace mkdd
