# The Simpsons Hit & Run: native Linux port (binary release)

A native Linux build of The Simpsons Hit & Run (2003) using SDL2 and OpenGL 3.3. It runs on X11 and Wayland, with
widescreen, high resolutions, anti-aliasing, anisotropic filtering, controllers and optional post-processing shaders.

**Bring your own game.** This release has no game data. You need your own copy of the PC game, installed or copied
from the disc: the folder holding `art/`, `scripts/` and the `.rcf` files.

## Which build

- `x86_64/`: 64-bit, recommended. Needs nothing extra on a desktop Linux.
- `i386/`: 32-bit. Needs the 32-bit graphics and windowing libraries (listed in `i386/README.txt`).

Both need glibc 2.43 or newer (Ubuntu 26.04 or a distribution of the same age).

## Running

```bash
cd x86_64
./run.sh "/path/to/The Simpsons - Hit & Run"
```

After the first run, `./run.sh` alone is enough. It sets up a `run/` folder next to itself, with links to your game
data and its own copies of `simpsons.ini` and the saves, so your original install is never written to.

- Settings: `run/simpsons.ini` (`#System` section: display mode, resolution, anti-aliasing, mipmaps, anisotropy)
- Shaders: `shaders/` (see `shaders/README.md`). Press M in the game to turn them on and off.
- Options: `./run.sh skipfe` skips the front end and goes straight into level 1

## Problems

If the game crashes or misbehaves, send `run/logs/last-session.log` with a description of what you were doing.
