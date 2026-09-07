#!/bin/bash
# Package the release build for another machine: dist/shar-linux-<arch>-<date>/ and a .tar.gz of it, holding the
# game, the libraries it needs (except the ones that must come from the target system: glibc, the OpenGL / X11 /
# Wayland stack, which goes with the graphics driver), the example shaders and a launcher (run.sh).
# usage: port/package.sh [--64]     (builds build-linux first; --64: the x86-64 build, build-linux64)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
if [ "$1" = --64 ]; then BUILD=$ROOT/build-linux64; ARCH=x86_64; else BUILD=$ROOT/build-linux; ARCH=i386; fi
NAME=shar-linux-$ARCH-$(date +%Y%m%d)
OUT=$ROOT/dist/$NAME

ninja -C "$BUILD" -j"$(nproc)" > "$ROOT/port/build.log" 2>&1 || { grep -E "error|FAILED" "$ROOT/port/build.log" | head -20; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT/lib"
cp "$BUILD/srr2" "$OUT/srr2"
cp -r "$ROOT/shaders" "$OUT/shaders"

# Libraries: everything the game links against, except glibc (must be the system's: the loader is), and the graphics
# and windowing stack (libGL & co, libdrm/gbm, X11/xcb, Wayland), which has to match the target's driver.
SYSTEM='^(libc\.so|libm\.so|libresolv\.so|libpthread\.so|libdl\.so|librt\.so|ld-linux|linux-gate|libGL\.so|libGLX\.so|libGLdispatch\.so|libEGL\.so|libOpenGL\.so|libgbm\.so|libdrm\.so|libX|libxcb|libxkbcommon|libwayland|libdecor)'
ldd "$OUT/srr2" | awk '/=>/ { print $1, $3 }' | while read name path; do
    if ! echo "$name" | grep -qE "$SYSTEM"; then
        cp -L "$path" "$OUT/lib/$name"
    fi
done

cat > "$OUT/run.sh" <<'EOF'
#!/bin/bash
# Start the game. usage: ./run.sh /path/to/game-data [game options...]
# game-data is your copy's PC data folder (holding art/, scripts/ and the .rcf files). The first time, a run/ folder
# is made next to this script with links to the data and copies of simpsons.ini and the saves (the game writes
# those); the shaders/ folder here is used too. Console output goes to run/logs/last-session.log.
HERE=$(cd "$(dirname "$0")" && pwd)
RUN=$HERE/run
DATA=
if [ -n "$1" ] && [ -d "$1" ]; then DATA=$1; shift
elif [ -f "$RUN/.data" ]; then DATA=$(cat "$RUN/.data"); fi
if [ -z "$DATA" ] || [ ! -d "$DATA" ]; then
    echo "usage: ./run.sh /path/to/game-data [options]   (the folder holding art/, scripts/ and *.rcf)" >&2
    exit 1
fi
DATA=$(cd "$DATA" && pwd)
mkdir -p "$RUN/logs"
echo "$DATA" > "$RUN/.data"
for f in ambience.rcf carsound.rcf dialog.rcf dialogf.rcf dialogg.rcf dialogs.rcf music00.rcf music01.rcf music02.rcf \
         music03.rcf nis.rcf scripts.rcf soundfx.rcf opening.bnr art movies scripts sound; do
    [ -e "$DATA/$f" ] && [ ! -e "$RUN/$f" ] && ln -s "$DATA/$f" "$RUN/$f"
done
for f in simpsons.ini Save1 Save2 Save3 Save4; do
    [ -e "$DATA/$f" ] && [ ! -e "$RUN/$f" ] && cp "$DATA/$f" "$RUN/$f"
done
[ -e "$RUN/shaders" ] || ln -s "$HERE/shaders" "$RUN/shaders"
cd "$RUN"
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$HERE/srr2" "$@" 2>&1 | tee "$RUN/logs/last-session.log"
EOF
chmod +x "$OUT/run.sh"

if [ $ARCH = i386 ]; then
    NEEDS="Needs: a 64-bit Linux as new as Ubuntu 26.04 (glibc 2.43), 32-bit graphics drivers and windowing libraries, and
your own copy of the PC game's data.

1. Install the 32-bit system libraries (Debian / Ubuntu):
     sudo dpkg --add-architecture i386 && sudo apt update
     sudo apt install libc6:i386 libgl1:i386 libgl1-mesa-dri:i386 libegl1:i386 libx11-6:i386 libxext6:i386 \\
         libxcursor1:i386 libxi6:i386 libxrandr2:i386 libxss1:i386 libxfixes3:i386 libxkbcommon0:i386 \\
         libwayland-client0:i386 libwayland-cursor0:i386 libwayland-egl1:i386 libdecor-0-0:i386 libxrender1:i386 \\
         libx11-xcb1:i386 libxcb-render0:i386 libxcb-shm0:i386 libxcb-dri3-0:i386 libgbm1:i386 libdrm2:i386
   With an NVIDIA card, also the driver's 32-bit libraries (e.g. libnvidia-gl-<version>:i386)."
else
    NEEDS="Needs: a 64-bit (x86-64) Linux as new as Ubuntu 26.04 (glibc 2.43) with a desktop (its graphics driver and
X11 / Wayland libraries, present on any desktop install), and your own copy of the PC game's data.

1. Nothing to install on a desktop system. On a minimal one (Debian / Ubuntu):
     sudo apt install libgl1 libgl1-mesa-dri libegl1 libx11-6 libxext6 libxcursor1 libxi6 libxrandr2 libxss1 \\
         libxfixes3 libxkbcommon0 libwayland-client0 libwayland-cursor0 libwayland-egl1 libdecor-0-0 libxrender1 \\
         libx11-xcb1 libxcb-render0 libxcb-shm0 libxcb-dri3-0 libgbm1 libdrm2"
fi
cat > "$OUT/README.txt" <<EOF
The Simpsons Hit & Run - Linux port, $ARCH (test build)

$NEEDS
2. Start it:  ./run.sh "/path/to/The Simpsons - Hit & Run"
   (afterwards just ./run.sh; options go after the path or on their own, e.g. ./run.sh skipfe)

Settings: run/simpsons.ini (#System: display, resolution, antialiasing, mipmaps, anisotropy).
Shaders: shaders/ (see shaders/README.md); M in the game turns them off and on.
A crash or problem: send run/logs/last-session.log.
EOF

tar -C "$ROOT/dist" -czf "$ROOT/dist/$NAME.tar.gz" "$NAME"
echo "$OUT"
echo "$ROOT/dist/$NAME.tar.gz ($(du -h "$ROOT/dist/$NAME.tar.gz" | cut -f1)), $(ls "$OUT/lib" | wc -l) libraries"
