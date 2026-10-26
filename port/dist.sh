#!/bin/bash
# Refresh the 'dist-linux-x86_64' branch: a binary-only, bring-your-own-game release of the x86-64 build. It is an
# orphan branch (no history shared with dev/main, so no source is reachable from it) holding port/package.sh --64 --noble's
# bundle with the game stripped. Only SDL2 and its libraries stay loose in lib/; ffmpeg (movies) and the rest go in
# ffmpeg-libs.tar.xz, which setup.sh unpacks (run.sh calls it on first start). README.md comes from port/dist_README.md.
# The binary is renamed Simpsons; the unstripped one goes to dist/symbols/x86_64-<build ID>/Simpsons (same basename:
# crash reports name the module, and port/crash_symbolize.py matches on it). The 32-bit build is not on the branch: hand
# out port/package.sh's i386 tarball separately. Makes one commit on that branch; the user pushes.
# usage: port/dist.sh [--no-build]     (--no-build: use the newest existing x86-64 bundle in dist/)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
BRANCH=dist-linux-x86_64
[ "$1" = --no-build ] || port/package.sh --64 --noble > /dev/null
SRC=$(ls -d dist/shar-linux-x86_64-[0-9]* | grep -v '\.tar\.gz$' | sort | tail -1)
[ -n "$SRC" ] || { echo "no x86_64 bundle in dist/" >&2; exit 1; }

WT=$(mktemp -d)
if git rev-parse -q --verify refs/heads/$BRANCH > /dev/null; then
    git worktree add -q "$WT" "$BRANCH"
else
    git worktree add -q --orphan -b "$BRANCH" "$WT"
fi
trap 'git -C "$ROOT" worktree remove --force "$WT"' EXIT
git -C "$WT" rm -rqf --ignore-unmatch .

cp "$SRC/srr2" "$WT/Simpsons"
cp "$SRC/run.sh" "$WT/"
cp -r "$SRC/shaders" "$WT/shaders"
strip "$WT/Simpsons"
ID=$(readelf -n "$SRC/srr2" | awk '/Build ID/ { print $3 }')
mkdir -p "dist/symbols/x86_64-$ID"
cp "$SRC/srr2" "dist/symbols/x86_64-$ID/Simpsons"

# lib/: SDL2 and the bundled libraries it needs; everything else (ffmpeg and its codecs) into the tarball
mkdir -p "$WT/lib"
KEEP="libSDL2-2.0.so.0 $(LD_LIBRARY_PATH=$SRC/lib ldd "$SRC/lib/libSDL2-2.0.so.0" | awk -v d="$SRC/lib/" 'index($3, d) == 1 { print $1 }')"
REST=
for f in $(ls "$SRC/lib"); do
    if echo " $KEEP " | tr '\n' ' ' | grep -q " $f "; then cp "$SRC/lib/$f" "$WT/lib/"; else REST="$REST $f"; fi
done
# reproducible (fixed order, times and owners), so an unchanged set of libraries adds nothing to the branch
tar -C "$SRC/lib" --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -cf - $REST | xz -9 -T1 > "$WT/ffmpeg-libs.tar.xz"

cat > "$WT/setup.sh" <<'EOF'
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
EOF
chmod +x "$WT/setup.sh"
# run.sh: set up on first start
sed -i 's|^RUN=\$HERE/run$|[ -e "$HERE/lib/.unpacked" ] \&\& [ ! "$HERE/ffmpeg-libs.tar.xz" -nt "$HERE/lib/.unpacked" ] \|\| "$HERE/setup.sh" \|\| exit 1\n&|' "$WT/run.sh"
grep -q setup.sh "$WT/run.sh" || { echo "run.sh: setup hook not inserted" >&2; exit 1; }
sed -i 's|"\$HERE/srr2"|"$HERE/Simpsons"|' "$WT/run.sh"
grep -qF '"$HERE/Simpsons"' "$WT/run.sh" && ! grep -q srr2 "$WT/run.sh" || { echo "run.sh: binary not renamed" >&2; exit 1; }

cp port/dist_README.md "$WT/README.md"
# run/ and the unpacked libraries are the player's; lib/'s own files are tracked regardless (added with -f)
printf '/run/\n/lib/\n' > "$WT/.gitignore"

git -C "$WT" add -A
git -C "$WT" add -f lib
git -C "$WT" -c user.name=zeropaulsy -c user.email=13328895+zeropaulsy@users.noreply.github.com \
    commit -q -m "Release $(date +%Y-%m-%d) (dev $(git rev-parse --short HEAD))"
echo "x86_64: $(basename "$SRC"), build ID $ID, lib/ $(ls "$WT/lib" | wc -l) files, ffmpeg-libs.tar.xz $(du -h "$WT/ffmpeg-libs.tar.xz" | cut -f1)"
git log -1 --oneline "refs/heads/$BRANCH" --
