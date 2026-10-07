#!/bin/bash -e
# Development shortcut: copy riscos-gtk's cairo additions (cairo/) into an
# already built cairo in $SRC, rebuild it (ninja) and reinstall. Fresh
# builds get them from patches/cairo-1.18.0-riscos.diff (build/mkcairopatch.sh).
#   build/dev-cairo.sh            RISC OS
#   TARGET=linuxarm build/dev-cairo.sh
. "$(dirname "$0")/env.sh"
meson_env() { env -u CC -u CXX -u AR -u RANLIB -u STRIP -u CFLAGS -u CXXFLAGS -u CPPFLAGS -u LDFLAGS "$@"; }
C=$SRC/cairo-1.18.0
[ -d "$C/_build" ] || { echo "dev-cairo.sh: no cairo build in $C: run build-deps.sh" >&2; exit 1; }
for f in "$REPO_DIR"/cairo/*; do
  cmp -s "$f" "$C/src/$(basename "$f")" || cp "$f" "$C/src/"
done
grep -q cairo-riscos-print.c "$C/src/meson.build" || {
  sed -i "s/  'cairo-recording-surface.c',/  'cairo-recording-surface.c',\n  'cairo-riscos-print.c',/" "$C/src/meson.build"
  sed -i "s/^  'cairo.h',/  'cairo.h',\n  'cairo-riscos.h',/" "$C/src/meson.build"
}
meson_env ninja -C "$C/_build" > "$SRC/cairo-dev.log" 2>&1 || { grep -E 'error|FAILED' "$SRC/cairo-dev.log" | head -30; exit 1; }
meson_env meson install -C "$C/_build" --destdir "$SYSROOT" >> "$SRC/cairo-dev.log"
echo "dev-cairo.sh: rebuilt $C"
