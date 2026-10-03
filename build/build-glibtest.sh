#!/bin/bash -e
# Build tests/glib/glibtest.c for RISC OS against $STAGE, run riscos-crossdev's
# checks on it (stack probes, FPU level, UnixLib consistency, unaligned casts
# in the source) and package out/GLibTest-$VERSION.zip for the Pi.
. "$(dirname "$0")/env.sh"
VERSION=${VERSION:-0.1}
OUT=$REPO_DIR/out
T=$REPO_DIR/tests/glib
mkdir -p "$OUT"
$CC $RO_CFLAGS -Wall -Wextra -Wno-unused-parameter $(pkg-config --cflags gio-2.0) \
  "$T/glibtest.c" -o "$OUT/glibtest" -static $(pkg-config --libs --static gio-2.0)
"$RCD_DIR/checks/check-all.sh" "$OUT/glibtest"
"$RCD_DIR/checks/check-unaligned.sh" $(pkg-config --cflags gio-2.0) "$T/glibtest.c"
A=$OUT/!GLibTest
rm -rf "$A"; mkdir -p "$A"
$STRIP -o "$OUT/glibtest.stripped" "$OUT/glibtest"
"$GCCSDK_ENV/bin/elf2aif" -e "$OUT/glibtest.stripped" "$A/glibtest,ff8" > /dev/null
cp "$T"/app/* "$A/"
cp "$GCCSDK_ENV"/riscos/PThrTicker,ffa "$GCCSDK_ENV"/riscos/PThreadTicker-ReadMe,fff \
   "$GCCSDK_ENV"/riscos/PThreadTicker-Licence,fff "$A/"
rm -f "$OUT/GLibTest-$VERSION.zip"
python3 "$RCD_DIR/package/rozip.py" "$OUT/GLibTest-$VERSION.zip" -C "$OUT" '!GLibTest'
# Nothing from the build machine may end up in what's shipped.
if strings "$A/glibtest,ff8" | grep -q -e "$REPO_DIR" -e "$(cd "$RCD_DIR" && pwd -P)" -e "$SYSROOT" -i -e claude; then
  echo "build-glibtest.sh: build paths found in glibtest,ff8" >&2; exit 1
fi
sha256sum "$OUT/GLibTest-$VERSION.zip"
