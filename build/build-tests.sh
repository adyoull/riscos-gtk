#!/bin/bash -e
# Build the M1 tests for RISC OS against $STAGE (tests/glib/glibtest.c and
# tests/render/rendertest.c), run riscos-crossdev's checks on each (stack
# probes, FPU level, UnixLib consistency, unaligned casts in the source) and
# package out/GTKTest-$VERSION.zip for the Pi: both programs, DejaVu Sans
# with a fonts.conf, and PThreadTicker.
. "$(dirname "$0")/env.sh"
[ "$TARGET" = riscos ] || { echo "build-tests.sh: RISC OS only (TARGET=riscos)" >&2; exit 1; }
VERSION=${VERSION:-0.6}
OUT=$REPO_DIR/out
A=$OUT/!GTKTest
mkdir -p "$OUT"
rm -rf "$A"; mkdir -p "$A/fonts"

prog() {  # NAME SOURCE PKGS...
  local n=$1 s=$2; shift 2
  $CC $RO_CFLAGS -Wall -Wextra -Wno-unused-parameter $(pkg-config --cflags "$@") \
    "$s" -o "$OUT/$n" -static $(pkg-config --libs --static "$@")
  "$RCD_DIR/checks/check-all.sh" "$OUT/$n"
  "$RCD_DIR/checks/check-unaligned.sh" $(pkg-config --cflags "$@") "$s"
  $STRIP -o "$OUT/$n.stripped" "$OUT/$n"
  "$GCCSDK_ENV/bin/elf2aif" -e "$OUT/$n.stripped" "$A/$n,ff8" > /dev/null
  # Nothing from the build machine may end up in what's shipped.
  if strings "$A/$n,ff8" | grep -q -e "$REPO_DIR" -e "$(cd "$RCD_DIR" && pwd -P)" -e "$SYSROOT" -i -e claude; then
    echo "build-tests.sh: build paths found in $n,ff8" >&2; exit 1
  fi
}
prog glibtest "$REPO_DIR/tests/glib/glibtest.c" gio-2.0
prog rendertest "$REPO_DIR/tests/render/rendertest.c" pangocairo gdk-pixbuf-2.0

cp "$REPO_DIR"/tests/app/* "$A/"
cp "$GCCSDK_ENV"/riscos/PThrTicker,ffa "$GCCSDK_ENV"/riscos/PThreadTicker-ReadMe,fff \
   "$GCCSDK_ENV"/riscos/PThreadTicker-Licence,fff "$A/"
unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf' \
  'dejavu-fonts-ttf-2.37/LICENSE' -d "$A/fonts"
mv "$A/fonts/LICENSE" "$A/fonts/Licence,fff"
"$REPO_DIR/tests/render/mkfontsconf.sh" '/<GTKTest$Dir>/fonts' '/<Wimp$ScrapDir>/GTKTest/fontcache' \
  > "$A/fonts/fonts.conf"
# fonts.conf must be well-formed XML (RISC OS paths carry < and >).
python3 -c 'import sys, xml.dom.minidom as m; m.parse(sys.argv[1])' "$A/fonts/fonts.conf"
rm -f "$OUT/GTKTest-$VERSION.zip"
python3 "$RCD_DIR/package/rozip.py" "$OUT/GTKTest-$VERSION.zip" -C "$OUT" '!GTKTest'
sha256sum "$OUT/GTKTest-$VERSION.zip"
