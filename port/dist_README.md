# The Simpsons Hit & Run: native Linux port (binary release)

A native Linux build of The Simpsons Hit & Run (2003) using SDL2 and OpenGL 3.3. It runs on X11 and Wayland, with
widescreen, high resolutions, anti-aliasing, anisotropic filtering, controllers and optional post-processing shaders.

**Bring your own game.** This release has no game data. You need your own copy of the PC game, installed or copied
from the disc: the folder holding `art/`, `scripts/` and the `.rcf` files.

## Requirements

A 64-bit (x86-64) Linux with glibc 2.43 or newer (Ubuntu 26.04 or a distribution of the same age) and a desktop. The
graphics driver and X11 / Wayland libraries come from your system; everything else is included.

## Running

```bash
./run.sh "/path/to/The Simpsons - Hit & Run"
```

The first start runs `setup.sh`, which unpacks the movie player's libraries (`ffmpeg-libs.tar.xz`) into `lib/` and
checks that your system has the graphics and windowing libraries the game needs, listing any that are missing with the
command to install them. You can also run `./setup.sh` yourself first.

After the first run, `./run.sh` alone is enough. It sets up a `run/` folder next to itself, with links to your game
data and its own copies of `simpsons.ini` and the saves, so your original install is never written to.

- Settings: `run/simpsons.ini` (`#System` section: display mode, resolution, anti-aliasing, mipmaps, anisotropy)
- Shaders: `shaders/` (see `shaders/README.md`). Press M in the game to turn them on and off.
- Options: `./run.sh skipfe` skips the front end and goes straight into level 1

## 32-bit build

The 32-bit build is a separate download (`shar-linux-i386-<date>.tar.gz`); it is not part of this release. It runs on
a 64-bit Linux with the 32-bit system libraries installed. On Debian / Ubuntu:

```bash
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install libc6:i386 libgl1:i386 libgl1-mesa-dri:i386 libegl1:i386 libx11-6:i386 libxext6:i386 \
    libxcursor1:i386 libxi6:i386 libxrandr2:i386 libxss1:i386 libxfixes3:i386 libxkbcommon0:i386 \
    libwayland-client0:i386 libwayland-cursor0:i386 libwayland-egl1:i386 libdecor-0-0:i386 libxrender1:i386 \
    libx11-xcb1:i386 libxcb-render0:i386 libxcb-shm0:i386 libxcb-dri3-0:i386 libgbm1:i386 libdrm2:i386
```

With an NVIDIA card, also install the driver's 32-bit libraries (e.g. `libnvidia-gl-<version>:i386`). Then unpack the
tarball and start it the same way: `./run.sh "/path/to/The Simpsons - Hit & Run"`.

## Problems

If the game crashes or misbehaves, send `run/logs/last-session.log` with a description of what you were doing.
