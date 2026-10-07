#!/bin/bash -e
# Regenerate patches/cairo-1.18.0-riscos.diff from cairo/ (riscos-gtk's
# additions to cairo: cairo-riscos.h, cairo-riscos-print.c) and the two
# meson.build lines that build and install them.
HERE=$(cd "$(dirname "$0")" && pwd); REPO_DIR=$(dirname "$HERE")
DL=${DL:-$REPO_DIR/dl}
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
tar -xf "$DL/cairo_1.18.0.orig.tar.xz" -C "$T" cairo-1.18.0/src/meson.build
mkdir -p "$T/a/src" "$T/b/src"
cp "$T/cairo-1.18.0/src/meson.build" "$T/a/src/"
cp "$T/cairo-1.18.0/src/meson.build" "$T/b/src/"
sed -i "s/  'cairo-recording-surface.c',/  'cairo-recording-surface.c',\n  'cairo-riscos-print.c',/" "$T/b/src/meson.build"
sed -i "s/^  'cairo.h',/  'cairo.h',\n  'cairo-riscos.h',/" "$T/b/src/meson.build"
cp "$REPO_DIR"/cairo/* "$T/b/src/"
( cd "$T" && diff -ruN a b ) > "$REPO_DIR/patches/cairo-1.18.0-riscos.diff" || true
echo "mkcairopatch.sh: $(grep -c '^+++' "$REPO_DIR/patches/cairo-1.18.0-riscos.diff") files in patches/cairo-1.18.0-riscos.diff"
