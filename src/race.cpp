// Reading the race out of the game's memory. See race.h, and docs/dev/vr.md for how each
// address was found.
#include "race.h"

#include "recomp.h"
#include "runtime.h"

namespace mkdd {
namespace {

constexpr uint32_t kRaceManager = 0x803CB7E8;  // RaceMgr::sRaceManager
constexpr uint32_t kPaused = 0x803CB6DC;       // PauseManager::mIsPause

// RaceMgr
constexpr uint32_t kDirector = 0x18;
constexpr uint32_t kInfo = 0x38;
// RaceDirector
constexpr uint32_t kPhase = 0x0C;
constexpr uint32_t kRunState = 0x30;
// RaceInfo
constexpr uint32_t kMode = 0x08;
constexpr uint32_t kPlayers = 0x1E;
constexpr uint32_t kScreens = 0x20;
constexpr uint32_t kDemoType = 0x28;

bool in_ram(uint32_t a, uint32_t size) { return a >= 0x80000000u && a + size <= 0x80000000u + RAM_SIZE && !(a & 3); }

}  // namespace

Race race_read() {
    Race r;
    const uint32_t mgr = mem_r32(kRaceManager);
    if (!in_ram(mgr, 0x100)) return r;
    const uint32_t dir = mem_r32(mgr + kDirector), info = mem_r32(mgr + kInfo);
    if (!in_ram(dir, 0x48) || !in_ram(info, 0x2A0)) return r;
    r.exists = true;
    r.phase = (int32_t)mem_r32(dir + kPhase);
    r.run_state = (int32_t)mem_r32(dir + kRunState);
    r.mode = (int32_t)mem_r32(info + kMode);
    r.players = (int16_t)mem_r16(info + kPlayers);
    r.screens = (int16_t)mem_r16(info + kScreens);
    r.demo = mem_r32(info + kDemoType) != 0;
    r.paused = mem_r8(kPaused) != 0;
    return r;
}

}  // namespace mkdd
