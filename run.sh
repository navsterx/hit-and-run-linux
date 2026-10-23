#!/bin/bash
# Start the game. usage: ./run.sh /path/to/game-data [--mod MOD]... [game options...]
# game-data is your copy's PC data folder (holding art/, scripts/ and the .rcf files). The first time, a run/ folder
# is made next to this script with links to the data and copies of simpsons.ini and the saves (the game writes
# those); the shaders/ folder here is used too. Console output goes to run/logs/last-session.log.
# --mod MOD loads a Mod Launcher mod: a .lmlm file or an extracted mod folder. Repeat it for several; when two mods
# change the same file, the one given first wins.
HERE=$(cd "$(dirname "$0")" && pwd)
[ -e "$HERE/lib/.unpacked" ] && [ ! "$HERE/ffmpeg-libs.tar.xz" -nt "$HERE/lib/.unpacked" ] || "$HERE/setup.sh" || exit 1
RUN=$HERE/run
MODS=; ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --mod) [ $# -ge 2 ] || { echo "--mod needs a .lmlm file or a mod folder" >&2; exit 1; }; M=$2; shift 2 ;;
        --mod=*) M=${1#--mod=}; shift ;;
        *) ARGS+=("$1"); shift; continue ;;
    esac
    [ -e "$M" ] || { echo "Mod not found: $M" >&2; exit 1; }
    M=$(cd "$(dirname "$M")" && pwd)/$(basename "$M")
    case "$M" in *:*) echo "Mod paths cannot contain ':': $M" >&2; exit 1 ;; esac
    MODS=${MODS:+$MODS:}$M
done
set -- "${ARGS[@]}"
[ -n "$MODS" ] && export SHAR_MODS=$MODS
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
"$HERE/Simpsons" "$@" 2>&1 | tee "$RUN/logs/last-session.log"
