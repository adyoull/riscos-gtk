#!/bin/bash -e
# !Geany: Geany 2.1 (GPL v2), the programmer's editor, on GTK 3 for RISC
# OS: riscos-gtk's demo of a large autotools program built against it.
#   RCD_DIR=... SRC=... SYSROOT=... build/build-geany.sh
# Unpacks the source into $SRC, applies patches/geany-2.1-riscos.diff
# (its data from the application directory), configures it for
# arm-riscos-gnueabihf (static; no plugins, VTE, socket or docs), builds
# libgeany's objects, links it with apps/geany/riscos-geany.c (main: the icon
# bar, the Log, RISC OS names), and packages out/!Geany and
# out/Geany-$VERSION.zip (build-app.sh) with Geany's data (filetypes,
# colour schemes, templates, tags) and icons.
# Notes for other autotools programs: docs/PORTING-NOTES.md, sdk/docs/PORTING.md.
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/env.sh"
[ "$TARGET" = riscos ] || { echo "build-geany.sh: RISC OS only" >&2; exit 1; }
V=2.1
VERSION=${VERSION:-2.1-riscos1}
G=$SRC/geany-$V
# GEANY_RELINK=1: only link and package again (Geany built already)
if [ -z "${GEANY_RELINK:-}" ]; then
rm -rf "$G"
tar -xzf "$DL/geany_$V.orig.tar.gz" -C "$SRC"
(cd "$G" && patch -p1 -s < "$REPO_DIR/patches/geany-$V-riscos.diff")

cd "$G"
# ctags' directory reading wants d_reclen without HAVE_SCANDIR; UnixLib has scandir
./configure --host=$HOST --disable-shared --enable-static --disable-plugins --disable-nls \
  --disable-vte --disable-socket --disable-html-docs --disable-pdf-docs --disable-api-docs \
  --prefix=/geany CPPFLAGS=-DHAVE_SCANDIR CFLAGS="$RO_CFLAGS" CXXFLAGS="$RO_CFLAGS" \
  PKG_CONFIG="pkg-config --static" > "$SRC/geany-configure.log" 2>&1 \
  || { tail -30 "$SRC/geany-configure.log"; exit 1; }
for d in scintilla ctags src/tagmanager; do
  make -C "$d" -j"$(nproc)" > "$SRC/geany-make.log" 2>&1 || { tail -30 "$SRC/geany-make.log"; exit 1; }
done
# libgeany's objects, archived here rather than by libtool: crossdev's
# libtool .la files can name the machine crossdev was built on (/opt/b14),
# and libtool then stops at libm.la (see sdk/docs/PORTING.md)
# (make stops at that link, after compiling them all)
make -C src -k -j"$(nproc)" libgeany.la >> "$SRC/geany-make.log" 2>&1 || true
n_lo=$(ls src/libgeany_la-*.lo | wc -l); n_o=$(ls src/libgeany_la-*.o | wc -l)
[ "$n_lo" -gt 30 ] && [ "$n_lo" = "$n_o" ] || { echo "build-geany.sh: libgeany: $n_o of $n_lo objects" >&2; tail -30 "$SRC/geany-make.log"; exit 1; }
rm -f src/libgeany-riscos.a
$AR rcs src/libgeany-riscos.a src/libgeany_la-*.o
# Geany's data and icons, installed into a staging folder
rm -rf "$G/_inst"
make -C data install DESTDIR="$G/_inst" >> "$SRC/geany-make.log" 2>&1
make -C icons install DESTDIR="$G/_inst" >> "$SRC/geany-make.log" 2>&1
fi

APP=$SRC/geany-app
rm -rf "$APP"; cp -r "$REPO_DIR/apps/geany/app" "$APP"
mkdir -p "$APP/share"
cp -r "$G/_inst/geany/share/geany" "$APP/share/"
# Geany's own icons (the symbol list's, its toolbar's), PNGs only: the
# RISC OS build has no SVG loader
mkdir -p "$APP/share/icons"
(cd "$G/_inst/geany/share/icons" && find hicolor -name '*.png' | while read -r f; do
  mkdir -p "$APP/share/icons/$(dirname "$f")"; cp "$f" "$APP/share/icons/$f"; done)
cp "$G/COPYING" "$APP/Licence,fff"

$CXX $RO_CFLAGS -Wall -Wno-deprecated-declarations -DHAVE_CONFIG_H -DGTK -DGEANY_PRIVATE -DRISCOS_PORT_VERSION=\"$VERSION\" \
  -I"$G" -I"$G/src" -I"$G/src/tagmanager" -I"$G/scintilla/include" -I"$G/scintilla/lexilla/include" \
  $(pkg-config --cflags gtk+-3.0) -x c "$REPO_DIR/apps/geany/riscos-geany.c" -x none \
  "$G/src/libgeany-riscos.a" "$G/src/tagmanager/.libs/libtagmanager.a" "$G/ctags/.libs/libctags.a" \
  "$G/ctags/.libs/libgnu_regex.a" "$G/scintilla/.libs/libscintilla.a" "$G/scintilla/.libs/liblexilla.a" \
  -o "$SRC/geany-prog" -static \
  $(pkg-config --libs --static gtk+-3.0 gmodule-2.0) -lstdc++
# a monospaced font, for the editor
export EXTRA_FONTS="DejaVuSansMono.ttf DejaVuSansMono-Bold.ttf"
export PREBUILT=$SRC/geany-prog
exec "$HERE/build-app.sh" Geany geany "$VERSION" "$APP"
