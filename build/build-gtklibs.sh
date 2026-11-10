#!/bin/bash -e
# !GTKLibs: the GTK stack as shared libraries, for riscos-gtk's programs,
# which are linked "mixed" (riscos-crossdev 1.11, docs/MIXED.md there):
# UnixLib, libstdc++ and each program's own libraries are linked into the
# program; GTK 3.24.52 with the RISC OS backend and the libraries under it
# are shared, one copy for every program, loaded by !SharedLibs (SOManager
# and ld-riscos/so/2).
#   RCD_DIR=... RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-deps.sh
#   RCD_DIR=... RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-gtklibs.sh
# build-deps.sh builds the stack position-independent (RISCOS_GTK_MIXED=1:
# env.sh). Here each archive becomes $STAGE/so/libX.so.N: linked -shared
# -nostdlib (no UnixLib of its own: the program's is used) against the
# libraries before it in LIBS, --as-needed, so each NEEDS only what it
# uses; checks/check-shared-lib.sh checks each. Then out/mixed/!GTKLibs
# (the libraries stripped, under their SONAMEs) and
# out/mixed/GTKLibs-$GTKLIBS_VERSION.zip: Resources.!GTKLibs, and
# Resources.!SharedLibs with the ld-riscos/so/2 loader and SOManager 3.04
# (riscos/sharedlibs-so2) to add to the user's !SharedLibs; PackMan
# installs both into !Boot.Resources. The programs (build-app.sh
# with RISCOS_GTK_MIXED=1) link against $STAGE/so and their !Run point
# <program>$LD$Env at !GTKLibs (tools/mixedrun.py).
HERE=$(cd "$(dirname "$0")" && pwd)
[ "${RISCOS_GTK_MIXED:-}" = 1 ] || { echo "build-gtklibs.sh: needs RISCOS_GTK_MIXED=1" >&2; exit 1; }
. "$HERE/env.sh"
SO=$STAGE/so
# !GTKLibs' version: GTK's, then riscos-gtk's build number (RC17 was build
# 17; RC18 is 18)
GTKLIBS_VERSION=${GTKLIBS_VERSION:-3.24.52-18}
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

gtklibs() {
  local G=$REPO_DIR/out/mixed/!GTKLibs f B=$REPO_DIR/out/mixed/gtklibs-zip
  rm -rf "$G"; mkdir -p "$G/lib"
  for f in "$SO"/*.so.*; do $STRIP --strip-unneeded -o "$G/lib/$(basename "$f"),e1f" "$f"; done
  for f in '!Boot,feb' '!Run,feb'; do
    cat > "$G/$f" <<EOF
| !GTKLibs $GTKLIBS_VERSION: GTK 3.24.52, riscos-gtk's RISC OS backend and
| the libraries under them, as shared libraries for riscos-gtk's programs
| (loaded by !SharedLibs: SOManager and ld-riscos/so/2)
Set GTKLibs\$Dir <Obey\$Dir>
Set GTKLibs\$Version $GTKLIBS_VERSION
Set GTKLibs\$Path <GTKLibs\$Dir>.lib.
IconSprites <GTKLibs\$Dir>.!Sprites
EOF
  done
  cat > "$G/!Help,fff" <<EOF
!GTKLibs $GTKLIBS_VERSION
==========$(echo "$GTKLIBS_VERSION" | tr -c '\n' '=')

The shared libraries for riscos-gtk's programs (AbiWord, Gnumeric,
ClawsMail, Geany, L3afpad, mtPaint...): GTK 3.24.52 with riscos-gtk's
RISC OS backend, and GLib, cairo, pango, harfbuzz, FreeType, fontconfig,
pixman, libpng, libjpeg and the rest under them. Every GTK program uses
this one copy, loaded by !SharedLibs (SOManager and ld-riscos/so/2), so
running several costs GTK's memory once.

It lives in !Boot.Resources (PackMan installs it there). It needs a
!SharedLibs with ld-riscos/so/2 in !Boot.Resources too. Its version is
GTK's, then riscos-gtk's build number: $GTKLIBS_VERSION, set in
GTKLibs\$Version when it's booted.

Built from riscos-gtk commit $(git -C "$REPO_DIR" rev-parse --short=12 HEAD) with riscos-crossdev.
The libraries are LGPL 2.1 and MIT-style: LicenceLGPL. Their complete
source is riscos-gtk's release source: https://github.com/adyoull/riscos-gtk
EOF
  # its icon: a shelf of library volumes (tools/mkgtklibsicon.py)
  python3 -B "$REPO_DIR/tools/mkgtklibsicon.py" "$G"
  cp "$REPO_DIR/riscos/licences/LGPL-2.1.txt" "$G/LicenceLGPL,fff"
  rm -rf "$B"; mkdir -p "$B/Resources"; cp -r "$G" "$B/Resources/"
  # and what the user's !SharedLibs is missing for these programs: the
  # ld-riscos/so/2 loader and SOManager 3.04 (riscos/sharedlibs-so2),
  # added to Boot.Resources.!SharedLibs (the package depends on SharedLibs)
  mkdir -p "$B/Resources/!SharedLibs/lib"
  cp "$REPO_DIR/riscos/sharedlibs-so2/SOManager,ffa" "$B/Resources/!SharedLibs/"
  cp "$REPO_DIR/riscos/sharedlibs-so2/lib/"* "$B/Resources/!SharedLibs/lib/"
  rm -f "$REPO_DIR/out/mixed/GTKLibs-$GTKLIBS_VERSION.zip"
  python3 "$RCD_DIR/package/rozip.py" "$REPO_DIR/out/mixed/GTKLibs-$GTKLIBS_VERSION.zip" -C "$B" Resources
  rm -rf "$B"
  ls -la "$REPO_DIR/out/mixed/GTKLibs-$GTKLIBS_VERSION.zip"
}

libs
gtklibs
