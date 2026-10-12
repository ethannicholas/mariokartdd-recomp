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
  [decomp-toolkit](https://github.com/encounter/decomp-toolkit)). It contains no
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

Early. The game boots through the Nintendo logo and the opening movie to the title screen,
and its attract mode cycles through demo races, which do not render yet. There is no audio
yet. See `docs/dev/status.md`.

## Building

### Requirements

macOS on Apple Silicon, with Xcode (or the Command Line Tools), CMake 3.20+, Ninja,
Python 3 and SDL2:

```sh
brew install cmake ninja sdl2 python
```

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
./build.sh
```

The first build fetches the `gcn-recomp` submodule if a plain `git clone` left it empty
(`git clone --recursive` avoids that).

### 3. Run

```sh
./build/mkdd            # uses the configured image / rom/*
./build/mkdd path/to/game.iso
```

Run it from the repository root: the memory card is created in `saves/` relative to the
current directory. Every run also records the controller input to `saves/inputs/<timestamp>/`
so that a route can be played back with `--replay=<that directory>`; `--no-input-log` turns
that off.

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
- `tools/`: `analyze.sh`, which regenerates `analysis/`; `dis` and `disraw`, which print
  disassembly from it and from your extracted executable.
- `docs/dev/`: working notes.
