#!/bin/bash -e
# !L3afpad: L3afpad 0.8.18.1.11 (GPL v2) on GTK 3 for RISC OS.
#   RCD_DIR=... SRC=... SYSROOT=... build/build-l3afpad.sh
# Unpacks the source into $SRC, applies patches/l3afpad-*-riscos.diff,
# builds with apps/l3afpad/config.h (no configure) and packages
# out/!L3afpad and out/L3afpad-$VERSION.zip (build-app.sh).
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/env.sh"
V=0.8.18.1.11
L=$SRC/l3afpad-$V
rm -rf "$L"
tar -xzf "$DL/l3afpad_$V.orig.tar.gz" -C "$SRC"
(cd "$L" && patch -p1 -s < "$REPO_DIR/patches/l3afpad-$V-riscos.diff")
cp "$REPO_DIR/apps/l3afpad/config.h" "$L/src/"
SOURCES=$(ls "$L"/src/*.c)
# an editor wants a monospaced font (L3afpad's default is "Monospace 12")
exec "$HERE/build-app.sh" L3afpad l3afpad "${VERSION:-0.8.18.1.11-riscos26}" "$REPO_DIR/apps/l3afpad/app" \
  -DHAVE_CONFIG_H -DRISCOS_PORT_VERSION=\"${VERSION:-0.8.18.1.11-riscos26}\" -I"$L/src" -Wno-unused-variable -Wno-unused-but-set-variable \
  $SOURCES "$REPO_DIR/apps/l3afpad/riscos-l3afpad.c"
