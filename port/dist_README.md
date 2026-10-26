# The Simpsons Hit & Run: native Linux port (binary release)

A native Linux 64-Bit build of The Simpsons Hit & Run (2003) using SDL2 and OpenGL 3.3. It runs on X11 and Wayland, with
widescreen, high resolutions, anti-aliasing, anisotropic filtering, controllers and optional post-processing shaders.

**Bring your own game.** This release has no game data. You need your own copy of the PC game, installed or copied
from the disc: the folder holding `art/`, `scripts/` and the `.rcf` files.

## Requirements

A 64-bit (x86-64) Linux as new as Ubuntu 24.04 / Linux Mint 22 (glibc 2.38 or newer) and a desktop. The
graphics driver and X11 / Wayland libraries come from your system; everything else is included.

Any desktop install already has them. If `run.sh` says some are missing (on a minimal Debian / Ubuntu install):

```bash
sudo apt install libgl1 libgl1-mesa-dri libegl1 libx11-6 libxext6 libxcursor1 libxi6 libxrandr2 libxss1 \
    libxfixes3 libxkbcommon0 libwayland-client0 libwayland-cursor0 libwayland-egl1 libdecor-0-0 libxrender1 \
    libx11-xcb1 libxcb-render0 libxcb-shm0 libxcb-dri3-0 libgbm1 libdrm2
```

## Running

```bash
./run.sh "/path/to/The Simpsons - Hit & Run"
```

After the first run, `./run.sh` alone is enough. It sets up a `run/` folder next to itself, with links to your game
data and its own copies of `simpsons.ini` and the saves, so your original install is never written to.

Alternatively, extract the release's contents straight into the game folder (next to `art/` and `scripts/`) and run
`./run.sh` there with no path. The game then runs in place: no `run/` folder is made, and `simpsons.ini`, the saves
and `logs/` are written in the game folder (the `run/...` paths below are then in the game folder).

- Settings: `run/simpsons.ini` (`#System` section: display mode, resolution, anti-aliasing, mipmaps, anisotropy)
- Shaders: `shaders/` (see `shaders/README.md`). Ambient occlusion, tone mapping and soft lighting on characters are
  on by default; press M in the game to turn them off and on, or edit `shaders/post.ini` and `shaders/materials.ini`.
- Options: `./run.sh skipfe` skips the front end and goes straight into level 1
- Mods: `./run.sh --mod /path/to/SomeMod.lmlm` (work in progress, see [Mods](#mods-work-in-progress))

## Mods (work in progress)

> **Mod support is a work in progress and is here for testing.** Many mods load and play, but some launcher
> features are missing or only partly done, and mods may show wrong textures, break missions or crash the game. Don't
> rely on it for a full playthrough yet, and keep backups of any saves you care about.

Mods made for Lucas' Simpsons Hit & Run Mod Launcher load without the launcher. Give `run.sh` the mod with `--mod`:
either a `.lmlm` package (read in place, no need to unpack it) or an extracted mod folder (the one holding
`Meta.ini`):

```bash
./run.sh "/path/to/The Simpsons - Hit & Run" --mod "/path/to/SomeMod.lmlm"
./run.sh --mod ~/mods/SomeMod.lmlm --mod ~/mods/AnotherModFolder     # several mods; the game path is remembered
```

- Repeat `--mod` to load several. If two mods replace the same file, the one given first wins.
- Mods load only for that run. Start without `--mod` to play the unmodded game.
- A mod's settings take the defaults from its `Meta.ini`. To change them, create `run/ModSettings.ini` with a
  `[ModInternalName]` section and `SettingName=value` lines.
- Total-conversion mods (`Main=1` in `Meta.ini`) keep their own saves in `run/ModSaves/<ModInternalName>/`, apart
  from the normal game's.
- At startup the log (`run/logs/last-session.log`) lists each mod loaded and any launcher hacks it needs that aren't
  supported yet. Those parts of the mod are ignored. For more detail, run with `SHAR_MOD_LOG=1 ./run.sh ...`.

What works so far: Lua scripting, Ogg audio, HD movies, custom text, car shops and skin shops, the 3D phone booth
preview, custom cars, new dialogue characters, mission script extensions, moved or added interiors, raised engine
limits, custom traffic and per-road traffic behaviour. Not done yet: custom characters' animation sets, custom audio,
custom bonus missions, skin preview animations, shader parameter overrides, custom trigger actions and hover car
refraction.

Tested so far: the Futurama demo (level 1), Fully Connected Map (level 1) and SHAR Remastered (boots). If a mod
misbehaves, report the mod's name and version along with `run/logs/last-session.log`.

## What this is

*The Simpsons: Hit & Run* shipped in 2003 for Windows XP, Direct3D 8 and 32-bit x86. Twenty years on, the PC
release survives through compatibility shims, community patches and Wine. Each of those layers stands between the
player and the game, and any of them can break.

This project removes those layers. It is the **original game's own code**, compiled for modern operating systems,
with everything Windows-specific under it rewritten:
| Original (2003) | This project |
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
| Linux x86-64 (default) | **Boots and plays** (tested: movies, front end, levels, races, interiors, the bonus game, mods); long play sessions in progress |
| Linux x86 (32-bit, legacy) | **Playable.** Tested through Level 7 |
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
  custom text, car shops, new dialogue characters, the 3D phone booth preview, custom cars, mission script
  extensions, moved or added interiors, raised engine limits and per-road traffic behaviour
- Custom animation sets, audio and the remaining launcher hacks are not supported yet
- **Work in progress:** see [Mods](#mods-work-in-progress) for how to load one for testing
