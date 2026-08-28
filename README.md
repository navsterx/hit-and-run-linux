# SHAR Native

**A native source port of *The Simpsons: Hit & Run* (2003): the original game, running natively on modern systems.**

> **Private repository.** This port is built on the game's original, proprietary source code. It must never be
> made public, forked publicly or distributed. No game data is included: you need your own copy of the PC game.

---

## What this is

*The Simpsons: Hit & Run* shipped in 2003 for Windows XP, Direct3D 8 and 32-bit x86. Twenty years on, the PC
release survives through compatibility shims, community patches and Wine. Each of those layers stands between the
player and the game, and any of them can break.

SHAR Native removes those layers. It is the **original game's own code**, compiled for modern operating systems,
with everything Windows-specific under it rewritten:

| Original (2003) | SHAR Native |
|---|---|
| Direct3D 8, fixed-function | OpenGL 3.3 core, GLSL shaders matching Direct3D 8's output |
| DirectInput | SDL2: keyboard, mouse, any modern gamepad, rumble |
| DirectSound + EAX hardware reverb | Software 3D mixer with EAX-style reverb in software |
| Bink video | FFmpeg |
| Win32 threads, files, windowing | Portable platform layer |
| 640x480 to 1600x1200, 4:3 | Any resolution, any aspect ratio, resizable window |

It is **not** an emulator, a remake or a reimplementation that approximates the game. The physics, AI, mission
scripting and timing are the shipped game's. The only gameplay changes are fixes for bugs that showed up once the
code left MSVC and Windows.

## Why it matters

- **Preservation.** The game no longer depends on an old OS, an old graphics API or 32-bit Windows compatibility.
  It is an ordinary codebase that can be built, debugged and kept running as platforms change.
- **Fidelity.** Because this is the original code, edge cases, glitches players know and speedrun tricks behave
  as they always did.
- **A foundation for modding.** Mods made for Lucas' Mod Launcher load natively, and engine limits that mods once
  had to patch into a running executable can be changed in the source.
- **Modern presentation without changing the game.** Widescreen, high resolutions, anti-aliasing and texture
  filtering, plus optional shaders, all leave the look of the original art untouched by default.

## Platforms

| Platform | Status |
|---|---|
| Linux x86 (32-bit) | **Playable.** Tested through Level 7 |
| Linux x86-64 | Planned (64-bit build in progress) |
| Windows x64 | Planned |
| macOS (Apple silicon / Intel) | Planned |
| Android / handhelds | Possible later: the renderer and input layers already allow it |

## Features

**Display**
- Any aspect ratio: 16:9, 16:10, ultrawide and narrower. The 3D view widens (Hor+) while the 4:3 interface stays
  centred and undistorted
- Resizable window, borderless fullscreen at the desktop resolution, or exclusive fullscreen
- Multisample anti-aliasing (4x by default) with smooth edges on fences and foliage
- Mipmapped textures with up to 16x anisotropic filtering
- Gamma that works in a window and under Wayland
- Vsync with a 60 fps cap, so physics and AI keep the timing they were written for

**Sound and video**
- Every sound effect, music track, dialogue line and movie, including the retail release's formats
- The game's environmental reverb (tunnels, sewers, interiors) in software, no sound card features needed

**Input**
- Keyboard and mouse, including mouse look
- Gamepads through SDL with console-style default bindings; Start pauses; gamepad rumble (which the Windows
  release never supported)

**Shaders (optional, off by default)**
- Post-processing passes: ambient occlusion, bloom, colour grading, tone mapping, CRT and more
- Per-material shaders such as toon shading or new car paint, assigned by material type, file or name
- Everything reloads while the game runs. Press **M** to compare with and without them

**Mods**
- Lucas' Mod Launcher packages (`.lmlm`) and extracted mod folders, with Lua scripting, Ogg audio, HD movies,
  custom text, car shops, new dialogue characters and the 3D phone booth preview
- Custom cars, characters, interiors and the remaining launcher hacks are not supported yet

**Robustness**
- A missing file is reported by name instead of hanging the game behind a disc-error screen
- Crashes leave a log with a backtrace resolved to source lines

## What you need

- **A copy of the PC game.** The retail release's data works, as does the data from the source dump. SHAR Native
  contains no game data and never will.
- For now, a Linux x86 system with OpenGL 3.3

## Quick start

```bash
port/play.sh --data "/path/to/The Simpsons - Hit & Run"
```

The first run builds the game, creates `run/` with links to your game data and its own `simpsons.ini` and save
files, then starts the game. Your original install is never written to.

Useful options:

```bash
port/play.sh --data DIR --mod MyMod.lmlm    # load a mod (repeatable; earlier mods win)
port/play.sh --data DIR --no-build          # skip the build step
port/play.sh --data DIR skipfe              # straight into Level 1
```

## Settings

Display mode, resolution and the game's usual options are in the in-game menus. Graphics extras go in the
`#System` section of `run/simpsons.ini`:

| Setting | Values | Default |
|---|---|---|
| `antialiasing=` | 0 (off), 2, 4, 8, 16 | 4 |
| `mipmaps=` | yes, no | yes |
| `anisotropy=` | 1 (off) to 16 | 16 |
| `display=` | window, borderless, fullscreen | from the menu |

Shaders live in `run/shaders/`: turn on post-processing in `post.ini` and assign material shaders in
`materials.ini`. See [`shaders/README.md`](shaders/README.md).

## Status and roadmap

