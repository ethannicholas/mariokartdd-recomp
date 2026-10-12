// Motion telemetry: the player's kart, for a motion platform.
//
// Once per game frame (the guest frame hook in vr_mkdd.cpp) the kart's world matrix is read
// (kart_pose in race.h): the body the first player's camera follows. Velocity is differenced
// from its position, a frame being 1/60 s -- the game steps once a frame -- and eased over
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
// side against it. Yaw is the heading, which the telemetry module washes out.
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

struct Track {
    int known = 0;   // 0 nothing, 1 the position, 2 the velocity too, 3 everything
    float pos[3];
    float vel[3];    // m/s, eased
    float acc[3];    // m/s^2, eased
};
Track g_track;

float dot(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// MKDD_MOTIONLOG=1: what goes into the telemetry module twice a second, before its fades
// and limits (GCN_TELEMETRY_LOG shows what comes out).
bool log() {
    static const bool on = getenv("MKDD_MOTIONLOG") != nullptr;
    return on;
}

}  // namespace

void motion_frame(bool live) {
    if (!telemetry::enabled()) return;
    telemetry::Motion m;
    Track& t = g_track;
    KartPose p;
    if (!live || !kart_pose(&p)) {
        // Paused, between races, in a menu: settle, and pick up afresh after.
        t.known = 0;
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
        auto rate = [](float tau) { return tau > 0.0f ? 1.0f - expf(-kDt / tau) : 1.0f; };
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
    constexpr float kDeg = 57.2957795f;
    auto clamp1 = [](float x) { return fmaxf(-1.0f, fminf(1.0f, x)); };
    static const float accel_scale = telemetry::get("accel_scale", 0.3f);

    m.active = true;
    m.speed_kmh = fabsf(p.speed) * 2.16f;
    m.pitch = asinf(clamp1(fwd[1])) * kDeg;      // nose up is positive
    m.roll = -asinf(clamp1(right[1])) * kDeg;    // right side down is positive
    m.yaw = atan2f(-fwd[0], fwd[2]) * kDeg;      // a heading: turning right raises it
    m.surge = dot(t.acc, fwd) * accel_scale;
    m.sway = dot(t.acc, right) * accel_scale;
    m.heave = dot(t.acc, up) * accel_scale;
    m.sway_velocity = dot(t.vel, right);
    if (log()) {
        static int n;
        if (n++ % 30 == 0)
            fprintf(stderr, "[motion] %5.1f km/h pitch %+5.1f roll %+5.1f heading %+6.1f | surge %+6.2f sway %+6.2f heave %+6.2f m/s2 slide %+5.2f m/s\n",
                    m.speed_kmh, m.pitch, m.roll, m.yaw, m.surge, m.sway, m.heave, m.sway_velocity);
    }
    telemetry::publish(m);
}

}  // namespace mkdd
