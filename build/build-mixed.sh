#!/bin/bash -e
# The mixed-link test (riscos-crossdev v1.11-test1, branch mixed-link-test):
# the GTK stack as shared libraries, and Geany linked against them with
# UnixLib, libstdc++ and libgcc still linked in.
#
#   RCD_DIR=<crossdev v1.11-test1> RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-deps.sh
#   RCD_DIR=... RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-mixed.sh libs
#   RCD_DIR=... RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-mixed.sh geany
#   RCD_DIR=... RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-mixed.sh package
#
# libs: each static library build-deps.sh made (position-independent, as
# RISCOS_GTK_MIXED=1 builds them) becomes $STAGE/so/libX.so.N: its whole
# archive, linked -shared -nostdlib (no UnixLib of its own: the program's
# is used) against the libraries before it in LIBS, --as-needed so each
# NEEDS only what it uses. checks/check-shared-lib.sh checks each.
# geany: Geany's objects (build-geany.sh's, rebuilt with this toolchain)
# linked -rdynamic against the .so files; packaged as out/mixed/!Geany
# with the libraries in !Geany.lib under their SONAMEs.
# package: out/mixed/!Geany from the static out/!Geany (build-geany.sh's:
# its data, fonts, theme): the program as ELF (geany,e1f, stripped),
# the libraries stripped into lib, PThreadTicker from this toolchain, and
# !Run setting geany$LD$Env; out/mixed/Geany-2.1-mixed-test1.zip.
HERE=$(cd "$(dirname "$0")" && pwd)
[ "${RISCOS_GTK_MIXED:-}" = 1 ] || { echo "build-mixed.sh: needs RISCOS_GTK_MIXED=1" >&2; exit 1; }
. "$HERE/env.sh"
SO=$STAGE/so
# archive SONAME, in link order (each may use the ones before it)
LIBS="
libz z.so.1
libffi ffi.so.8
libpcre2-8 pcre2-8.so.0
libintl intl.so.8
libglib-2.0 glib-2.0.so.0
libgobject-2.0 gobject-2.0.so.0
libgmodule-2.0 gmodule-2.0.so.0
libgio-2.0 gio-2.0.so.0
libpng16 png16.so.16
libjpeg jpeg.so.62
libfreetype freetype.so.6
libexpat expat.so.1
libfontconfig fontconfig.so.1
libfribidi fribidi.so.0
libharfbuzz harfbuzz.so.0
libpixman-1 pixman-1.so.0
libcairo cairo.so.2
libcairo-gobject cairo-gobject.so.2
libpango-1.0 pango-1.0.so.0
libpangoft2-1.0 pangoft2-1.0.so.0
libpangocairo-1.0 pangocairo-1.0.so.0
libgdk_pixbuf-2.0 gdk_pixbuf-2.0.so.0
libatk-1.0 atk-1.0.so.0
libepoxy epoxy.so.0
libgdk-3 gdk-3.so.0
libgtk-3 gtk-3.so.0
"

libs() {
  rm -rf "$SO"; mkdir -p "$SO"
  local before="" a so name
  while read -r a so; do
    [ -n "$a" ] || continue
    name=lib${so%%.so.*}
    # libepoxy's dlopen comes from riscos/dl's stub (always fails)
    extra=""; [ "$a" = libepoxy ] && extra="$STAGE/lib/libdl.a"
    $CC $RO_CFLAGS -shared -nostdlib -Wl,-soname,lib$so -o "$SO/lib$so" \
      -Wl,--whole-archive "$STAGE/lib/$a.a" -Wl,--no-whole-archive $extra \
      -L"$SO" -Wl,--as-needed $before -Wl,--no-as-needed -lgcc
    ln -sf "lib$so" "$SO/$name.so"
    "$RCD_DIR/checks/check-shared-lib.sh" "$SO/lib$so"
    before="$before -l${name#lib}"
  done <<< "$LIBS"
  ls -la "$SO"
}

