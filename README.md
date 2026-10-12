# mariokartdd-recomp

A static recompilation of *Mario Kart: Double Dash!!* (Nintendo GameCube, 2003) to native
code.

The game's PowerPC code is translated ahead of time into C, then compiled and linked
against a runtime that stands in for the GameCube hardware (graphics, audio, DVD,
controllers, memory card). The result is a native program that runs the original game logic
without an emulator's CPU core. The recompiler and runtime are
[gcn-recomp](https://github.com/ethannicholas/gcn-recomp), shared with
[bluestorm-recomp](https://github.com/ethannicholas/bluestorm-recomp),
[prime-recomp](https://github.com/ethannicholas/prime-recomp) and
[fzerogx-recomp](https://github.com/ethannicholas/fzerogx-recomp).

> **This repository contains no game code or data.** You must supply your own disc image,
> dumped from a copy of the game that you own. The build process reads the code out of
> *your* image and recompiles it locally. No game code or assets are distributed here; what
> this repository holds about the game is a table of its function addresses and names.

## License

The code in this repository and in `gcn-recomp` is released under the [MIT License](LICENSE).

## Legal

- *Mario Kart: Double Dash!!*, GameCube and all related names and marks are property of
  Nintendo. This project is not affiliated with or endorsed by Nintendo.
- This repository contains only original source code plus metadata describing code layout
  (function addresses and names in `analysis/`, produced with
  [decomp-toolkit](https://github.com/encounter/decomp-toolkit) and named from the linker
  map that ships on the game's disc). It contains no
  copyrighted game code, assets, BIOS/IPL files or DSP microcode.
- You need a legally obtained copy of the game. Dump it from your own disc using your own
  GameCube/Wii (e.g. with [CleanRip](https://wiibrew.org/wiki/CleanRip)). Do not download
  game images, and do not share the image, anything extracted from it (`build/main.dol`),
  the generated C code (`build/gen/`), built binaries, or memory card files, since all of
  these contain or are derived from the game.

## Supported version

Only the North American release is supported:

| Title | Disc ID | `main.dol` SHA-1 |
|---|---|---|
| Mario Kart: Double Dash!! (USA) | `GM4E01` | `e389e50357d4a5076ab3d38672204658f411123d` |

The build verifies this hash and stops with an error for any other version.

## Status

Early. The game runs from a blank memory card through the opening movie, the title screen
and the menus into a Grand Prix race, with sound. See `docs/dev/status.md` for
how far it gets and what is known not to work.

## Building

### Requirements

**macOS (Apple Silicon)**

- Xcode (or the Command Line Tools), CMake 3.20+, Ninja, Python 3
- SDL2

```sh
brew install cmake ninja sdl2 python
```

**Windows (x64 or ARM64)**

- Clang, CMake 3.20+, Ninja, Python 3
- The Windows SDK and MSVC headers/libraries (the "Desktop development with C++" workload of
  Visual Studio or the standalone Build Tools): clang targets the MSVC ABI and uses them

```powershell
winget install Kitware.CMake Ninja-build.Ninja Python.Python.3.13 LLVM.LLVM Microsoft.VisualStudio.2022.BuildTools
```

**Quest 3**: see [Running on a Quest 3](#running-on-a-quest-3).

### 1. Provide your game image

Dump your disc, then place the image in `rom/` as an uncompressed **`.iso`** or a **`.ciso`**:

```
rom/game.iso
```

If your dump is in another format (`.rvz`, `.gcz`), convert it first, for example with
Dolphin (right-click the game → *Convert File…* → *Uncompressed Disc Image* or *CISO*),
`dolphin-tool convert`, or [`nodtool`](https://github.com/encounter/nod).

The build uses the first `.iso` or `.ciso` it finds in `rom/`. To use an image elsewhere,
pass `-DGAME_ISO=/path/to/game.iso` when configuring.

### 2. Build

```sh
./build.sh          # macOS
```

```powershell
.\build.ps1         # Windows
```

The first build fetches the `gcn-recomp` submodule if a plain `git clone` left it empty
(`git clone --recursive` avoids that).

### 3. Run

```sh
./build/mkdd            # uses the configured image / rom/*
./build/mkdd path/to/game.iso
```

On Windows the executable is `build\mkdd.exe`.

Run it from the repository root: the memory card is created in `saves/` relative to the
current directory. Every run also records the controller input to `saves/inputs/<timestamp>/`
so that a route can be played back with `--replay=<that directory>`; `--no-input-log` turns
that off.

## Playing in VR on a PC

Early: built, but not yet tried in a headset.

On Windows, `--vr` plays the game on a PC VR headset through OpenXR, using whichever runtime
is set as active: SteamVR, Virtual Desktop, Meta Quest Link, or any other that supports
OpenGL.

```powershell
.\build\mkdd.exe --vr
```

Connect the headset and start its PC software first. Menus, the course flyovers, the
results and split-screen races are on a flat screen in front of you; your own race is all
around you, in stereo 3D, from the moment the camera drops onto the grid to the finish, and
goes back to the flat screen while paused. Clicking the right thumbstick in a race puts you
in the driver's seat, and clicking it again goes back to the game's chase camera. The window
on the desktop mirrors the headset. A
gamepad and the keyboard work alongside the headset's controllers.

Settings are read from a `vr.txt` in the directory you run the game from; see
[Tuning the VR view](#tuning-the-vr-view). To use a runtime other than the active one, point
`XR_RUNTIME_JSON` at its manifest.

## Running on a Quest 3

Early: the app builds from the same scripts as the other gcn-recomp games, but has not yet
been played in the headset. The scripts are PowerShell, so these steps are for a Windows PC.

### Requirements

On the PC, besides CMake, Ninja and Python as for the desktop build:

- [Android NDK](https://developer.android.com/ndk/downloads), unpacked to
  `%LOCALAPPDATA%\Android\Sdk\ndk\<version>\`
- [platform-tools](https://developer.android.com/tools/releases/platform-tools) (for `adb`),
  unpacked to `%LOCALAPPDATA%\Android\Sdk\platform-tools\`
- The Android SDK's `build-tools;34.0.0` and `platforms;android-34`, installed with
  `sdkmanager` from the
  [command-line tools](https://developer.android.com/studio#command-line-tools-only)
  (run `sdkmanager --licenses` first)
- A JDK, for signing the APK: `winget install Microsoft.OpenJDK.21`

On the headset, enable
[developer mode](https://developer.oculus.com/documentation/native/android/mobile-device-setup/),
connect it over USB, and accept the *Allow USB debugging* prompt.

### Build and install

With your disc image in `rom/`:

```powershell
.\build-android.ps1          # cross-compiles libmkdd.so for arm64
.\package-apk.ps1 -Install   # packages the APK, installs it, and pushes the disc image
```

The disc image is copied to `/sdcard/Android/data/com.ethannicholas.mkdd/files/`, which
takes a few minutes the first time. The memory card and the shader cache live there too.

### Playing

The app appears under **Library → Unknown Sources → Double Dash Recomp**, and behaves as
[on a PC](#playing-in-vr-on-a-pc).

| GameCube | Touch controller |
|---|---|
| Control stick | Left thumbstick |
| C stick | Right thumbstick |
| A / B | A / B (right) |
| X / Y | X / Y (left) |
| Z | Right grip |
| L / R | Left / right trigger |
| Start | Menu (left) |
| D-pad | Left thumbstick while holding the left grip |
| *(driver's seat on/off, in a race)* | Right thumbstick click |

`adb logcat -s mkdd` shows the app's log.

### Tuning the VR view

Settings are read at startup from `vr.txt` (beside the game on a PC; on the Quest,
`/sdcard/Android/data/com.ethannicholas.mkdd/files/vr.txt`, which `adb push` updates
between runs). One `key value` per line, `#` starts a comment; all optional. The values
shown are this game's defaults:

```
units_per_metre 100      # game units per real metre: sets the apparent size of the world
near_m 0.1               # near and far planes in metres
far_m 4000
hud_scale 0.5            # how much of the field of view the HUD's frame occupies
hud_distance_m 4         # how far in front of the camera that frame stands
hud_height_m 0           # how far above the forward axis its centre sits
world_pitch_deg 18       # the chase camera's downward pitch taken out of the world, in degrees
follow_camera_pitch 1    #   1 follows the camera's own pitch in a race instead
fp_x 0                   # where your eyes sit in the driver's seat, in the kart model's
fp_y 75                  #   units (100 a metre): left, up, forward
fp_z 20
fp_hide 45               # draws this near your eyes (the driver's head) are not drawn
eye_scale 1.4            # eye resolution, times what the headset recommends
msaa 4                   # antialiasing samples per pixel in stereo
transition_s 1           # seconds the switch between flat and stereo takes
theater_stereo 0         # 1 shows the flat screen as a 3D picture (a second render pass)
```

## Controls

| GameCube | Keyboard | Controller |
|---|---|---|
| Control stick | Arrow keys | Left stick |
| C stick | – | Right stick |
| A / B / X / Y | X / Z / C / S | A / X / B / Y |
| L / R | Q / W | Triggers |
| Z | D | Right shoulder |
| D-pad | I / J / K / L | D-pad |
| Start | Return | Start |

Escape quits.

## Repository layout

- `gcn-recomp/`: the shared recompiler, GameCube runtime, renderer and frontends, as a
  submodule. Nothing in it is specific to this game.
- `analysis/`: function and data layout of `main.dol`, in decomp-toolkit's format.
- `recomp/`: the tables that steer the recompiler for this game.
- `src/`: the game's half of the headset: when to be in stereo, the world's scale, and
  what the eyes leave out.
- `tools/`: `analyze.sh`, which regenerates `analysis/`; `name_from_map.py`, which names
  its functions from the linker map on your disc; `dis` and `disraw`, which print
  disassembly from it and from your extracted executable.
- `docs/dev/`: working notes.
