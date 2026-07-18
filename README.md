# The Simpsons: Hit & Run — native Linux port

A port of the original 2003 PC source code of *The Simpsons: Hit & Run* to Linux, running natively
(no Wine) on SDL2 and OpenGL. The game code is kept as close to the original as possible; Windows-only
pieces (Direct3D 8, DirectInput, DirectSound/EAX, Bink, Win32 threads and files) are replaced by
Linux implementations underneath it.

> **This repository is private.** It contains proprietary source code and must not be made public.
> No game data is included: you need your own copy of the PC game.

## Status

The game builds, boots, plays through the front end and into gameplay with sound, music, dialogue and
movies. It works with the retail PC release's data as well as the data from the source dump.

### Done

- [x] **Build system** — CMake + Ninja project generated from the original Visual Studio projects
  (`port/gen_cmake.py`), 32-bit (`-m32`), release and debug (original `RAD_DEBUG`) configurations
- [x] **Platform layer** — SDL2 window and event loop; Win32 threads, sync, fibers and timers on POSIX;
  case-insensitive file access to the game data
- [x] **Renderer** — the OpenGL Pure3D backend on SDL2, with
  - [x] DXT textures decoded in software (correct row order, so sprites and HUD portraits are the right way up)
  - [x] multithreaded loading via shared GL contexts (Direct3D 8 was multithreaded, GL is not)
  - [x] texture clamping matching Direct3D (no dark seams)
  - [x] vsync plus a 60 fps cap (physics and AI break at thousands of fps)
  - [x] colour-write masks and alpha testing matching Direct3D (HUD map, see-through fences and ramps)
- [x] **Gameplay** — loads levels and plays; uninitialised reads that were zero by luck under MSVC fixed
- [x] **Input** — keyboard and mouse (mouse look via relative mode), wired and wireless gamepads through SDL;
  verified in play; Start pauses; the menu cursor hides while a gamepad is in use
- [x] **Rumble** — gamepad vibration through SDL (the game's Force Feedback option; the Windows release never
  rumbled gamepads)
- [x] **Audio** — software mixer on SDL audio with 3D positioning: sound effects, music, dialogue
- [x] **EAX reverb** — software reverb in tunnels, sewers and interiors, driven by the game's own presets;
  verified by ear
- [x] **Movies** — Bink movies decoded with FFmpeg, audio through the game's mixer
- [x] **Retail PC data** — split music archives (`music00`–`03.rcf`), Radical ADPCM dialogue,
  logo movies replaced by stubs in some copies
- [x] **Any aspect ratio** — 16:9, 16:10, ultrawide and narrower windows; the 3D view widens (Hor+) while
  the 4:3 UI stays centred; letterbox bars, fades and the front-end TV frame span the window; verified in play,
  including extreme ratios
- [x] **Resizable window** — the game adapts live to any window size
- [x] **Fullscreen** — borderless at the desktop's native resolution, or exclusive at the chosen resolution
- [x] **Resolution menu** — common PC sizes plus the desktop's own, filtered to what fits the screen;
  any `WxH` set in `simpsons.ini` is accepted
- [x] **Launcher** — `port/play.sh` builds if needed, sets up the run directory and starts the game

### To do

- [ ] **Full playthrough** — all seven levels, missions, races, bonus game, cutscenes, saving and loading
- [ ] **64-bit build** — the code assumes 32-bit pointers in places
- [ ] **Modern OpenGL** — replace the fixed-function GL 1.x renderer (core profile, shaders)
- [ ] **Mods** — Mod Launcher mods load (custom files, Lua, Ogg audio, HD movies); custom cars, characters,
  shops and the other hacks are not implemented yet (Futurama: Hit & Run crashes at the car shop)
- [ ] **Packaging** — a simple install or AppImage-style bundle of the binary

## Building

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

`CMakeLists.txt` is generated: change `port/gen_cmake.py` and run `python3 port/gen_cmake.py` instead of editing it.

## Running

Point the launcher at your copy's PC data directory (the one containing `art/`, `scripts/` and the `.rcf` files):

```bash
port/play.sh --data "/path/to/The Simpsons - Hit & Run"
port/play.sh --data /path/to/game --debug      # use the debug build
port/play.sh --data /path/to/game --no-build   # skip the build step
port/play.sh --data /path/to/game skipfe       # extra arguments go to the game (skipfe: straight into Level 1)
```

On first use it creates `run/` with links to the game data and a private copy of `simpsons.ini` and the save
files, which the game writes to. Boot movies only play in the release build.

### Debug and test switches

| Variable | Effect |
|---|---|
| `SHAR_FPS_CAP=n` | Frame-rate cap (default 60, 0 = off) |
| `SHAR_VSYNC=0` | Turn vsync off |
| `SHAR_FRAME_LOG=1` | Print a frame-time histogram every 5 s |
| `SHAR_NO_AUDIO=1` | Run without an audio device |
| `SHAR_SHOT_EVERY=n` | Save `shot_NNN.png` in the run directory every n seconds |
| `SHAR_KEY_SCRIPT="dik@start-end,..."` | Hold DirectInput key codes during time windows (ms; 1000+n = mouse button n) |
| `SHAR_RESIZE_SCRIPT="WxH@ms,..."` | Resize the window at those times |
| `SHAR_INPUT_LOG`, `SHAR_NO_INPUT`, `SHAR_NO_MOUSE`, `SHAR_FORCE_MOUSELOOK` | Input debugging |
| `SHAR_ASSERT_ABORT=1` | Make failed asserts abort instead of logging |
| `SHAR_LOAD_BUDGET_MS=n` | Time per frame given to the background loader |

`port/run.sh [seconds] [args]` rebuilds and runs the game under gdb, printing a backtrace on a crash.

## Layout

| Path | Contents |
|---|---|
| `code/` | The game |
| `libs/` | Radical's libraries: Pure3D renderer, radcore, radsound, radmovie, Scrooby UI, ... |
| `build/` | Original Visual Studio projects (read by the generator) |
| `port/` | Everything added for the port: generator, compatibility layer (`compat/`), scripts |
| `CLAUDE.md` | Detailed porting notes: platform model and the Windows-to-Linux differences that mattered |
