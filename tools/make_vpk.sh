#!/bin/sh
# make_vpk.sh - the game as a VectorPie package: build/vdefender-<version>.vpk
#
# Run from the project folder after the Pi build (BUILD.md). The package holds
# the Pi program, game.ini, leds.cfg and run.sh: the game is the program
# itself (translated from Defender's source), it needs no ROM files. The
# cabinet's menu installs it from a USB drive's games/ folder or from
# /persistent/games/packages/ (Settings -> GAMES -> INSTALL GAMES). save/
# (settings, the game's CMOS with its high scores) is kept on an update.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
build="$root/build/pi"
stage="$root/build/pkg/vdefender"
[ -f "$build/vdefender" ] || { echo "make_vpk: no Pi build in $build" >&2; exit 1; }
rm -rf "$(dirname "$stage")"
mkdir -p "$stage"
cp "$build/vdefender" "$root/package/game.ini" "$root/package/leds.cfg" "$root/package/run.sh" "$stage/"
[ -f "$root/package/marquee.png" ] && cp "$root/package/marquee.png" "$stage/"
version=$(sed -n 's/^version[[:space:]]*=[[:space:]]*\([0-9.]*\).*/\1/p' "$stage/game.ini" | head -n 1)
out=$(mktemp -d)
sh "$root/vendor/VectorPie/sdk/tools/vp-pack/vp-pack.sh" "$stage" "$out"
vpk="$root/build/vdefender-$version.vpk"
mv -f "$out/vdefender-$version.vpk" "$vpk"
rmdir "$out"

# Built on Windows the programs in the archive are not marked as such: the
# file system there has no such mark for vp-pack's chmod to set. Mark them.
python - "$vpk" <<'PY'
import io, sys, tarfile
path = sys.argv[1]
out = io.BytesIO()
with tarfile.open(path, "r:gz") as old, tarfile.open(fileobj=out, mode="w:gz") as new:
    for member in old.getmembers():
        if member.name in ("vdefender/vdefender", "vdefender/run.sh"):
            member.mode = 0o755
        new.addfile(member, old.extractfile(member) if member.isfile() else None)
open(path, "wb").write(out.getvalue())
PY
echo "make_vpk: $vpk"
