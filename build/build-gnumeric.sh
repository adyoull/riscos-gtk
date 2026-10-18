#!/bin/bash -e
# !Gnumeric: Gnumeric 1.12.57 (GPL v2/v3), the spreadsheet, on GTK 3 for
# RISC OS.
#   RCD_DIR=... SRC=... SYSROOT=... build/build-gnumeric.sh
#   TARGET=linuxarm ...: the same build for the tests (stops after make)
# Builds what Gnumeric needs into $STAGE if it isn't there:
#   libxml2 2.9.14 (no Python, FTP, lzma; HTTP for libgsf's gsf-input-http),
#   libxslt 1.1.39 (GOffice's MathML),
#   libgsf 1.14.51 (structured files: OLE2, ZIP; no bz2, no introspection),
#   GOffice 0.10.57 (charts, number formats; patches/goffice-0.10.57-riscos.diff:
#     librsvg optional, as it is Rust now; its settings in a key file),
# then Gnumeric (patches/gnumeric-1.12.57-riscos.diff).
# Gnumeric's and GOffice's plugins (functions, file formats, chart types)
# are linked in: see build/mkstaticmodules.sh and GLib's static module
# registry (patches/glib-2.80.0-riscos.diff, g_module_register_static).
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/env.sh"
JOBS=${JOBS:-$(nproc)}
log() { echo "build-gnumeric.sh: $*"; }
# the tests' build (linuxarm) uses the RISC OS paths too (GOFFICE_DIR,
# GNUMERIC_DIR), as on RISC OS
RISCOS_UI=""
[ "$TARGET" = riscos ] || RISCOS_UI="-DGO_RISCOS_UI -DGNM_RISCOS_UI"
# UnixLib has no frexpl; on ARM EABI long double is double, so it's frexp
FREXPL=""
[ "$TARGET" != riscos ] || FREXPL="-Dfrexpl=frexp"
fail() { grep -E 'error|Error' "$1" | head -20; echo "build-gnumeric.sh: failed, see $1" >&2; exit 1; }
# A libtool library's objects archived here (make stops at libtool's link:
# crossdev's .la files can name the machine crossdev was built on, and
# libtool then stops at libm.la; see sdk/docs/PORTING.md)
ar_objects() {  # DIR LIB.a [MIN]: DIR's *.o (all compiled) into $STAGE/lib/LIB.a
  local n_c n_o
  n_o=$(ls "$1"/*.o 2>/dev/null | wc -l)
  [ "$n_o" -ge "${3:-1}" ] || { echo "build-gnumeric.sh: $1: only $n_o objects" >&2; return 1; }
  rm -f "$STAGE/lib/$2"; $AR rcs "$STAGE/lib/$2" "$1"/*.o
}

# ---- libxml2 ----
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

# ---- libxslt (GOffice's MathML to LaTeX) ----
if [ ! -f "$STAGE/lib/libxslt.a" ]; then
  log libxslt
  T=$SRC/libxslt-1.1.39
  rm -rf "$T"; tar -xf "$DL/libxslt_1.1.39.orig.tar.xz" -C "$SRC"
  (cd "$T" && CFLAGS="$RO_CFLAGS" PKG_CONFIG="pkg-config --static" ./configure --host=$HOST --build=$BUILD \
     --prefix=$PREFIX --libdir=$PREFIX/lib --disable-shared --enable-static --without-python \
     --without-crypto --without-debug --without-debugger --without-profiler --without-plugins \
     > ../libxslt.log 2>&1 &&
   { make -k -j$JOBS -C libxslt libxslt.la >> ../libxslt.log 2>&1 || true; } &&
   ar_objects libxslt libxslt.a 19 &&
   { make -k -j$JOBS -C libexslt libexslt.la >> ../libxslt.log 2>&1 || true; } &&
   ar_objects libexslt libexslt.a 5 &&
   make -C libxslt install-xsltincHEADERS install-nodist_xsltincHEADERS DESTDIR="$SYSROOT" >> ../libxslt.log 2>&1 &&
   make -C libexslt install-exsltincHEADERS install-nodist_exsltincHEADERS DESTDIR="$SYSROOT" >> ../libxslt.log 2>&1 &&
   make install-pkgconfigDATA DESTDIR="$SYSROOT" >> ../libxslt.log 2>&1) || fail "$SRC/libxslt.log"
fi

# ---- libgsf ----
if [ ! -f "$STAGE/lib/libgsf-1.a" ]; then
  log libgsf
  G=$SRC/libgsf-1.14.51
  rm -rf "$G"; tar -xf "$DL/libgsf_1.14.51.orig.tar.xz" -C "$SRC"
  (cd "$G" && CFLAGS="$RO_CFLAGS" PKG_CONFIG="pkg-config --static" ./configure --host=$HOST --build=$BUILD \
     --prefix=$PREFIX --libdir=$PREFIX/lib --disable-shared --enable-static --without-bz2 \
     --disable-introspection --without-gdk-pixbuf --disable-nls --disable-gtk-doc \
     > ../libgsf.log 2>&1 &&
   { make -k -j$JOBS -C gsf libgsf-1.la >> ../libgsf.log 2>&1 || true; } &&
   ar_objects gsf libgsf-1.a 40 &&
   make -C gsf install-libgsf_1_includeHEADERS DESTDIR="$SYSROOT" >> ../libgsf.log 2>&1 &&
   make install-pkgconfigDATA DESTDIR="$SYSROOT" >> ../libgsf.log 2>&1) || fail "$SRC/libgsf.log"
fi
# ---- librsvg's stand-in (riscos/rsvg-stub: loads no SVG) ----
if [ ! -f "$STAGE/lib/librsvg-2.a" ]; then
  log "librsvg stand-in"
  R=$REPO_DIR/riscos/rsvg-stub
  mkdir -p "$STAGE/include/librsvg-2.0/librsvg" "$STAGE/lib/pkgconfig"
  cp "$R"/librsvg/*.h "$STAGE/include/librsvg-2.0/librsvg/"
  $CC $RO_CFLAGS -I"$R" $(pkg-config --cflags gdk-pixbuf-2.0 cairo) -c "$R/rsvg-stub.c" -o "$SRC/rsvg-stub.o"
  rm -f "$STAGE/lib/librsvg-2.a"; $AR rcs "$STAGE/lib/librsvg-2.a" "$SRC/rsvg-stub.o"
  sed "s|@PREFIX@|$PREFIX|" "$R/librsvg-2.0.pc.in" > "$STAGE/lib/pkgconfig/librsvg-2.0.pc"
fi

# ---- GOffice ----
GO=$SRC/goffice-0.10.57
if [ ! -f "$STAGE/lib/libgoffice-0.10.a" ]; then
  log GOffice
  rm -rf "$GO"; tar -xf "$DL/goffice_0.10.57.orig.tar.xz" -C "$SRC"
  [ ! -f "$REPO_DIR/patches/goffice-0.10.57-riscos.diff" ] ||
    patch -d "$GO" -p1 -s < "$REPO_DIR/patches/goffice-0.10.57-riscos.diff"
  (cd "$GO" && CFLAGS="$RO_CFLAGS -Wno-deprecated-declarations $FREXPL $RISCOS_UI" PKG_CONFIG="pkg-config --static" \
     ./configure --host=$HOST --build=$BUILD --prefix=$PREFIX --libdir=$PREFIX/lib \
     --disable-shared --enable-static --with-config-backend=keyfile --without-lasem \
     --without-libspectre --without-long-double --disable-introspection --disable-nls --disable-gtk-doc \
     > ../goffice.log 2>&1 &&
   { make -k -j$JOBS >> ../goffice.log 2>&1 || true; } &&
   n_lo=$(find goffice -name "*.lo" | wc -l) && n_o=$(find goffice -name "*.o" | wc -l) &&
   [ "$n_lo" -gt 100 ] && [ "$n_lo" = "$n_o" ] &&
   rm -f "$STAGE/lib/libgoffice-0.10.a" && $AR rcs "$STAGE/lib/libgoffice-0.10.a" $(find goffice -name "*.o" | sort) &&
   make -C goffice install-data-am DESTDIR="$SYSROOT" >> ../goffice.log 2>&1 &&
   make install-pkgconfigDATA DESTDIR="$SYSROOT" >> ../goffice.log 2>&1) || fail "$SRC/goffice.log"
fi
# ---- Gnumeric ----
V=1.12.57
GN=$SRC/gnumeric-$V
if [ -z "${GNUMERIC_RELINK:-}" ]; then
  log Gnumeric
  rm -rf "$GN"; tar -xf "$DL/gnumeric_$V.orig.tar.xz" -C "$SRC"
  [ ! -f "$REPO_DIR/patches/gnumeric-$V-riscos.diff" ] ||
    patch -d "$GN" -p1 -s < "$REPO_DIR/patches/gnumeric-$V-riscos.diff"
  (cd "$GN" && CFLAGS="$RO_CFLAGS -Wno-deprecated-declarations $FREXPL $RISCOS_UI" PKG_CONFIG="pkg-config --static" \
     ./configure --host=$HOST --build=$BUILD --prefix=$PREFIX --libdir=$PREFIX/lib \
     --disable-shared --enable-static --without-python --without-perl --without-gda \
     --without-psiconv --without-paradox --without-long-double \
     --disable-introspection --disable-nls ITSTOOL=true \
     > ../gnumeric.log 2>&1 &&
   # (the build machine's glib-compile-resources: configure finds the sysroot's)
   { make -k -j$JOBS GLIB_COMPILE_RESOURCES=glib-compile-resources >> ../gnumeric.log 2>&1 || true; }) || fail "$SRC/gnumeric.log"
fi

# ---- the plugins, built in (tools/mkstaticmodules.py) ----
# Gnumeric's (functions, file formats) and GOffice's (chart types,
# regressions); not the external solvers (glpk, lp_solve programs), Excel
# XLL add-ins, or the examples
SKIP=" excelplugins glpk lpsolve sample_datasource uihello lasem "
module_file() {  # PLUGIN.XML: its loader's module_file
  grep -o 'value="[^"]*" name="module_file"\|name="module_file" value="[^"]*"' "$1" |
    sed -E 's/.*value="([^"]*)".*/\1/' | head -1
}
MODS=()
for x in "$GN"/plugins/*/plugin.xml "$GO"/plugins/*/plugin.xml; do
  d=$(dirname "$x"); n=$(basename "$d"); mf=$(module_file "$x")
  case "$SKIP" in *" $n "*) continue ;; esac
  [ -n "$mf" ] && ls "$d"/*.o > /dev/null 2>&1 || continue
  MODS+=("$n/$mf=$(ls "$d"/*.o | tr '\n' ,)")
done
ST=$SRC/gnumeric-static
rm -rf "$ST"
python3 "$REPO_DIR/tools/mkstaticmodules.py" --nm $HOST-nm --objcopy $HOST-objcopy --ar $AR \
  --out "$ST" "${MODS[@]}" > "$ST.archives" || { echo "build-gnumeric.sh: plugins" >&2; exit 1; }
log "$(wc -l < "$ST.archives") plugins built in"

# ---- libspreadsheet (the objects libtool made .lo files for) ----
n_lo=$(find "$GN/src" -name "*.lo" | wc -l)
[ "$n_lo" -gt 200 ] || { echo "build-gnumeric.sh: only $n_lo libspreadsheet objects" >&2; exit 1; }
rm -f "$SRC/libspreadsheet.a"
$AR rcs "$SRC/libspreadsheet.a" $(find "$GN/src" -name "*.lo" | sort | sed 's/\.lo$/.o/')
# Gnumeric's main() becomes gnumeric_main (apps/gnumeric/riscos-gnumeric.c's comes first)
$HOST-objcopy --redefine-sym main=gnumeric_main "$GN/src/main-application.o" "$SRC/gnumeric-main.o"
GN_LIBS="$GN/src/io-context-gtk.o $(cat "$ST.archives") $SRC/libspreadsheet.a"
GN_CFLAGS="-I$GN -I$GN/src $(pkg-config --cflags libgoffice-0.10 gtk+-3.0 libgsf-1)"
GN_LDLIBS="$(pkg-config --libs --static libgoffice-0.10 gtk+-3.0 libgsf-1 libxml-2.0 libxslt gmodule-2.0) -lm"
# ---- !Gnumeric ----
VERSION=${VERSION:-1.12.57-riscos10}
APP=$SRC/gnumeric-app
rm -rf "$APP"; cp -r "$REPO_DIR/apps/gnumeric/app" "$APP"
# the plugins' descriptions (what each built-in module provides), in the
# folders the modules are registered under ("fn-math/plugin")
for spec in "${MODS[@]}"; do
  n=${spec%%/*}
  if [ -f "$GN/plugins/$n/plugin.xml" ]; then
    P=$GN/plugins/$n; D=$APP/lib/plugins/$n
  else
    P=$GO/plugins/$n; D=$APP/goffice/lib/plugins/$n
  fi
  mkdir -p "$D"; cp "$P/plugin.xml" "$D/"
  # and the files its services read at run time (<file>: GOffice's chart
  # types plot-types.xml, the trend lines' reg-types.xml, smoothing's
  # types.xml); without them Insert > Chart has no chart types
  for f in $(sed -n 's|.*<file>\([^<]*\)</file>.*|\1|p' "$P/plugin.xml"); do
    [ -f "$P/$f" ] || { echo "build-gnumeric.sh: $n's plugin.xml names $f, which isn't there" >&2; exit 1; }
    cp "$P/$f" "$D/"
  done
done
# Gnumeric's data (autoformat templates) and its icons (hicolor)
I=$SRC/gnumeric-inst; rm -rf "$I"
make -C "$GN/templates" install DESTDIR="$I" > /dev/null
make -C "$GN/icons" install DESTDIR="$I" > /dev/null
mkdir -p "$APP/share" "$APP/share/icons"
cp -r "$I$PREFIX/share/gnumeric/$V"/* "$APP/share/"
(cd "$I$PREFIX/share/icons" && find hicolor -name '*.png' | while read -r f; do
  mkdir -p "$APP/share/icons/$(dirname "$f")"; cp "$f" "$APP/share/icons/$f"; done)
cat "$GN/COPYING" "$GN/COPYING-gpl2" "$GN/COPYING-gpl3" > "$APP/Licence,fff"   # GPL 2 or 3
if [ "$TARGET" != riscos ]; then
  # the tests link GN_LIBS themselves (tests/gtk/gnumeric-fake.c), with
  # $SRC/gnumeric-app's plugin descriptions and data
  printf '%s\n' "$GN_LIBS" > "$SRC/gnumeric-libs.txt"
  log done; exit 0
fi

$CC $RO_CFLAGS -Wall -Wno-deprecated-declarations $GN_CFLAGS -DRISCOS_PORT_VERSION=\"$VERSION\" \
  "$REPO_DIR/apps/gnumeric/riscos-gnumeric.c" "$ST/staticmodules.c" "$SRC/gnumeric-main.o" $GN_LIBS \
  -o "$SRC/gnumeric-prog" -static $GN_LDLIBS
export PREBUILT=$SRC/gnumeric-prog
ICON_PACK=gnumeric exec "$HERE/build-app.sh" Gnumeric gnumeric "$VERSION" "$APP"