The base game is feature-complete and plays through to Level 7. The full checklist of what is done and what is
left is in [`TODO.md`](TODO.md). Next, in order:

1. **Full verification**: a complete playthrough of all seven levels, races, the bonus game
   with several pads, and the other languages
2. **Robustness**: fixing whatever those runs turn up, plus continued memory checking (valgrind) of new areas
3. **Mods**: the remaining Mod Launcher hacks, starting with shop item lists and custom cars
4. **64-bit build**, then **Windows and macOS**
5. **Packaging**: a self-contained bundle for each platform
6. **Steering-wheel force feedback**, once there is a wheel to test with

## Reporting a crash

Every session's output is saved to `run/logs/last-session.log`. After a crash (or an error exit, or Ctrl+C) it is
kept as `run/crashes/crash-<date>-<time>.log`, ending in a backtrace resolved to source files and lines. Attach
that file and say what you were doing.

## Building from source

Requirements (Debian/Ubuntu package names; enable i386 with `sudo dpkg --add-architecture i386`):

```bash
sudo apt install cmake ninja-build python3 gcc-multilib g++-multilib \
    libsdl2-dev:i386 libgl-dev:i386 libopenal-dev:i386 \
    libavformat-dev:i386 libavcodec-dev:i386 libswscale-dev:i386 libswresample-dev:i386 libavutil-dev:i386
```

Then:

```bash
cmake -B build-linux -G Ninja                              # release build
cmake -B build-linux-debug -G Ninja -DSHAR_RAD_DEBUG=ON    # debug build: asserts and Radical's debug output
ninja -C build-linux
```

Boot movies only play in the release build. `port/package.sh` makes a self-contained tarball for another Linux
machine. The porting details are in [`CLAUDE.md`](CLAUDE.md).

`CMakeLists.txt` is generated from the original Visual Studio projects: change `port/gen_cmake.py`, never the
generated file.

<details>
<summary>Debug and test switches</summary>

| Variable | Effect |
|---|---|
| `SHAR_FPS_CAP=n` | Frame-rate cap (default 60, 0 = off) |
| `SHAR_VSYNC=0` | Turn vsync off |
| `SHAR_FRAME_LOG=1` | Print a frame-time histogram every 5 s |
| `SHAR_NO_AUDIO=1` | Run without an audio device |
| `SHAR_SHOT_EVERY=n` | Save `shot_NNN.png` in the run directory every n seconds |
| `SHAR_KEY_SCRIPT="dik@start-end,..."` | Hold DirectInput key codes during time windows (ms; 1000+n = mouse button n) |
| `SHAR_RESIZE_SCRIPT="WxH@ms,..."` | Resize the window at those times |
| `SHAR_QUIT_AT=ms` | Close the window at that time (tests shutting down from any context) |
| `SHAR_KEYPRESS_SCRIPT="M@ms,..."` | Press keys (SDL key names) at those times, through the window's event queue |
| `SHAR_INPUT_LOG`, `SHAR_NO_INPUT`, `SHAR_NO_MOUSE`, `SHAR_FORCE_MOUSELOOK` | Input debugging |
| `SHAR_ASSERT_ABORT=1` | Make failed asserts abort instead of logging |
| `SHAR_LOAD_BUDGET_MS=n` | Time per frame given to the background loader |
| `SHAR_CRASH_HANDLER=0` | Don't install the crash-report signal handler |
| `SHAR_MSAA=n` | Anti-aliasing samples, overriding `simpsons.ini` (0 = off) |
| `SHAR_ALPHA_TO_COVERAGE=0` | Hard edges on cut-out textures even with anti-aliasing |
| `SHAR_MIPMAPS=0`, `SHAR_ANISOTROPY=n` | Texture filtering, overriding `simpsons.ini` |
| `SHAR_SHADERS=dir`, `SHAR_POST=0` | Shaders folder (default `shaders/`); post-processing off |
| `SHAR_MATERIAL_LOG=1` | Print each material's name and type when first drawn (for `materials.ini`) |
| `SHAR_GL_DEBUG=1`, `SHAR_GL_STATS=1` | OpenGL debug messages; per-frame draw/upload counts and render times |

`port/run.sh [seconds] [args]` rebuilds and runs the game under gdb, printing a backtrace on a crash.

</details>

## Repository layout

| Path | Contents |
|---|---|
| `code/` | The game |
| `libs/` | Radical's libraries: Pure3D renderer, radcore, radsound, radmovie, Scrooby UI and more |
| `build/` | The original Visual Studio projects, read by the generator |
| `port/` | Everything added for the port: build generator, platform layer, mod support, scripts |
| `shaders/` | Example shaders and their documentation |
| `mod/tools/` | Modding tools (`.lmlm` extractor) |
| `TODO.md` | Progress checklist: what is done and what is left |
| `CLAUDE.md` | Detailed porting notes: platform model and the Windows-to-Linux differences that mattered |

## Credits

- *The Simpsons: Hit & Run* was developed by **Radical Entertainment** and published by **Vivendi Universal Games**.
  *The Simpsons* is a trademark of its owners. This project is not affiliated with or endorsed by any of them.
- The mod format and hack definitions come from **Lucas' Mod Launcher** by the Donut Team.
- Built with SDL2, FFmpeg, Lua 5.4 (MIT) and libpng.

## Legal

The source code in this repository is proprietary and is not licensed to anyone. This port exists for private
preservation and research. Do not publish, share or distribute it, its builds or any part of it.
