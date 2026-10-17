#!/bin/bash -e
# mkmime.sh DIR - the freedesktop.org MIME database (shared-mime-info 2.4)
# in DIR (an application's share/mime), so GIO knows a file's type from its
# name (g_content_type_guess: *.xlsx is a spreadsheet, *.png a picture),
# as on Linux. Programs choose what to do with a file, and file choosers
# filter, by these types. Without it every file was
# application/octet-stream.
# Only what GLib's xdgmime reads without a cache: globs2, magic, aliases,
# subclasses, icons. No mime.cache (xdgmime would mmap it: on RISC OS
# that's a dynamic area) and no per-type descriptions. Needs the build
# machine's update-mime-database (shared-mime-info). MIT licence.
REPO_DIR=$(cd "$(dirname "$0")/.." && pwd)
: "${DL:=$REPO_DIR/dl}"
OUT=$1
[ -n "$OUT" ] || { echo "usage: mkmime.sh DIR" >&2; exit 1; }
command -v update-mime-database > /dev/null || { echo "mkmime.sh: needs update-mime-database (apt install shared-mime-info)" >&2; exit 1; }
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
tar -xjf "$DL/shared-mime-info_2.4.orig.tar.bz2" -C "$W" \
  shared-mime-info-2.4/data/freedesktop.org.xml.in shared-mime-info-2.4/COPYING
mkdir -p "$W/db/packages"
cp "$W/shared-mime-info-2.4/data/freedesktop.org.xml.in" "$W/db/packages/freedesktop.org.xml"
update-mime-database -n "$W/db" > /dev/null 2>&1
rm -rf "$OUT"; mkdir -p "$OUT"
for f in globs2 magic aliases subclasses generic-icons icons types; do
  cp "$W/db/$f" "$OUT/$f"
done
cp "$W/shared-mime-info-2.4/COPYING" "$OUT/Licence,fff"
grep -q ':\*\.xlsx$' "$OUT/globs2"
