#!/bin/bash -e
# SVG pictures for GTK programs on RISC OS: librsvg 2.40.20 (LGPL v2 or
# later; the last version written in C), with libcroco 0.6.13 (LGPL v2,
# its CSS parser) and libxml2 2.9.14 (MIT), built into $STAGE as static
# libraries. Also librsvg's GdkPixbuf loader (io-svg.c), compiled with
# the names gdk-pixbuf's built-in loaders have: patches/
# gdk-pixbuf-2.42.10-riscos.diff refers to them weakly, so a program
# linked with `pkg-config --static riscos-gtk-svg` gets SVG in
# gdk_pixbuf_new_from_file, GtkImage, icon themes and the rest; programs
# without it are unchanged.
#   RCD_DIR=... SRC=... SYSROOT=... build/build-svg.sh  (TARGET=linuxarm too)
# Programs that call librsvg themselves use pkg-config --static librsvg-2.0.
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/env.sh"
JOBS=${JOBS:-$(nproc)}
log() { echo "build-svg.sh: $*"; }
fail() { grep -E 'error|Error' "$1" | head -20; echo "build-svg.sh: failed, see $1" >&2; exit 1; }
# A libtool library's objects archived here (make stops at libtool's link:
# crossdev's .la files can name the machine crossdev was built on)
ar_objects() {  # DIR LIB.a MIN
  local n_o
  n_o=$(ls "$1"/*.o 2>/dev/null | wc -l)
  [ "$n_o" -ge "$3" ] || { echo "build-svg.sh: $1: only $n_o objects" >&2; return 1; }
  rm -f "$STAGE/lib/$2"; $AR rcs "$STAGE/lib/$2" "$1"/*.o
}

# ---- libxml2 (also Gnumeric's and AbiWord's) ----
if [ ! -f "$STAGE/lib/libxml2.a" ]; then
  log libxml2
  X=$SRC/libxml2-2.9.14
  rm -rf "$X"; tar -xf "$DL/libxml2_2.9.14+dfsg.orig.tar.xz" -C "$SRC"
  (cd "$X" && CFLAGS="$RO_CFLAGS" ./configure --host=$HOST --build=$BUILD --prefix=$PREFIX \
     --libdir=$PREFIX/lib --disable-shared --enable-static --without-python --without-lzma \
     --without-ftp --without-debug --without-modules --with-zlib \
     > ../libxml2.log 2>&1 &&
   make -j$JOBS libxml2.la >> ../libxml2.log 2>&1 &&
   make install-libLTLIBRARIES install-data-am install-pkgconfigDATA -C . DESTDIR="$SYSROOT" >> ../libxml2.log 2>&1 &&
   make -C include install DESTDIR="$SYSROOT" >> ../libxml2.log 2>&1) || fail "$SRC/libxml2.log"
fi
# (no build-machine paths in its .pc: configure put zlib's -L in it)
sed -i "s| -L$STAGE/lib||g" "$STAGE/lib/pkgconfig/libxml-2.0.pc"

# ---- libcroco ----
if [ ! -f "$STAGE/lib/libcroco-0.6.a" ]; then
  log libcroco
  C=$SRC/libcroco-0.6.13
  rm -rf "$C"; tar -xf "$DL/libcroco_0.6.13.orig.tar.xz" -C "$SRC"
  (cd "$C" && CFLAGS="$RO_CFLAGS" PKG_CONFIG="pkg-config --static" ./configure --host=$HOST --build=$BUILD \
     --prefix=$PREFIX --libdir=$PREFIX/lib --disable-shared --enable-static --disable-gtk-doc \
     > ../libcroco.log 2>&1 &&
   { make -k -j$JOBS -C src libcroco-0.6.la >> ../libcroco.log 2>&1 || true; } &&
   ar_objects src libcroco-0.6.a 25 &&
   make -C src install-crocoincHEADERS DESTDIR="$SYSROOT" >> ../libcroco.log 2>&1 &&
   make install-pkgconfigDATA DESTDIR="$SYSROOT" >> ../libcroco.log 2>&1) || fail "$SRC/libcroco.log"
  # the stand-in's files, if an older build left them
  rm -f "$STAGE/lib/librsvg-2.a"
fi

# ---- librsvg (and its GdkPixbuf loader) ----
if ! $HOST-nm "$STAGE/lib/librsvg-2.a" 2>/dev/null | grep -q ' T _gdk_pixbuf__svg_fill_vtable$'; then
  log librsvg
  R=$SRC/librsvg-2.40.20
  rm -rf "$R"; tar -xf "$DL/librsvg_2.40.20.orig.tar.xz" -C "$SRC"
  rm -rf "$STAGE/include/librsvg-2.0"
  (cd "$R" && CFLAGS="$RO_CFLAGS -Wno-deprecated-declarations" PKG_CONFIG="pkg-config --static" \
     ./configure --host=$HOST --build=$BUILD --prefix=$PREFIX --libdir=$PREFIX/lib \
     --disable-shared --enable-static --disable-pixbuf-loader --disable-tools \
     --disable-introspection --enable-vala=no --disable-gtk-doc --disable-installed-tests \
     > ../librsvg.log 2>&1 &&
   # (its test mode's font is named by the source folder: not this machine's)
   sed -i 's|-DSRCDIR=\\""${abs_top_srcdir}"\\"|-DSRCDIR=\\"/librsvg-2.40.20\\"|' Makefile &&
   { make -k -j$JOBS librsvg-2.la >> ../librsvg.log 2>&1 || true; } &&
   n_o=$(ls librsvg_2_la-*.o 2>/dev/null | wc -l) && [ "$n_o" -ge 25 ] &&
   $CC $RO_CFLAGS -I. -DGDK_PIXBUF_ENABLE_BACKEND $(pkg-config --static --cflags gdk-pixbuf-2.0 cairo pangocairo libxml-2.0 libcroco-0.6) \
     -Dfill_vtable=_gdk_pixbuf__svg_fill_vtable -Dfill_info=_gdk_pixbuf__svg_fill_info \
     -c gdk-pixbuf-loader/io-svg.c -o io-svg.o >> ../librsvg.log 2>&1 &&
   rm -f "$STAGE/lib/librsvg-2.a" && $AR rcs "$STAGE/lib/librsvg-2.a" librsvg_2_la-*.o io-svg.o &&
   make install-librsvgincHEADERS install-pkgconfigDATA DESTDIR="$SYSROOT" >> ../librsvg.log 2>&1) ||
     fail "$SRC/librsvg.log"
  # what a static link needs too (librsvg-2.0.pc leaves its private
  # requirements empty)
  sed -i 's/^Requires.private:.*/Requires.private: libxml-2.0 libcroco-0.6 pangocairo pangoft2/' \
    "$STAGE/lib/pkgconfig/librsvg-2.0.pc"
  cat > "$STAGE/lib/pkgconfig/riscos-gtk-svg.pc" <<EOF
prefix=$PREFIX
libdir=\${prefix}/lib

Name: riscos-gtk-svg
Description: SVG in GdkPixbuf (librsvg's loader, built in)
Version: 2.40.20
Requires: librsvg-2.0
Libs: -Wl,-u,_gdk_pixbuf__svg_fill_vtable -Wl,-u,_gdk_pixbuf__svg_fill_info
EOF
fi
log "librsvg $($HOST-nm "$STAGE/lib/librsvg-2.a" | grep -c ' T rsvg_') functions, the GdkPixbuf loader"
