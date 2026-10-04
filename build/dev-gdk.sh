#!/bin/bash -e
# Development shortcut: copy gdk/riscos into an already built GTK tree in
# $SRC and rebuild it incrementally (ninja), then reinstall. For a clean
# build use build-deps.sh (delete $STAGE/lib/libgtk-3.a first).
#   build/dev-gdk.sh            RISC OS
#   TARGET=linuxarm build/dev-gdk.sh
. "$(dirname "$0")/env.sh"
G=$SRC/gtk+-3.24.41
[ -d "$G/_build" ] || { echo "dev-gdk.sh: no GTK build in $G: run build-deps.sh" >&2; exit 1; }
for f in "$REPO_DIR"/gdk/riscos/*; do
  cmp -s "$f" "$G/gdk/riscos/$(basename "$f")" || cp "$f" "$G/gdk/riscos/"
done
if [ "$TARGET" = linuxarm ]; then
  grep -q fake_riscos.c "$G/gdk/riscos/meson.build" ||
    sed -i "s/  'gdkriscos-server.c',/  'gdkriscos-server.c',\n  'fake_riscos.c',/" "$G/gdk/riscos/meson.build"
fi
env -u CC -u CXX -u AR -u RANLIB -u STRIP -u CFLAGS -u CXXFLAGS -u CPPFLAGS -u LDFLAGS \
  ninja -C "$G/_build" > "$SRC/gtk-dev.log" 2>&1 || { grep -E 'error|FAILED' "$SRC/gtk-dev.log" | head -30; exit 1; }
env -u CC -u CXX -u AR -u RANLIB -u STRIP -u CFLAGS -u CXXFLAGS -u CPPFLAGS -u LDFLAGS \
  meson install -C "$G/_build" --destdir "$SYSROOT" >> "$SRC/gtk-dev.log"
echo "dev-gdk.sh: rebuilt $G"
