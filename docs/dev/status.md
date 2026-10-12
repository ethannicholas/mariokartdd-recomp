# Status

## 2026-10-12: title screen and attract mode

- Nintendo logo (fades in and out); the opening movie (`Movie/play1.thp`, THP, 608x448
  interlaced, with audio); the title screen with PRESS START.
- Left alone, attract mode loads demo races one after another (Daisy Cruiser, Yoshi
  Circuit, ...) and runs them, but they draw almost nothing: a dark screen with the logo in
  the corner. Not looked at yet.
- No audio: the DSP ucode's protocol is emulated but it renders silence (`audio.md`).

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

## Next

- The demo races: why they draw nothing.
- A route from the title screen into a race (`routes.md`).
- Audio: synthesis for the JAudio ucode.
- Windows: `build.ps1` from prime-recomp has not been brought over.
