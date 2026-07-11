#!/bin/bash
# Build (if needed) and start the game normally: no gdb, no timeout.
# usage: port/play.sh --data DIR [--mod MOD]... [--debug] [--no-build] [game args...]
#   --data DIR   your copy's PC game data dir (the one holding art/, scripts/, *.rcf); required
#   --mod MOD    load a Mod Launcher mod: a .lmlm package (read in place) or an extracted mod folder;
#                repeatable, earlier mods win (port/compat/shar_mods.h)
#   --debug      use the debug build (build-linux-debug, asserts + debug output; no boot movies)
#   --no-build   skip the ninja step
# env: SHAR_RUN   run directory (default run/), set up on first use: symlinks to the data,
#                 private copies of simpsons.ini and Save1-4 (the game writes them)
#      SHAR_BUILD build dir (overrides --debug)
# Other SHAR_* variables (SHAR_FPS_CAP, SHAR_VSYNC, SHAR_NO_AUDIO, ...) pass through to the game.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
RUN=${SHAR_RUN:-$ROOT/run}

DATA=; DEBUG=0; BUILD_STEP=1; MODS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --data) [ $# -ge 2 ] || { echo "--data needs a directory" >&2; exit 1; }; DATA=$2; shift 2 ;;
        --data=*) DATA=${1#--data=}; shift ;;
        --mod) [ $# -ge 2 ] || { echo "--mod needs a folder or .lmlm" >&2; exit 1; }; MODS+=("$2"); shift 2 ;;
        --mod=*) MODS+=("${1#--mod=}"); shift ;;
        --debug) DEBUG=1; shift ;;
        --no-build) BUILD_STEP=0; shift ;;
        *) break ;;
    esac
done
if [ $DEBUG = 1 ]; then
    BUILD=${SHAR_BUILD:-$ROOT/build-linux-debug}; CMAKE_ARGS=(-DSHAR_RAD_DEBUG=ON)
else
    BUILD=${SHAR_BUILD:-$ROOT/build-linux}; CMAKE_ARGS=()
fi

if [ -z "$DATA" ]; then
    echo "usage: port/play.sh --data DIR [--mod MOD]... [--debug] [--no-build] [game args...]" >&2
    echo "  DIR is your copy's PC game data dir (holding art/, scripts/, *.rcf)" >&2; exit 1
fi
[ -d "$DATA" ] || { echo "Game data dir not found: $DATA" >&2; exit 1; }
DATA=$(cd "$DATA" && pwd)   # absolute, for the symlinks

# Run directory
mkdir -p "$RUN"
for f in ambience.rcf carsound.rcf dialog.rcf dialogf.rcf dialogg.rcf dialogs.rcf music00.rcf music01.rcf music02.rcf music03.rcf nis.rcf \
         scripts.rcf soundfx.rcf opening.bnr art movies scripts sound; do
    [ -e "$DATA/$f" ] && [ ! -e "$RUN/$f" ] && ln -s "$DATA/$f" "$RUN/$f"
done
for f in simpsons.ini Save1 Save2 Save3 Save4; do
    [ -e "$DATA/$f" ] && [ ! -e "$RUN/$f" ] && cp "$DATA/$f" "$RUN/$f"
done

# Mods: absolute paths in SHAR_MODS (the game runs from the run dir)
MOD_PATHS=
for m in "${MODS[@]}"; do
    [ -e "$m" ] || { echo "Mod not found: $m" >&2; exit 1; }
    m=$(realpath "$m")
    case "$m" in *:*) echo "Mod paths cannot contain ':': $m" >&2; exit 1 ;; esac
    MOD_PATHS=${MOD_PATHS:+$MOD_PATHS:}$m
done
[ -n "$MOD_PATHS" ] && export SHAR_MODS=$MOD_PATHS

# Build
if [ $BUILD_STEP = 1 ]; then
    [ -f "$BUILD/build.ninja" ] || cmake -S "$ROOT" -B "$BUILD" -G Ninja "${CMAKE_ARGS[@]}"
    echo "Building $BUILD ..."
    ninja -C "$BUILD" -j"$(nproc)" > "$ROOT/port/build.log" 2>&1 || {
        grep -E "error|FAILED" "$ROOT/port/build.log" | head -20; echo "Build failed, see port/build.log" >&2; exit 1; }
fi
[ -x "$BUILD/srr2" ] || { echo "$BUILD/srr2 not found" >&2; exit 1; }

cd "$RUN"
exec "$BUILD/srr2" "$@"
