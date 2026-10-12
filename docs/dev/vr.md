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
  wheelbase about 100 and its track about 150, its wheels about 65 across.
- `world_pitch_deg` 18: the chase camera looks down at the kart. The wheels' lowest points,
  front against rear, rise 35 units over 105 forward in view space, about 18 degrees.
  Without it the track ran uphill in the eye. The camera's pitch changes with speed and
  turns; a fixed value is a start, and the camera's own matrix per frame
  (`render_set_world_pitch`) would follow it.
- `far_m` 4000: the sky dome is drawn up to about 1.8 km out.

## What the eyes leave out

The race's post-processing is orthographic and samples EFB copies: draws that put a
half-size copy of the frame back over it (an alpha mask, an RGB565 copy and an RG8 one,
made every frame). An eye hangs orthographic draws on the HUD frame, where these showed as
a translucent rectangle of smeared scenery. `eye_filter` hides every orthographic draw that
samples a copy. The HUD itself samples ordinary textures.

## Still to look at

- Wearing it: scale, the HUD's distance and size, the pitch, comfort at speed.
- The camera's pitch per frame instead of a constant.
- A first-person view from the driver's seat: the game has none, so it would be an eye
  hook (as Prime's) placed from the kart's pose, hiding the driver.
- Lens flare: the game tests the sun's visibility by copying 2x2 pixels of the EFB and
  reading them back; what an eye does to that is unexamined.
- The theater screen as a stereo pair for the 3D menus (`theater_stereo`).
