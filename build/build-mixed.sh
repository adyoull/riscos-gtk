#!/bin/bash -e
# The mixed-link test (riscos-crossdev v1.11-test1, branch mixed-link-test):
# the GTK stack as shared libraries, and Geany linked against them with
# UnixLib, libstdc++ and libgcc still linked in.
#
#   RCD_DIR=<crossdev v1.11-test1> RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-deps.sh
#   RCD_DIR=... RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-mixed.sh libs
#   RCD_DIR=... RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-mixed.sh geany
#   RCD_DIR=... RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-mixed.sh l3afpad
#   RCD_DIR=... RISCOS_GTK_MIXED=1 SRC=... SYSROOT=... build/build-mixed.sh bundle
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
# !GTKLibs' version: GTK's, then riscos-gtk's build number (continuing
# from the release candidates: RC17 was build 17)
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

# l3afpad: L3afpad (C), linked as Geany is. It uses no C++ itself, so the
# link is done with g++: libstdc++ comes in from the static library for
# what harfbuzz.so needs (operator new, __cxa_*), and the program exports
# it.
l3afpad() {
  local V=0.8.18.1.11 L=$SRC/l3afpad-0.8.18.1.11 OUTD=$REPO_DIR/out/mixed
  if [ ! -f "$L/src/config.h" ]; then
    rm -rf "$L"; tar -xzf "$DL/l3afpad_$V.orig.tar.gz" -C "$SRC"
    (cd "$L" && patch -p1 -s < "$REPO_DIR/patches/l3afpad-$V-riscos.diff")
    cp "$REPO_DIR/apps/l3afpad/config.h" "$L/src/"
  fi
  mkdir -p "$OUTD"
  local libs="" a so
  while read -r a so; do [ -n "$a" ] && libs="$libs -l${so%%.so.*}"; done <<< "$LIBS"
  local O=$SRC/l3afpad-objs f; rm -rf "$O"; mkdir -p "$O"
  for f in "$L"/src/*.c "$REPO_DIR/apps/l3afpad/riscos-l3afpad.c"; do
    "$SRC/.wrap/$HOST-gcc" $RO_CFLAGS -Wall -Wno-deprecated-declarations -Wno-unused-variable -Wno-unused-but-set-variable \
      -DHAVE_CONFIG_H -DRISCOS_PORT_VERSION=\"0.8.18.1.11-mixed-test2\" -I"$L/src" \
      $(pkg-config --cflags gtk+-3.0) -c "$f" -o "$O/$(basename "$f" .c).o"
  done
  "$SRC/.wrap/$HOST-g++" $RO_CFLAGS "$O"/*.o -o "$OUTD/l3afpad" -rdynamic -L"$SO" $libs
  "$RCD_DIR/checks/check-all.sh" "$OUTD/l3afpad"
  ls -la "$OUTD/l3afpad"
}

# gtklibs: out/mixed/!GTKLibs, the libraries every mixed GTK program
# uses, in one place so that SOManager loads one copy for all of them:
# !Boot and !Run set GTKLibs$Path; the programs' !Run point
# <prog>$LD$Env at it.
gtklibs() {
  local G=$REPO_DIR/out/mixed/!GTKLibs f
  rm -rf "$G"; mkdir -p "$G/lib"
  for f in "$SO"/*.so.*; do $STRIP --strip-unneeded -o "$G/lib/$(basename "$f"),e1f" "$f"; done
  for f in '!Boot,feb' '!Run,feb'; do
    cat > "$G/$f" <<'EOF'
| !GTKLibs (mixed-link TEST): GTK 3.24.52, riscos-gtk's RISC OS backend
| and the libraries under them, as shared libraries for mixed-linked GTK
| programs (loaded by !SharedLibs: SOManager and ld-riscos/so/2)
Set GTKLibs$Dir <Obey$Dir>
Set GTKLibs$Version @VERSION@
Set GTKLibs$Path <GTKLibs$Dir>.lib.
IconSprites <GTKLibs$Dir>.!Sprites
EOF
  done
  sed -i "s/@VERSION@/$GTKLIBS_VERSION/" "$G/!Boot,feb" "$G/!Run,feb"
  cat > "$G/!Help,fff" <<EOF
!GTKLibs $GTKLIBS_VERSION
==========$(echo "$GTKLIBS_VERSION" | tr -c '\n' '=')

The shared libraries for GTK programs linked "mixed" (riscos-crossdev
1.11-test1): GTK 3.24.52 with riscos-gtk's RISC OS backend, and GLib,
cairo, pango, harfbuzz, FreeType, fontconfig, pixman, libpng, libjpeg and
the rest under them. Every mixed GTK program uses this one copy, loaded
by !SharedLibs (SOManager and ld-riscos/so/2).

It lives in !Boot.Resources (PackMan installs it there). Its version is
GTK's, then riscos-gtk's build number: $GTKLIBS_VERSION, set in
GTKLibs\$Version when it's booted.

A TEST BUILD (riscos-gtk branch mixed-link-test, commit $(git -C "$REPO_DIR" rev-parse --short=12 HEAD)).
The libraries are LGPL 2.1 and MIT-style: LicenceLGPL; their source is
riscos-gtk's, https://github.com/adyoull/riscos-gtk
EOF
  # its icon: a shelf of library volumes (tools/mkgtklibsicon.py)
  python3 -B "$REPO_DIR/tools/mkgtklibsicon.py" "$G"
  cp "$REPO_DIR/riscos/licences/LGPL-2.1.txt" "$G/LicenceLGPL,fff"
}

# package NAME PROG: out/mixed/!NAME from the static out/!NAME (its data,
# fonts, theme): the program as ELF (PROG,e1f, stripped), PThreadTicker
# from this toolchain, and !Run checking for !SharedLibs (ld-riscos/so/2)
# and !GTKLibs and setting PROG$LD$Env
package() {
  local NAME=$1 PROG=$2 OUTD=$REPO_DIR/out/mixed
  local A=$OUTD/!$NAME
  [ -d "$REPO_DIR/out/!$NAME" ] || { echo "build-mixed.sh: build the static !$NAME first" >&2; exit 1; }
  rm -rf "$A"; cp -r "$REPO_DIR/out/!$NAME" "$A"; rm -f "$A/$PROG,ff8"; rm -rf "$A/lib"
  $STRIP -o "$A/$PROG,e1f" "$OUTD/$PROG"
  cp "$GCCSDK_ENV/riscos/PThrTicker,ffa" "$A/"
  local size slot
  size=$(stat -c %s "$A/$PROG,e1f"); slot=$(( (size + 4 * 1024 * 1024 + 1023) / 1024 ))
  python3 -B "$REPO_DIR/tools/mixedrun.py" "$A/!Run,feb" "$NAME" "$PROG" "$slot"
}

# bundle: two zips for RISC OS 5 itself.
#   GTKMixed-test5.zip: !Geany, !L3afpad, apps/geany/mixed-test's ReadMe
#     and !MemReport. The programs don't carry the libraries.
#   GTKLibs-<GTKLIBS_VERSION>.zip: Resources.!GTKLibs, to copy into !Boot.Resources as
#     PackMan will install it. !SharedLibs isn't shipped: the programs use
#     the one in !Boot.Resources (it must have ld-riscos/so/2).
bundle() {
  local OUTD=$REPO_DIR/out/mixed B=$REPO_DIR/out/mixed/bundle
  gtklibs
  package Geany geany
  package L3afpad l3afpad
  rm -rf "$B"; mkdir -p "$B/GTKMixed" "$B/Resources"
  cp -r "$OUTD/!Geany" "$OUTD/!L3afpad" \
    "$REPO_DIR/apps/geany/mixed-test/!MemReport" "$REPO_DIR/apps/geany/mixed-test/ReadMe,fff" "$B/GTKMixed/"
  cp -r "$OUTD/!GTKLibs" "$B/Resources/"
  rm -f "$OUTD"/GTKMixed-test5.zip "$OUTD"/GTKLibs-*.zip
  python3 "$RCD_DIR/package/rozip.py" "$OUTD/GTKMixed-test5.zip" -C "$B" GTKMixed
  python3 "$RCD_DIR/package/rozip.py" "$OUTD/GTKLibs-$GTKLIBS_VERSION.zip" -C "$B" Resources
  ls -la "$OUTD"/GTKMixed-test5.zip "$OUTD/GTKLibs-$GTKLIBS_VERSION.zip"
}

case "${1:-}" in
  bundle) bundle ;;
  libs) libs ;;
  geany) geany ;;
  l3afpad) l3afpad ;;
  *) echo "usage: build-mixed.sh libs|geany|l3afpad|bundle" >&2; exit 1 ;;
esac
