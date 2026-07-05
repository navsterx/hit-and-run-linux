#!/bin/bash
# Run the game under valgrind memcheck to find reads of uninitialised memory, then summarise the reports by
# where the memory came from (which allocation / stack frame), most frequent first.
# usage: port/memcheck.sh [seconds] [game args...]          e.g. port/memcheck.sh 330 skipfe
# env:   SHAR_RUN    run directory (default run/; use a scratch one with display=window, resolution=640x480)
#        SHAR_BUILD  build dir (default build-linux: -O0 -g, which keeps memcheck's line numbers exact)
#        VG_TOOLS    where valgrind and its helpers are unpacked (default ~/.cache/shar-memcheck)
# Output: $RUN/memcheck.log (raw), $RUN/memcheck.txt (summary). Other SHAR_* variables pass through to the game,
# e.g. SHAR_KEY_SCRIPT to move around (SDL_GetTicks times; under memcheck Level 1 is playable after ~4 minutes).
#
# Every allocation from the game's heaps and FBMemoryPool is marked undefined (memorymanager.cpp, memorypool.cpp),
# so recycled memory counts as uninitialised too. Known noise: SDL/X11/udev/dbus during window creation, and
# glibc's vector string routines reading past the terminator of strcpy'd buffers (printf/sscanf/snprintf of
# stack buffers, LoadingManager request names).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
RUN=${SHAR_RUN:-$ROOT/run}
BUILD=${SHAR_BUILD:-$ROOT/build-linux}
T=${VG_TOOLS:-$HOME/.cache/shar-memcheck}
SECS=${1:-330}; shift || true

# One-time setup, no root needed: valgrind (ships the x86 tools), and the i386 glibc debug symbols merged into a
# private copy of ld-linux.so.2 (valgrind needs ld.so's strlen symbol; the system one is stripped). The game is
# run as a copy whose ELF interpreter points at that loader.
if [ ! -x "$T/vg/usr/bin/valgrind" ] || [ ! -f "$T/ld-linux.so.2" ]; then
    mkdir -p "$T/deb" && cd "$T/deb"
    apt-get download valgrind libc6-dbg:i386 elfutils patchelf libelf1t64 libdw1t64 > /dev/null
    for d in valgrind_*.deb; do dpkg-deb -x "$d" "$T/vg"; done
    for d in libc6-dbg_*.deb elfutils_*.deb patchelf_*.deb libelf*.deb libdw*.deb; do dpkg-deb -x "$d" "$T/tools"; done
    L=/usr/lib/i386-linux-gnu/ld-linux.so.2
    ID=$(readelf -n "$L" | awk '/Build ID/{print $3}')
    LD_LIBRARY_PATH="$T/tools/usr/lib/x86_64-linux-gnu" "$T/tools/usr/bin/eu-unstrip" -o "$T/ld-linux.so.2" "$L" \
        "$T/tools/usr/lib/debug/.build-id/${ID:0:2}/${ID:2}.debug"
fi

ninja -C "$BUILD" -j"$(nproc)" > "$ROOT/port/build.log" 2>&1 || { grep -E "error|FAILED" "$ROOT/port/build.log" | head -20; exit 1; }
cp "$BUILD/srr2" "$T/srr2"
LD_LIBRARY_PATH="$T/tools/usr/lib/x86_64-linux-gnu" "$T/tools/usr/bin/patchelf" --set-interpreter "$T/ld-linux.so.2" "$T/srr2"

cd "$RUN"
set +e
# nouserintercepts: the game replaces operator new/delete itself; memcheck must not swap in its own
VALGRIND_LIB="$T/vg/usr/libexec/valgrind" SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy} \
timeout -s INT "$SECS" "$T/vg/usr/bin/valgrind" --tool=memcheck --error-limit=no --num-callers=24 --track-origins=yes \
    --leak-check=no --soname-synonyms=somalloc=nouserintercepts --log-file="$RUN/memcheck.log" "$T/srr2" "$@" \
    > "$RUN/memcheck.out" 2>&1
python3 "$ROOT/port/memcheck_summary.py" "$RUN/memcheck.log" > "$RUN/memcheck.txt"
grep "^###" "$RUN/memcheck.txt" | cut -c1-200
echo "summary: $RUN/memcheck.txt   raw: $RUN/memcheck.log"
