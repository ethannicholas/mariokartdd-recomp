# Audio: the JAudio DSP ucode

The game does not use AX. JSystem's audio library (JAudio) boots its own DSP program, a
synthesizer ucode of 0x1D20 bytes at 0x80343680 (gcn-recomp fingerprint `2FCDF1EC`), and
drives it with a mail protocol of its own. gcn-recomp emulates that protocol in
`runtime/hw/jaudio.cpp`; the protocol itself is described at the top of that file. This
page is how this game uses it.

## The game's side

- `fn_800A1C40` boots the ucode as a DSP task; `fn_800A1C00` (`DspHandShake`) is the task's
  init callback and spins on the CPU mailbox until the ucode's second boot mail,
  `F3551111`, arrives. Before gcn-recomp knew the ucode it treated it as AX, sent only
  `DCD10000`, and the game hung there before the Nintendo logo.
- `fn_800A2000` replaces the SDK's DSP interrupt handler: `DCD10000`/`DCD10001` run the
  task's init/resume callback, `DCD10003` (done) and `DCD10002` (yield) switch tasks,
  `DCD10004` (sync) calls `fn_80092D28`, which reads the following `F355xxxx` mail, and
  `DCD10005` (frame end) is answered with `CDD10003`.
- `fn_800A1D00` (`DSPSendCommands2`) sends a command: a count mail, then the words. Its
  wrappers: `fn_800A1AE0` (opcode `0x81`, five words: 64 voices and four buffers) and
  `fn_800A1BA0` (opcode `0x82`, render: 7 frames, volume `0x4000`, left and right output
  buffers 0x460 bytes apart -- 0x50 samples a frame). `fn_800A1A60` sends a voice-group
  sync: a zero mail, then `(group << 16) | flags`.
- The audio thread (`fn_80092B9C`) waits on a queue: message 0 comes from the AI DMA
  callback, message 1 from each `F355FF00` (a frame rendered). It counts the frames down
  and, at zero, starts the next render (`fn_80092374`); otherwise it sends the next frame's
  syncs (`fn_80091F48`).

## Status

The protocol runs: the game boots, the audio thread cycles, and the output buffers are
filled with silence. Nothing is synthesised yet -- the voice parameter blocks, the ARAM
sample formats, resampling and the reverb buffers are next.
