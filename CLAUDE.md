# CLAUDE.md: The Simpsons Hit & Run, native Linux port

Port of the original 2003 source to Linux (32-bit default, 64-bit works) with SDL2 + OpenGL 3.3 core. Original dump:
`~/Documents/Decomp/hitnrun_source/game` (never modify). Status (2026-10-21): base game plays end to end as far as
tested (Level 7), sound/music/dialogue/movies/reverb/rumble, any aspect ratio; retail PC data works; Lucas' Mod Launcher
mods partly supported. Missing: steering-wheel force feedback.

**Full detail** (every lesson, mod hack internals, memcheck history, test results) is in `port/NOTES.md`: read the
relevant section before working on a subsystem. `README.md` = user overview, `TODO.md` = checklist, `RUST.md` = notes
for a possible Rust/Bevy rewrite.

The source is leaked/proprietary: keep everything private (remote `origin` github.com/zeropaulsy/simpsons-hit-and-run-recomp
is private). Commit as `zeropaulsy <13328895+zeropaulsy@users.noreply.github.com>`; the user pushes.

## Layout

```
code/, libs/, build/   copy of the dump's sources + VS projects (git-tracked)
port/                  everything added for the port
  gen_cmake.py         generates CMakeLists.txt from the .vcxproj files (vcx.py). Never edit CMakeLists.txt by hand
  check.py FILE...     syntax-check with the target's flags; syntax_survey.py: all sources; survey64.py: pointer casts
  run.sh [secs] [args] rebuild + run under gdb, backtrace on crash
  play.sh --data DIR   set up run dir and play (--debug, --64, --no-build, --mod X); logs in run/logs, crashes in
                       run/crashes (crash_symbolize.py resolves them)
  memcheck.sh [secs]   valgrind memcheck + summary of uninitialised reads by origin
  package.sh [--64]    bundle
  compat/              msvc_compat.h (force-included), Win32 shims (threads, fibers, MessageBox), crash handler,
                       pddi_heap.cpp, srr2.map (linker version script), shar_mod*.cpp (mod support)
  third_party/lua/     Lua 5.4.8 for mods
shaders/               example post-processing/material shaders (format: shaders/README.md)
mod/tools/             lmlm_extract.py (.lmlm "LSPA" packages)
build-linux/           release (RAD_RELEASE)         build-linux-debug/  -DSHAR_RAD_DEBUG=ON (asserts)
build-linux64[-asan|-debug]/  -DSHAR_64BIT=ON        build-linux-audit/  -DSHAR_UNINIT_AUDIT=ON (all warnings)
run/                   run dir (ignored): data symlinks, user's simpsons.ini/saves. Experiments: own run dir in the
                       scratchpad via SHAR_RUN=..., e.g. display=window, resolution=1280x720
```

## Commands

```bash
python3 port/gen_cmake.py                 # after changing port rules
ninja -C build-linux-debug -j6
SHAR_BUILD=$PWD/build-linux-debug port/run.sh 120   # absolute path
SHAR_RUN=<scratch run dir> port/memcheck.sh 330 skipfe
```

Game options (`srr2 ...`): `skipfe` (straight to L1), `skipfe l7 m3` (level/mission), `race1-4` (bonus races of the
level), `demo l3` (AI drives), `incar` (put in the mission car), `tp<N>` (teleport destination N), `skipsunday` (start
mission at once; crashes loading L3 M1, L6 M1, L7 M3; a retry hangs), `nofail`, `autopass[N]` (stages pass after N s), `coins`,
`bonus` (bonus game unlocked), `l0` (bonus game menus). Details and interior door coordinates: NOTES.md "Commands".

Env vars: `SHAR_SHOT_EVERY=s` (screenshots), `SHAR_QUIT_AT=ms`, `SHAR_KEY_SCRIPT="dik@start-end,..."` (retail ini: E=18
action, Enter=28; 1000+n = mouse button), `SHAR_KEYPRESS_SCRIPT` (window keys), `SHAR_RESIZE_SCRIPT`,
`SHAR_MINIMIZE_SCRIPT`, `SHAR_TP_POS="x,y,z"`, `SHAR_TP_INTERIOR_POS`, `SHAR_MAX_FRAME_MS` (slow motion; use under
memcheck), `SHAR_ASSERT_ABORT=1`, `SHAR_FILL_ALLOC` / `SHAR_FILL_PDDI=0xFF`, `SHAR_FPS_CAP`, `SHAR_VSYNC=0`,
`SHAR_GL_DEBUG=1`, `SHAR_GL_STATS=1`, `SHAR_MSAA`, `SHAR_MIPMAPS`, `SHAR_ANISOTROPY`, `SHAR_X11=1`, `SHAR_NO_AUDIO=1`.
Logs: `SHAR_{FRAME,INPUT,SOUND,DIALOG,RSD,REVERB,RUMBLE,MOD,TRAFFIC,MATERIAL}_LOG=1`, `SHAR_MOD_DUMP=dir`.
Mesa GL errors: `__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 MESA_DEBUG=1`. No speakers:
`SDL_AUDIODRIVER=disk`. Debug logs contain NULs: `grep -a`. `perf` is not permitted on this machine.

## Platform model

