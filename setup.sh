#!/bin/bash
# Get the release ready to run: unpack ffmpeg-libs.tar.xz (the movie player's libraries) into lib/ and check that the
# system has the graphics and windowing libraries the game needs. run.sh calls this on first start; safe to rerun.
HERE=$(cd "$(dirname "$0")" && pwd)
if [ ! -e "$HERE/lib/.unpacked" ] || [ "$HERE/ffmpeg-libs.tar.xz" -nt "$HERE/lib/.unpacked" ]; then
    echo "Unpacking libraries..."
    tar -C "$HERE/lib" -xJf "$HERE/ffmpeg-libs.tar.xz" || { echo "setup: could not unpack ffmpeg-libs.tar.xz" >&2; exit 1; }
    touch "$HERE/lib/.unpacked"
fi
chmod +x "$HERE/Simpsons" "$HERE/run.sh"
MISSING=$(LD_LIBRARY_PATH="$HERE/lib" ldd "$HERE/Simpsons" "$HERE/lib/libSDL2-2.0.so.0" 2>/dev/null | awk '/not found/ { print $1 }' | sort -u)
if [ -n "$MISSING" ]; then
    echo "setup: these system libraries are missing:" $MISSING >&2
    echo "On Debian / Ubuntu:  sudo apt install libgl1 libgl1-mesa-dri libegl1 libx11-6 libxext6 libxcursor1 libxi6 \\" >&2
    echo "    libxrandr2 libxss1 libxfixes3 libxkbcommon0 libwayland-client0 libwayland-cursor0 libwayland-egl1 \\" >&2
    echo "    libdecor-0-0 libxrender1 libx11-xcb1 libxcb-render0 libxcb-shm0 libxcb-dri3-0 libgbm1 libdrm2" >&2
    exit 1
fi
echo "Ready: ./run.sh \"/path/to/The Simpsons - Hit & Run\""
