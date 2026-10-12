// Motion telemetry: the player's kart, for a motion platform.
//
// Once per game frame (the guest frame hook in vr_mkdd.cpp) the kart is read (kart_pose in
// race.h): the body the first player's camera follows. Velocity is differenced from its
// position, a frame being 1/60 s -- the game steps once a frame -- and eased over
// `velocity_s`, and acceleration differenced from that and eased over `accel_s`
// (telemetry.txt; 0.05 s and 0.15 s by default). Both are then put in the kart's own frame:
// surge along its nose, sway to its right, heave along its up.
//
// The world is 100 units to the metre: the game's own km/h (KartCtrl::GetCarSpeed) is
// mSpeed, in units a frame, times 2.16, which is 3.6 * 60 / 100, and the speed differenced
// from the position agrees with it to within a km/h. At that scale a kart corners hard:
// 20-50 m/s^2 sideways at 95 km/h on Luigi Circuit's first turn, the bumps of the grass and
// a wall 50-90. `accel_scale` (0.3) brings them nearer a platform's range before the shared
// module's own limits (15 m/s^2 by default), which would otherwise hold every channel at
// its limit through every turn.
//
// Gravity is the world's -y, so pitch is the nose against the horizontal and roll the right
// side against it. A kart game's slopes are gentle, so pitch is doubled (`pitch_gain` 2): on
// a steep climb a rider felt the platform shift only a couple of degrees. Roll is not: the
// banked turns are steep already (30-38 degrees through Luigi Circuit's, for five seconds). In
// a crash the kart tumbles through 70 degrees and more; pitch and roll fade out for it and the
// yaw's thrash (below) stands for it instead.
//
// Yaw is not the heading. Turning the platform with every turn of the kart was jarring, and
// it cannot turn all the way round anyway, so it yaws only for what a driver would feel
// through the seat (Motion::yaw_heading false):
// - in a power slide (mCarStatus & 3), the slip angle -- the kart's nose against the way it
//   is going, positive with the nose to the right of it, 25-75 degrees in a slide -- times
//   `drift_yaw_gain` (0.25), at most `drift_yaw_max` (15) degrees, eased in and out over
//   `drift_fade_s`;
// - in a spin or a crash (KartCrash's status bits), a thrash: `spin_yaw` degrees times the
//   sine of how far the kart has turned since it started, so a spin swings the platform back
//   and forth once a turn. `yaw_rate` is raised to 360 degrees a second to let it through.
// The sideways velocity (Motion::sway_velocity) is sent only then too, for a profile that
// takes traction loss from it.
//
// The ground under the kart (its CrsGround's attribute and material) sets how rough the
// telemetry module's road texture is: `road_texture` on the road, `offroad_texture` on the
// attributes the game slows the kart on, the materials the game rumbles the controller on
// at least as much, none in the air.
//
// The engine is sent as an engine a shaker can play: `idle_rpm` (900) to `max_rpm` (8000) as
// the speed goes to `top_kmh` (130). The game's own mCarRPM sums the wheels' spin, sits at its
// ceiling (16300) at any speed and jumps by thousands from frame to frame over bumps; it is
// used only near a standstill, where it is the engine revving on the grid.
//
// Inactive -- the platform settles -- outside a live race. The first frame of a run only
// sets the position and the second the velocity, both sent as a discontinuity; so is a jump
// of more than kJump units in a frame, which is Lakitu putting the kart back on the track.
#include "motion.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "race.h"
#include "telemetry.h"

