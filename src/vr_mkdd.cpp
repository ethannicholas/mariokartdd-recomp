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
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <cstdio>
#include <cstdlib>

#include "gx/render.h"
#include "gx/render_gl.h"
#include "race.h"
#include "vr_game.h"

namespace {

// The latest answer, for the render thread: decided on the guest thread from one
// consistent frame of the game's memory.
std::atomic<bool> g_stereo{false};

// The chase camera's pitch, read on the guest thread each live frame, and whether the world
// follows it (follow_camera_pitch in vr.txt, on by default) rather than world_pitch_deg.
std::atomic<float> g_camera_pitch{0.0f};
std::atomic<bool> g_have_pitch{false};
bool g_follow_pitch = true;

// The player's kart in the camera's view space as of the newest guest frame (kart_in_view),
// for the first-person eye: the eye finds the kart's own draws in each batch by it.
std::mutex g_kart_mutex;
float g_kart[3][4];
bool g_have_kart = false;

// Where the eye sits in first person, in the kart body model's frame (game units: x the
// kart's left, y up, z forward), and how near that point a draw must be to be left out of
// the eyes -- the driver's head and shoulders. vr.txt: fp_x, fp_y, fp_z, fp_hide.
float g_fp[3] = {0.0f, 75.0f, 20.0f};
float g_fp_hide = 45.0f;

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
    float pitch;
    if (live && mkdd::camera_pitch(&pitch)) {
        g_camera_pitch.store(pitch, std::memory_order_relaxed);
        g_have_pitch.store(true, std::memory_order_relaxed);
    }
    float kart[3][4];
    const bool have_kart = live && mkdd::kart_in_view(kart);
    std::lock_guard<std::mutex> lk(g_kart_mutex);
    g_have_kart = have_kart;
    if (have_kart) memcpy(g_kart, kart, sizeof(kart));
}

// First person: the game has no cockpit camera, so the eye is put in the driver's seat.
// The kart's body model is found in the batch being drawn -- the position matrix whose
// rotation is the kart's, as read from memory for the newest guest frame, and whose
// origin is nearest the kart's -- so the eye moves with exactly the kart that frame shows,
// however far behind the guest the render thread is. The eye looks along the kart's nose
// and tilts with it; the head's own tracking is on top of that.
bool eye_hook(const gx::Batch& b, const std::vector<uint8_t>& offscreen, uint32_t, gx::EyeOverride& out) {
    float k[3][4];
    {
        std::lock_guard<std::mutex> lk(g_kart_mutex);
        if (!g_have_kart) return false;
        memcpy(k, g_kart, sizeof(k));
    }
    const size_t n = b.mtxs.size() / 12;
    int best = -1;
    float best_d = 1e30f;
    for (size_t m = 0; m < n; m++) {
        const float* q = &b.mtxs[m * 12];
        float rot = 0.0f;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) rot += fabsf(q[i * 4 + j] - k[i][j]);
        if (rot > 0.2f) continue;
        const float dx = q[3] - k[0][3], dy = q[7] - k[1][3], dz = q[11] - k[2][3];
        const float d = dx * dx + dy * dy + dz * dz;
        if (d < best_d && d < 300.0f * 300.0f) {
            best_d = d;
            best = (int)m;
        }
    }
    if (best < 0) return false;
    const float* q = &b.mtxs[best * 12];
    // The eye's axes in the camera's view space: right is the kart's -x, up its y, back its -z.
    float r[3], u[3], bk[3], p[3];
    for (int i = 0; i < 3; i++) {
        r[i] = -q[i * 4 + 0];
        u[i] = q[i * 4 + 1];
        bk[i] = -q[i * 4 + 2];
        p[i] = q[i * 4 + 3] + q[i * 4 + 0] * g_fp[0] + q[i * 4 + 1] * g_fp[1] + q[i * 4 + 2] * g_fp[2];
    }
    // view_to_eye is the inverse of the eye's pose: rows are its axes, then -axes.p.
    float* v = out.view_to_eye;
    memset(v, 0, 16 * sizeof(float));
    for (int i = 0; i < 3; i++) {
        v[i * 4 + 0] = r[i];
        v[i * 4 + 1] = u[i];
        v[i * 4 + 2] = bk[i];
    }
    v[12] = -(r[0] * p[0] + r[1] * p[1] + r[2] * p[2]);
    v[13] = -(u[0] * p[0] + u[1] * p[1] + u[2] * p[2]);
    v[14] = -(bk[0] * p[0] + bk[1] * p[1] + bk[2] * p[2]);
    v[15] = 1.0f;
    memset(out.hud_to_eye, 0, sizeof(out.hud_to_eye));
    out.hud_to_eye[0] = out.hud_to_eye[5] = out.hud_to_eye[10] = out.hud_to_eye[15] = 1.0f;
    // The driver's head and shoulders: every draw whose position matrix's origin is near the eye.
    out.hide.assign(b.cmds.size(), 0);
    for (size_t i = 0; i < b.cmds.size(); i++) {
        const gx::Cmd& c = b.cmds[i];
        if (c.type != gx::CmdType::Draw || offscreen[i] || (size_t)c.mtx >= n) continue;
        const float* m = &b.mtxs[c.mtx * 12];
        const float dx = m[3] - p[0], dy = m[7] - p[1], dz = m[11] - p[2];
        if (dx * dx + dy * dy + dz * dz < g_fp_hide * g_fp_hide) out.hide[i] = 1;
    }
    return true;
}

void set_first_person(bool on, const VrConfig&) { gx::render_set_first_person(on); }

bool wants_stereo(const gx::Batch&) { return g_stereo.load(std::memory_order_relaxed); }

// The renderer asks this on its own thread as it prepares each frame's eyes. The pitch was
// read for the newest guest frame, which may be a frame or two ahead of the one being
// drawn; it is eased over about a tenth of a second, which hides that and the camera's
// small bumps.
bool world_pitch(const gx::Batch&, float* pitch) {
    if (!g_follow_pitch || !g_have_pitch.load(std::memory_order_relaxed)) return false;
    static float eased = g_camera_pitch.load(std::memory_order_relaxed);
    eased += (g_camera_pitch.load(std::memory_order_relaxed) - eased) * 0.15f;
    *pitch = eased;
    return true;
}

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
    // is level in the headset. In a race the camera's own pitch is followed instead (see
    // world_pitch) unless vr.txt has follow_camera_pitch 0.
    c.world_pitch_deg = 18.0f;
}

void config_loaded(const VrConfig& c) {
    g_follow_pitch = c.get("follow_camera_pitch", 1.0f) != 0.0f;
    g_fp[0] = c.get("fp_x", g_fp[0]);
    g_fp[1] = c.get("fp_y", g_fp[1]);
    g_fp[2] = c.get("fp_z", g_fp[2]);
    g_fp_hide = c.get("fp_hide", g_fp_hide);
}

const bool installed = [] {
    vr::GameHooks h;
    h.config_defaults = config_defaults;
    h.config_loaded = config_loaded;
    h.wants_stereo = wants_stereo;
    h.eye_filter = eye_filter;
    h.set_first_person = set_first_person;
    vr::set_game_hooks(h);
    gx::set_guest_frame_hook(guest_frame);
    gx::render_set_world_pitch_hook(world_pitch);
    gx::render_set_eye_hook(eye_hook);
    return true;
}();

}  // namespace
