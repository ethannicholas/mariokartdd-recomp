# VR

The headset frontends (PC VR with `--vr` on Windows, the Quest app) are gcn-recomp's
(`openxr/`, `android/`). What is this game's is in `src/`:

- `race.cpp` reads the race out of the game's memory;
- `vr_mkdd.cpp` answers the frontend's hooks (`vr::GameHooks`) and decides, once a frame
  on the guest thread (`gx::set_guest_frame_hook`), whether to be in stereo.

Not yet worn: everything below was checked with the desktop eye preview (`--eye`, which
renders through the stereo path with the game's own VR settings and eye filter) and the
stereo log (`MKDD_VRLOG=1`).

## What the game's memory says

From the decompilation's headers (`RaceMgr.h`, `RaceDirector.h`, `RaceInfo.h`,
`PauseManager.h`), the offsets confirmed in the retail code and by RAM dumps
(`GCN_RAM_DUMP_AT`) over the route in `routes.md`, a pause, and the title's attract demo.

| where | what |
|---|---|
| 0x803CB7E8 | `RaceMgr::sRaceManager`; 0 outside a race scene |
| RaceMgr+0x18 | its `RaceDirector` |
| RaceMgr+0x38 | its `RaceInfo` (`isRaceModeMiniGame` reads the mode through it) |
| RaceDirector+0x0C | the phase. 1 the course flyover; 17 from the camera's drop onto the grid to the finish (`RaceDirector::calc` checks for the race's end only in 17); 16, 18, 19 run `doSafety`, `doWait` and the ending (by `calc`'s jump table) |
| RaceDirector+0x30 | within phase 17: 0 on the grid, 1 the countdown, 2 racing |
| RaceInfo+0x08 | the mode: 1 time trial, 2 Grand Prix, 3 versus, 4-7 battles, 8 award, 9 credits |
| RaceInfo+0x1E | human players; 0 in the attract demo |
| RaceInfo+0x20 | screens (split screen) |
| RaceInfo+0x28 | demo type: nonzero in the attract demo |
| 0x803CB6DC | `PauseManager::mIsPause`, 1 on the pause menu (found by diffing a pause against racing either side of it; it sits after `PauseManager::mThis`, as in the decompilation) |

## When to be in stereo

`race_live`: a race scene, not the attract demo, at least one human player, one screen, a
racing or battle mode, phase 17, not paused. So stereo from the grid through the
countdown to the finish; the flat screen for the menus, the flyover, the pause menu, the
results, the award ceremony and split screen. After a pause, stereo comes back only once
the race has been live for 45 frames, so quitting from the pause menu does not blink
into stereo on the way out. On the scripted route: stereo at frame 6306, the flat screen at
the pause (7202), stereo again at 7508.

## Scale and pitch

- `units_per_metre` 100: in view space the player's kart is about 200 units long, its
  wheelbase about 100 and its track about 150, its wheels about 65 across. (Its speed says
  otherwise: about 43 units a frame at roughly 30 mph on the speedometer would be nearer
  190 units a metre. Kart games are not to scale; the size is what a headset judges.)
- The chase camera looks down at the kart, by about 18 degrees on the flat (its position
  and look-at point, and the wheels' lowest points in view space, agree). The world is
  levelled by the camera's own pitch every frame: `camera_pitch` reads `KartCam`'s view
  matrix on the guest thread (atan2 of the world's up in the camera's frame), and the
  renderer takes it through `render_set_world_pitch_hook` on its own thread, eased over
  about a tenth of a second since the guest may be a frame or two ahead.
  `follow_camera_pitch 0` in `vr.txt` uses the fixed `world_pitch_deg` (18) instead.
- `far_m` 4000: the sky dome is drawn up to about 1.8 km out.

| where | what |
|---|---|
| 0x803CC588 | the kart controller: the eight karts' bodies first, the players' cameras (`KartCam*`) at +0x200 |
| KartCam+0x00 | the `KartBody` it follows |
| KartCam+0x80 | its view matrix, GX 3x4 by rows (+0x148 field of view, 80; +0x150 near 10; +0x154 far 200000; +0x1E8 position; +0x1F4 look-at) |
| KartBody+0x110 | its world matrix: columns the kart's left, up and forward (the direction of travel), then its position (also at +0x23C; the last frame's at +0x248) |

## First person

The game has no cockpit camera, so first person (the right thumbstick click; `--first-person`
with `--eye` on the desktop) is an eye hook that puts the eye in the driver's seat. Each
frame the guest thread reads the kart's matrix in the camera's view space (`kart_in_view`:
the body's world matrix through the camera's view). The eye hook, on the render thread,
looks in that frame's batch for the position matrix with the same rotation whose origin is
nearest -- the kart body model, whose origin sits 60 units up the kart's axis from the
body's -- and places the eye at (`fp_x`, `fp_y`, `fp_z`) = (0, 75, 20) in it, looking
along the kart's nose and tilting with it. Taking the kart from the batch rather than from
memory keeps the eye on exactly the kart that frame draws. Draws whose position matrix's
origin is within `fp_hide` (45 units) of the eye -- the driver's head and shoulders -- are
left out of the eyes.

In the eye preview this gives the steering wheel and the kart's nose ahead, the front tyres
at the edges. Lakitu, who the game hangs in front of its camera at the start and when the
kart is stuck, hangs close in front of the eye too.
## What the eyes leave out

The race's post-processing is orthographic and samples EFB copies: draws that put a
half-size copy of the frame back over it (an alpha mask, an RGB565 copy and an RG8 one,
made every frame). An eye hangs orthographic draws on the HUD frame, where these showed as
a translucent rectangle of smeared scenery. `eye_filter` hides every orthographic draw that
samples a copy. The HUD itself samples ordinary textures.

## Still to look at

- Wearing it: scale, the HUD's distance and size, the pitch, comfort at speed.
- First person in a headset: the seat's height, whether tilting with the kart on slopes and
  bumps is comfortable or the view should be levelled, Lakitu up close.
- Lens flare: the game tests the sun's visibility by copying 2x2 pixels of the EFB and
  reading them back; what an eye does to that is unexamined.
- The theater screen as a stereo pair for the 3D menus (`theater_stereo`).
