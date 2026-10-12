// This game's answers to the headset frontend's questions (gcn-recomp/runtime/vr_game.h):
// when to be in stereo, how big the world is, and what the eyes leave out. See
// docs/dev/vr.md.
//
// Stereo -- the world around the viewer, six degrees of freedom -- for a live race: the
// player's own on one screen (not the title's demo, not split screen), from the camera's
// drop onto the grid through the countdown to the finish, and not paused (race_live in
// race.h). The theater panel for everything else: the menus, the course flyover, the pause
// menu, the results. The frontend holds a change for two frames before following it.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "gx/render.h"
#include "race.h"
#include "vr_game.h"

namespace {

// The latest answer, for the render thread: decided on the guest thread from one
// consistent frame of the game's memory.
std::atomic<bool> g_stereo{false};

// Frames a race must have been live before stereo comes back after it was not. Leaving the
// pause menu for the menus resumes the race for a moment first, which would otherwise blink
// into stereo on the way out; resuming costs the same wait.
constexpr uint32_t kResumeFrames = 45;

// MKDD_VRLOG=1: each change of the stereo answer.
bool log() {
    static const bool on = getenv("MKDD_VRLOG") != nullptr;
    return on;
}

void guest_frame(uint32_t frame) {
    const mkdd::Race race = mkdd::race_read();
    const bool race_live = mkdd::race_live(race);
    static uint32_t live_since = 0;
    static bool was_live = false, was_paused = false;
    if (race_live && !was_live) live_since = frame;
    if (race.exists && race.paused) was_paused = true;
    if (!race.exists) was_paused = false;
    was_live = race_live;
    const bool live = race_live && (!was_paused || frame - live_since >= kResumeFrames);
    if (live) was_paused = false;
    if (log() && live != g_stereo.load(std::memory_order_relaxed))
        fprintf(stderr, "[vr] frame %u: %s (phase %d, run %d, mode %d, players %d, screens %d, demo %d, paused %d)\n", frame,
                live ? "stereo" : "theater", race.phase, race.run_state, race.mode, race.players, race.screens, race.demo,
                race.paused);
    g_stereo.store(live, std::memory_order_relaxed);
}

bool wants_stereo(const gx::Batch&) { return g_stereo.load(std::memory_order_relaxed); }

// The race's post-processing -- a fullscreen pass that draws a half-size copy of the frame
// back over it through a depth mask, and the like -- is orthographic, so an eye would hang
// it on the HUD frame as a picture of the flat view. The eyes leave out every orthographic
// draw that samples an EFB copy; the HUD's own elements sample textures.
int eye_filter(const gx::PixelState&, const gx::EyeDrawFacts& f) {
    return f.ortho && f.samples_copy ? gx::EyeHideDraw : gx::EyeKeep;
}

void config_defaults(VrConfig& c) {
    // A kart is about 200 units long, its wheelbase about 100 and its wheels about 65 across
    // (docs/dev/vr.md): about 100 units to the metre.
    c.units_per_metre = 100.0f;
    // The sky dome is drawn up to about 1.8 km out.
    c.far_m = 4000.0f;
    // The chase camera looks down at the kart by about 18 degrees; taken back out, the track
    // is level in the headset.
    c.world_pitch_deg = 18.0f;
}

const bool installed = [] {
    vr::GameHooks h;
    h.config_defaults = config_defaults;
    h.wants_stereo = wants_stereo;
    h.eye_filter = eye_filter;
    vr::set_game_hooks(h);
    gx::set_guest_frame_hook(guest_frame);
    return true;
}();

}  // namespace
