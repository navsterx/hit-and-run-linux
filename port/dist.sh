#!/bin/bash
# Refresh the 'dist' branch: a binary-only, bring-your-own-game release. It is an orphan branch (no history shared with
# dev/main, so no source is reachable from it) holding, per architecture, port/package.sh's bundle with the game
# stripped, plus README.md (from port/dist_README.md). The unstripped binaries go to dist/symbols/ (by build ID) for
# decoding crash reports. Makes one commit on 'dist'; the user pushes.
# usage: port/dist.sh [--no-build]     (--no-build: use the newest existing bundles in dist/)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
if [ "$1" != --no-build ]; then
    port/package.sh --64 > /dev/null
    port/package.sh > /dev/null
fi

WT=$(mktemp -d)
if git rev-parse -q --verify refs/heads/dist > /dev/null; then
    git worktree add -q "$WT" dist
else
    git worktree add -q --orphan -b dist "$WT"
fi
trap 'git -C "$ROOT" worktree remove --force "$WT"' EXIT
git -C "$WT" rm -rqf --ignore-unmatch .

mkdir -p dist/symbols
for ARCH in x86_64 i386; do
    SRC=$(ls -d dist/shar-linux-$ARCH-[0-9]* | grep -v '\.tar\.gz$' | sort | tail -1)
    [ -n "$SRC" ] || { echo "no $ARCH bundle in dist/" >&2; exit 1; }
    cp -r "$SRC" "$WT/$ARCH"
    rm -rf "$WT/$ARCH/run"
    strip "$WT/$ARCH/srr2"
    ID=$(readelf -n "$SRC/srr2" | awk '/Build ID/ { print $3 }')
    cp "$SRC/srr2" "dist/symbols/srr2-$ARCH-$ID"
    echo "$ARCH: $(basename "$SRC"), build ID $ID"
done
cp port/dist_README.md "$WT/README.md"
printf '*/run/\n' > "$WT/.gitignore"

git -C "$WT" add -A
git -C "$WT" -c user.name=zeropaulsy -c user.email=13328895+zeropaulsy@users.noreply.github.com \
    commit -q -m "Release $(date +%Y-%m-%d) (dev $(git rev-parse --short HEAD))"
git log -1 --oneline refs/heads/dist --
