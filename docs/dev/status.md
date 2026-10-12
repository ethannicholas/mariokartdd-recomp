# Status

## 2026-10-12: a Grand Prix race

From a blank memory card, scripted input (`routes.md`) goes through the whole first-run
flow and into a race:

- Nintendo logo; the opening movie (`Movie/play1.thp`, THP, 608x448 interlaced); the
  title screen; the memory card prompt, and the save is written.
- Start Game, number of players, Grand Prix / 50cc, character select, Mushroom Cup.
- Luigi Circuit: the course flyover, the countdown, the race with seven CPU racers, the
  HUD, the minimap and the speedometer.
- Left alone at the title, attract mode plays demo races (Daisy Cruiser, Yoshi Circuit,
  ...).
- Audio: the JAudio ucode is synthesised (`audio.md`): sound effects, voices, sequenced
  and streamed music, the echo lines. Not yet checked by ear against the console.
- Speed: `mkdd_bench` runs the attract-mode races at about 150-160 fps, 330 fps over the
  whole two minutes.

## Fixed on the way (all in gcn-recomp)

- The game hung before the Nintendo logo, in the audio library's DSP handshake: the
  runtime only knew AX. It now recognises the JAudio ucode and speaks its protocol
  (`audio.md`). The DSP's mailbox also had to deliver one interrupt per interrupting mail:
  the ucode sends `DCD10004` and `DCD10005` back to back.
- The opening movie never appeared, for two reasons:
  - The THP decoder (`fn_800F0800`) refused every frame with error `0x1C`: it checks
    HID2.LCE (locked cache enabled), which the game sets on another thread, and the runtime
    kept SPRs per guest thread. The processor's configuration registers are now global.
  - The player (`PlayControl`, `fn_801B42DC`, a VI post-retrace callback) starts an
    interlaced movie only on a given field, which it asks `VIGetNextField` for, which reads
    the beam position. The runtime's VI counted lines 1..263 in every field with the
    horizontal count fixed at 1, so every field looked like the first. The beam now counts
    through both fields of a frame and has a horizontal position.
- Every race drew nothing but the HUD. The frame starts with JSystem's EFB clear: a
  black quad at the near plane with a Z texture (`GXSetZTexture`, replace) whose texels hold
  the far depth. The renderer had no Z textures, so the quad wrote the nearest depth over
  the frame and every later draw failed the depth test. Found by skipping draws
  (`GCN_DRAW_SKIP=0-0` brought the whole race back).

## Next

- A route that steers round a whole race, then the rest of the cup; the other modes.
- Audio: a listening pass; the movie audio's peak sits at a constant 15732, worth a look.
- VR: wear it (`vr.md`): stereo switching, scale, pitch and the eye filter are in and were
  checked in the desktop eye preview; the Quest targets cross-compile; PC VR is built on
  Windows only.
- Windows: `build.ps1` is in but has not been run here.