- Linux is a PC platform: `RAD_WIN32` stays defined; `RAD_LINUX` marks OS differences. Also `LINUX`, `RAD_PC`,
  `PDDI_NO_ASM`. In Pure3D, Win32 blocks yield to `RAD_LINUX`.
- `gen_cmake.py`: `win32/` sources replaced by a `linux/` sibling or excluded; `LINUX_EXCLUDE` / `LINUX_EXTRA` per
  target; DX8 pddi replaced by the GL pddi.
- Ported: Pure3D platform + GL display on SDL2; radcore drive (case-insensitive paths); SDL2 controllers; window/event
  loop `code/main/linuxplatform_sdl.cpp`; radsound HAL `hal/linux/linuxhal.cpp` (SDL mixer, software 3D + reverb);
  Bink via FFmpeg (`radmovie/src/linux/binkffmpeg.cpp`, needs i386 libav* -dev packages); radscript `InvokeVf` in C++.

## Key lessons (details in NOTES.md)

- **Allocator isolation**: the game replaces global operator new. `-static-libstdc++ -Wl,--exclude-libs,ALL`,
  `srr2.map` keeps new/delete local (check `nm -D --defined-only srr2 | grep _Zn` is empty), pddigl's new/delete are
  objcopy-renamed to `pddi_heap_*` (a failed POST_BUILD objcopy is not redone: delete the library). The compat layer
  must never use C++ new (malloc only).
- **Uninitialised reads were zero by luck under MSVC**: fix by initialising to zero, mark `// uninit fix (Linux port)`;
  rerun the audit build and memcheck after porting new code.
- **Line endings**: most sources are CRLF; edit preserving them (Python `newline=''` or bytes).
- GL is single-threaded where D3D8 was not: loader threads get shared contexts; texture uploads lazy on the render thread.
- Frame timing: vsync + 60 fps cap (unsynced 0 ms frames broke physics); loads advance via `radLoadManager::Service`'s
  per-frame budget; a throttled (minimised) window pauses vsync.
- radscript relies on MSVC `typeid` names and vtable slots (work while scripted interfaces have no virtual dtors/overloads).
- The GL pddi was never finished against D3D8 (the shipped game used pddidx8): when something renders wrong, compare with
  `libs/pure3d/pddi/dx8` in the dump. Renderer: GL 3.3 core (`pddi/gl/glpipeline.cpp`, one GLSL program per material
  kind matching D3D8), MSAA FBO, user post passes (`glpost.cpp`, `shaders/post.ini`) and material shaders
  (`glcustom.cpp`, `shaders/materials.ini`); M toggles user shaders. The game renders with culling off.
- Texture row order: Lock() views are bottom-up except DXT; decoders use `Builder::BOTTOM`.
- Retail data differs from the dump (split music RCFs, RADP dialogue, Ogg logo movies).
- Black/frozen window at large sizes: check `nvidia-smi` for gnome-shell's VRAM leak (log out/in) before bisecting.
- Wayland: native SDL Wayland driver by default (XWayland leaked GBs on the laptop); 32-bit NVIDIA egl-wayland2 fails,
  egl-wayland forced. Intel Ice Lake: `INTEL_DEBUG=nohiz` set automatically (GPU hangs otherwise).
- Missing files fail the request (`File not found: <path>`) instead of retrying forever.
- Boot movies only play in release builds.
- GCC 15 libstdc++ assertions off outside the debug build. Sound clip budget doubled on Linux; failed clips play silence.
- 64-bit: pointer casts via `uintptr_t`/`GuiParam`/`shar_PointerKey()`, heaps x2, saves layout-identical.

## Mods (Lucas' Mod Launcher format)

Folders or `.lmlm` packages with `Meta.ini`, `CustomFiles/`, per-hack configs, optional Lua. `port/compat/shar_mods.cpp`
loads `SHAR_MODS` (':'-separated, earlier wins; `play.sh --mod`); every read-only open goes through `shar_ModResolve`.
Configs via `shar_ModConfig*` / `shar_modxml.h`; limits via `shar_ModLimit*`. Implemented hacks and how each is wired:
NOTES.md "Mods". Not done: CustomAnimationSetSupport, CustomAudioSupport, CustomBonusMissionSupport,
CustomSkinPreviewAnimations, OverrideShaderParameters, CustomTriggerActions, HoverCarRefraction, [Sound] limits.
Test mods: Futurama: Hit & Run demo 0.1.3, Fully Connected Map 1.0.7, SHAR Remastered 1.0.
Docs: docs.donutteam.com/docs/LucasSimpsonsHitAndRunModLauncher/.

## Priorities

1. Verify the base game: the user plays (Claude does not script gameplay it can't see). Levels 1-2 done, on Level 3.
   Riskiest left: bonus game with 2-4 pads, races, later levels, other languages.
2. Memcheck coverage (Claude, unattended): one mission per level done; L5-L7 remaining missions not yet.
3. Robustness: crashes/asserts from those runs, shutdown paths. Teardown asserts in the debug build are the original's; ignore.
4. Mods: remaining hacks, tested with the user in the loop.
5. Packaging. 6. Steering-wheel FFB (needs a wheel). 7. Renderer follow-ups (Vulkan shelved). 8. 64-bit: long sessions.