namespace mkdd {
namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr float kUnitsPerMetre = 100.0f;
constexpr float kJump = 300.0f;   // units in a frame: 650 km/h
constexpr float kDeg = 57.2957795f;

struct Track {
    int known = 0;   // 0 nothing, 1 the position, 2 the velocity too, 3 everything
    float pos[3];
    float vel[3];    // m/s, eased
    float acc[3];    // m/s^2, eased
};
Track g_track;

// The platform's yaw (see the top of the file).
struct Yaw {
    float drift_w = 0.0f;   // how much of the slide is in it, 0 to 1, eased
    float drift = 0.0f;     // the slide's angle, degrees, eased
    float spin_w = 0.0f;    // the same for the spin
    bool spinning = false;
    float heading_prev = 0.0f;
    float spun = 0.0f;      // degrees turned since the spin started
};
Yaw g_yaw;
float g_rev = 0.0f;   // the engine, 0 idle to 1 flat out, eased

float dot(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

float rate(float tau) { return tau > 0.0f ? 1.0f - expf(-kDt / tau) : 1.0f; }

// MKDD_MOTIONLOG=1: what goes into the telemetry module twice a second, before its fades
// and limits (GCN_TELEMETRY_LOG shows what comes out), with the game state it came from.
// MKDD_MOTIONLOG_EVERY=n logs every nth frame instead.
int log_every() {
    static const int n = getenv("MKDD_MOTIONLOG") ? (getenv("MKDD_MOTIONLOG_EVERY") ? atoi(getenv("MKDD_MOTIONLOG_EVERY")) : 30) : 0;
    return n;
}

// How rough the ground is, 0 to 1 (Motion::roughness).
float roughness(const KartPose& p) {
    if ((p.status & kStatusAirborne) || p.wheels_down <= 0) return 0.0f;
    static const float road = telemetry::get("road_texture", 0.4f);
    static const float offroad = telemetry::get("offroad_texture", 0.9f);
    float r = road;
    // The attributes KartCtrl::GetMaxSpeed slows the kart on.
    if (p.ground == 0x00 || p.ground == 0x03 || p.ground == 0x0C || p.ground == 0x13) r = offroad;
    // The materials KartHandle rumbles the controller on, at its strengths 30, 2 and 1.
    if (p.material == 6) r = fmaxf(r, 1.0f);
    else if (p.material == 7) r = fmaxf(r, 0.7f);
    else if (p.material == 3 || p.material == 4 || p.material == 5 || p.material == 0xE) r = fmaxf(r, 0.5f);
    return r;
}

}  // namespace

void motion_frame(bool live) {
    if (!telemetry::enabled()) return;
    static const bool defaults = [] {
        telemetry::set_default("pitch_gain", 2.0f);
        telemetry::set_default("yaw_rate", 360.0f);
        return true;
    }();
    (void)defaults;
    telemetry::Motion m;
    Track& t = g_track;
    KartPose p;
    if (!live || !kart_pose(&p)) {
        // Paused, between races, in a menu: settle, and pick up afresh after.
        t.known = 0;
        g_yaw = Yaw();
        g_rev = 0.0f;
        telemetry::publish(m);
        return;
    }
    const float pos[3] = {p.m[0][3], p.m[1][3], p.m[2][3]};
    float d[3];
    for (int i = 0; i < 3; i++) d[i] = pos[i] - t.pos[i];
    const bool jump = dot(d, d) > kJump * kJump;
    for (int i = 0; i < 3; i++) d[i] /= kUnitsPerMetre * kDt;   // now m/s
    if (t.known == 0 || jump) {
        for (int i = 0; i < 3; i++) t.vel[i] = t.acc[i] = 0.0f;
        t.known = 1;
        m.discontinuity = true;
    } else if (t.known == 1) {
        for (int i = 0; i < 3; i++) t.vel[i] = d[i];
        t.known = 2;
        m.discontinuity = true;
    } else {
        static const float vel_s = telemetry::get("velocity_s", 0.05f);
        static const float acc_s = telemetry::get("accel_s", 0.15f);
        for (int i = 0; i < 3; i++) {
            const float prev = t.vel[i];
            t.vel[i] += (d[i] - t.vel[i]) * rate(vel_s);
            t.acc[i] += ((t.vel[i] - prev) / kDt - t.acc[i]) * rate(acc_s);
        }
        t.known = 3;
    }
    for (int i = 0; i < 3; i++) t.pos[i] = pos[i];

    // The kart's axes are the matrix's columns: left, up and forward.
    const float right[3] = {-p.m[0][0], -p.m[1][0], -p.m[2][0]};
    const float up[3] = {p.m[0][1], p.m[1][1], p.m[2][1]};
    const float fwd[3] = {p.m[0][2], p.m[1][2], p.m[2][2]};
    auto clamp1 = [](float x) { return fmaxf(-1.0f, fminf(1.0f, x)); };
    static const float accel_scale = telemetry::get("accel_scale", 0.3f);

    // The slip angle: the nose against the velocity, positive with the nose to its right.
    const float v_fwd = dot(t.vel, fwd), v_right = dot(t.vel, right);
    const bool moving = v_fwd * v_fwd + v_right * v_right > 3.0f * 3.0f;
    const float slip = moving ? atan2f(-v_right, fabsf(v_fwd)) * kDeg : 0.0f;

    // The yaw: a slide, a spin, or nothing.
    Yaw& y = g_yaw;
    static const float drift_gain = telemetry::get("drift_yaw_gain", 0.25f);
    static const float drift_max = telemetry::get("drift_yaw_max", 15.0f);
    static const float drift_fade = telemetry::get("drift_fade_s", 0.25f);
    static const float spin_yaw = telemetry::get("spin_yaw", 12.0f);
    const float heading = atan2f(-fwd[0], fwd[2]) * kDeg;   // turning right raises it
    const bool spinning = (p.status & (kStatusSpin | kStatusHalfSpin | kStatusCrash)) != 0;
    const bool drifting = !spinning && (p.status & kStatusDrift) && moving;
    if (spinning && !y.spinning) y.spun = 0.0f;
    if (spinning) {
        float turned = heading - y.heading_prev;
        while (turned > 180.0f) turned -= 360.0f;
        while (turned < -180.0f) turned += 360.0f;
        y.spun += turned;
    }
    y.spinning = spinning;
    y.heading_prev = heading;
    y.spin_w += ((spinning ? 1.0f : 0.0f) - y.spin_w) * rate(0.08f);
    y.drift_w += ((drifting ? 1.0f : 0.0f) - y.drift_w) * rate(drift_fade);
    if (drifting) y.drift += (fmaxf(-drift_max, fminf(drift_max, slip * drift_gain)) - y.drift) * rate(0.15f);

    // The engine: the speed, or the game's revs near a standstill.
    static const float idle_rpm = telemetry::get("idle_rpm", 900.0f);
    static const float max_rpm = telemetry::get("max_rpm", 8000.0f);
    static const float top_kmh = telemetry::get("top_kmh", 130.0f);
    const float kmh = fabsf(p.speed) * 2.16f;
    const float by_speed = fminf(1.0f, kmh / top_kmh);
    const float revs = fminf(1.0f, fmaxf(0.0f, (p.rpm - 300.0f) / 16000.0f));
    const float standstill = fmaxf(0.0f, 1.0f - kmh / 20.0f);
    g_rev += (by_speed + standstill * fmaxf(0.0f, revs - by_speed) - g_rev) * rate(0.1f);

    const float upright = 1.0f - y.spin_w;
    m.active = true;
    m.speed_kmh = kmh;
    m.pitch = asinf(clamp1(fwd[1])) * kDeg * upright;      // nose up is positive
    m.roll = -asinf(clamp1(right[1])) * kDeg * upright;    // right side down is positive
    m.yaw_heading = false;
    m.yaw = y.drift * y.drift_w + spin_yaw * sinf(y.spun / kDeg) * y.spin_w;
    m.surge = dot(t.acc, fwd) * accel_scale;
    m.sway = dot(t.acc, right) * accel_scale;
    m.heave = dot(t.acc, up) * accel_scale;
    m.sway_velocity = v_right * fmaxf(y.drift_w, y.spin_w);
    m.rpm = idle_rpm + (max_rpm - idle_rpm) * g_rev;
    m.max_rpm = max_rpm;
    m.roughness = roughness(p);
    if (log_every()) {
        static int n;
        if (n++ % log_every() == 0)
            fprintf(stderr, "[motion] %5.1f km/h pitch %+5.1f roll %+5.1f yaw %+6.1f | surge %+6.2f sway %+6.2f heave %+6.2f m/s2 slide %+5.2f m/s | "
                            "slip %+6.1f drift %.2f spin %.2f spun %+5.0f | status %08X ground %02X/%02X rough %.2f wheels %d revs %5.0f rpm %5.0f\n",
                    m.speed_kmh, m.pitch, m.roll, m.yaw, m.surge, m.sway, m.heave, v_right, slip, y.drift_w, y.spin_w, y.spun,
                    p.status, p.ground, p.material, m.roughness, p.wheels_down, p.rpm, m.rpm);
    }
    telemetry::publish(m);
}

}  // namespace mkdd