geany() {
  V=2.1 G=$SRC/geany-2.1
  # Geany's own objects and libraries, built with this toolchain (static
  # build's steps: build-geany.sh, up to its link)
  if [ ! -f "$G/src/libgeany-riscos.a" ]; then
    sed -n '/^if \[ -z "${GEANY_RELINK:-}" \]; then/,/^fi$/p' "$HERE/build-geany.sh" > "$SRC/geany-steps.sh"
    (G=$G V=$V . "$SRC/geany-steps.sh")
  fi
  local OUTD=$REPO_DIR/out/mixed
  mkdir -p "$OUTD"
  local libs="" a so
  while read -r a so; do [ -n "$a" ] && libs="$libs -l${so%%.so.*}"; done <<< "$LIBS"
  "$SRC/.wrap/$HOST-g++" $RO_CFLAGS -Wall -Wno-deprecated-declarations -DHAVE_CONFIG_H -DGTK -DGEANY_PRIVATE \
    -DRISCOS_PORT_VERSION=\"2.1-mixed-test1\" \
    -I"$G" -I"$G/src" -I"$G/src/tagmanager" -I"$G/scintilla/include" -I"$G/scintilla/lexilla/include" \
    $(pkg-config --cflags gtk+-3.0) -x c "$REPO_DIR/apps/geany/riscos-geany.c" -x none \
    "$G/src/libgeany-riscos.a" "$G/src/tagmanager/.libs/libtagmanager.a" "$G/ctags/.libs/libctags.a" \
    "$G/ctags/.libs/libgnu_regex.a" "$G/scintilla/.libs/libscintilla.a" "$G/scintilla/.libs/liblexilla.a" \
    -o "$OUTD/geany" -rdynamic -L"$SO" $libs -lstdc++
  "$RCD_DIR/checks/check-all.sh" "$OUTD/geany"
  ls -la "$OUTD/geany"
}

package() {
  local OUTD=$REPO_DIR/out/mixed A=$REPO_DIR/out/mixed/!Geany f
  [ -d "$REPO_DIR/out/!Geany" ] || { echo "build-mixed.sh: build the static !Geany first (build-geany.sh)" >&2; exit 1; }
  rm -rf "$A"; cp -r "$REPO_DIR/out/!Geany" "$A"; rm -f "$A/geany,ff8"
  $STRIP -o "$A/geany,e1f" "$OUTD/geany"
  mkdir -p "$A/lib"
  for f in "$SO"/*.so.*; do $STRIP --strip-unneeded -o "$A/lib/$(basename "$f"),e1f" "$f"; done
  cp "$GCCSDK_ENV/riscos/PThrTicker,ffa" "$A/"
  python3 - "$A/!Run,feb" <<'PYEOF'
import sys
p = sys.argv[1]; s = open(p).read()
s = s.replace("""| A UnixLib program converted to Absolute (AIF), statically linked with GTK
| 3.24.52 and riscos-gtk's RISC OS GDK backend: it needs SharedUnixLibrary,
| ARMEABISupport and Iconv (PackMan). It runs as a normal Wimp task.""", """| MIXED-LINK TEST (riscos-crossdev 1.11-test1): an ELF program with UnixLib
| linked in, and GTK 3.24.52, riscos-gtk's GDK backend and the libraries
| under them as shared libraries in <Geany$Dir>.lib, loaded by !SharedLibs
| (SOManager and ld-riscos/so/2). Needs SharedUnixLibrary 1.16,
| ARMEABISupport, Iconv, and a !SharedLibs with ld-riscos/so/2 (Iris's).""")
s = s.replace("| The font cache and the Log", 'If "<SharedLibs$Dir>" = "" Then Error Geany (mixed test) needs !SharedLibs with ld-riscos/so/2: let the Filer see it first\n| The font cache and the Log', 1)
import re
s = re.sub(r"\| The WimpSlot holds the program; the C heap grows it as needed \(no\n\| dynamic area, so no logical address space is reserved\)\.\nWimpSlot -min \d+K -max \d+K",
  "| The WimpSlot holds the program (the shared libraries are loaded by\n| SOManager); the C heap grows it as needed.\nWimpSlot -min 11000K -max 11000K\n"
  "| The shared libraries: SOManager reads <program>$LD$Env for \"geany\"\n"
  "Set GeanyLibs$Path <Geany$Dir>.lib.\nSet geany$LD$Env \"LD_LIBRARY_PATH=/GeanyLibs:\"", s)
open(p, "w").write(s)
PYEOF
  grep -q 'geany\$LD\$Env' "$A/!Run,feb" || { echo "build-mixed.sh: !Run not changed" >&2; exit 1; }
  rm -f "$OUTD/Geany-2.1-mixed-test1.zip"
  python3 "$RCD_DIR/package/rozip.py" "$OUTD/Geany-2.1-mixed-test1.zip" -C "$OUTD" "!Geany"
  ls -la "$OUTD/Geany-2.1-mixed-test1.zip"
}

case "${1:-}" in
  libs) libs ;;
  geany) geany ;;
  package) package ;;
  *) echo "usage: build-mixed.sh libs|geany|package" >&2; exit 1 ;;
esac
