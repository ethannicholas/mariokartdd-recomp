# Working notes for this repository

This is a static recompilation of *Mario Kart: Double Dash!!* (GameCube). The recompiler,
runtime, renderer and frontends are the `gcn-recomp` submodule (canonical checkout at
`~/Source/gcn-recomp`), shared with `~/Source/bluestorm-recomp`, `~/Source/prime-recomp`
and `~/Source/fzerogx-recomp`. Fix shared things there: commit in the submodule, push to
the canonical repo, then commit the new pointer here. Other sessions may be pushing to
gcn-recomp at the same time: fetch and rebase before pushing. Only what is specific to this
game -- `analysis/`, the `recomp/*.txt` tables, `src/` (the game's hooks, once there are
any), `docs/dev/` -- lives in this repository. Anything game-specific a shared mechanism
needs goes in a setting or hook supplied from here, never in gcn-recomp;
`gcn-recomp/tools/check_generic.py` enforces the names part of that after every edit
(`.claude/settings.json`) and at commit in the submodule.

**The repository must contain no game code or data.** Only original source and metadata
about code layout (`analysis/`). The disc image lives in `rom/`, and everything derived
from it -- `build/main.dol` and the generated C in `build/gen/` -- is ignored by git. Never
commit, quote at length, or paste disassembly of the game into tracked files; a function
name and address is fine, its instructions are not. The disc also carries a linker map
(`debugInfoS.MAP`) of a different, earlier build: it is game data like the rest, and its
addresses do not match the retail DOL.

`README.md` is the public landing page: only what someone needs to build and play. Record
anything non-obvious you learn in `docs/dev/`:

- `docs/dev/status.md` -- how far the game gets, what is broken, the next milestones.
- `docs/dev/audio.md` -- the JAudio DSP ucode and how the game drives it.
- `docs/dev/routes.md` -- scripted input routes through the game.

When a belief is overturned, say what the belief was and what disproved it.

All of the game's code is in `main.dol` (no relocatable modules). `analysis/symbols.txt` is
dtk's own analysis of the retail DOL: SDK functions it recognises are named, the rest are
`fn_<address>`. `tools/dis fn_800A1C00` (or `tools/dis 800A1C00`) disassembles a function;
name functions by hand in `recomp/names.txt`.

Commit on `main` and push as work lands; no feature branches or PRs.
