#!/bin/bash -e
# !AbiWord: AbiWord 3.0.8 (GPL v2 or later), the word processor, on GTK 3
# for RISC OS.
#   RCD_DIR=... SRC=... SYSROOT=... build/build-abiword.sh
#   TARGET=linuxarm ...: the same build for the tests (stops after make)
# Builds what AbiWord needs into $STAGE if it isn't there:
#   libxml2, libxslt, libgsf, GOffice and librsvg's stand-in (as for
#     Gnumeric: build-gnumeric.sh makes them; run it first),
#   wv 1.2.9 (Word 97-2003 files),
#   Boost 1.83's headers (AbiWord uses boost::function, bind and
#     shared_ptr; nothing compiled) in $SRC/boost-1.83, not the devkit,
# then AbiWord: Debian's patches (patches/abiword-3.0.8-debian) and
# patches/abiword-3.0.8-riscos.diff, autoreconf (needs autoconf-archive),
# its file format plugins linked in (--enable-builtin-plugins), spell
# checking through enchant/hunspell (build-spell.sh). The program's main() is apps/abiword/riscos-abiword.cpp.
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/env.sh"
JOBS=${JOBS:-$(nproc)}
log() { echo "build-abiword.sh: $*"; }
fail() { grep -E 'error|Error' "$1" | head -20; echo "build-abiword.sh: failed, see $1" >&2; exit 1; }
for f in libxml2.a libxslt.a libgsf-1.a libgoffice-0.10.a librsvg-2.a; do
  [ -f "$STAGE/lib/$f" ] || { echo "build-abiword.sh: no $f: run build-gnumeric.sh first" >&2; exit 1; }
done
# spell checking: enchant and hunspell (build-spell.sh, shared with other
# programs), with the en_GB dictionary
"$HERE/build-spell.sh"
command -v autoreconf > /dev/null && [ -f /usr/share/aclocal/ax_check_compile_flag.m4 ] ||
  { echo "build-abiword.sh: needs autoconf and autoconf-archive (build/check-host.sh)" >&2; exit 1; }

# ---- librsvg's stand-in: AbiWord also uses rsvg_handle_new/write/close ----
if ! $HOST-nm "$STAGE/lib/librsvg-2.a" 2>/dev/null | grep -q ' T rsvg_handle_new$'; then
  log "librsvg stand-in"
  R=$REPO_DIR/riscos/rsvg-stub
  cp "$R"/librsvg/*.h "$STAGE/include/librsvg-2.0/librsvg/"
  $CC $RO_CFLAGS -I"$R" $(pkg-config --cflags gdk-pixbuf-2.0 cairo) -c "$R/rsvg-stub.c" -o "$SRC/rsvg-stub.o"
  rm -f "$STAGE/lib/librsvg-2.a"; $AR rcs "$STAGE/lib/librsvg-2.a" "$SRC/rsvg-stub.o"
fi

# ---- wv (Word 97-2003) ----
if [ ! -f "$STAGE/lib/libwv.a" ]; then
  log wv
  W=$SRC/wv-1.2.9
  rm -rf "$W"; tar -xzf "$DL/wv_1.2.9.orig.tar.gz" -C "$SRC"
  (cd "$W" && CFLAGS="$RO_CFLAGS" PKG_CONFIG="pkg-config --static" ./configure --host=$HOST --build=$BUILD \
     --prefix=$PREFIX --libdir=$PREFIX/lib --disable-shared --enable-static --without-libwmf \
     > ../wv.log 2>&1 &&
   { make -k -j$JOBS libwv.la >> ../wv.log 2>&1 || true; } &&
   n_lo=$(ls *.lo | wc -l) && [ "$n_lo" -gt 80 ] && [ "$n_lo" = "$(ls *.o | wc -l)" ] &&
   rm -f "$STAGE/lib/libwv.a" && $AR rcs "$STAGE/lib/libwv.a" $(ls *.lo | sed 's/\.lo$/.o/') &&
   make install-wvhdrHEADERS install-pkgconfigDATA DESTDIR="$SYSROOT" >> ../wv.log 2>&1) || fail "$SRC/wv.log"
fi

# ---- Boost's headers ----
BOOST=$SRC/boost-1.83
if [ ! -f "$BOOST/include/boost/version.hpp" ]; then
  log "Boost headers"
  rm -rf "$BOOST" "$SRC/boost-x"; mkdir -p "$BOOST/include/boost" "$SRC/boost-x"
  tar -xJf "$DL/boost1.83_1.83.0.orig.tar.xz" -C "$SRC/boost-x" --wildcards 'boost/libs/*'
  # the tarball is Boost's modular layout: each library's include/boost
  find "$SRC/boost-x/boost/libs" -type d -path '*/include/boost' -prune |
    while read -r d; do cp -r "$d/." "$BOOST/include/boost/"; done
  rm -rf "$SRC/boost-x"
fi

# ---- libtool and the toolchain's .la files ----
# crossdev's libstdc++.la, libm.la ... name the machine crossdev was built
# on (/opt/b14), so libtool can't link C++ libraries. Copies that name
# this toolchain, with its .a files, in $SRC/la-fix; AbiWord's libtool is
# pointed at them below. (The tests' cross compiler has no such problem.)
LAFIX=$SRC/la-fix
if [ "$TARGET" = riscos ]; then
  T=$GCCSDK_ENV/arm-riscos-gnueabihf/lib
  rm -rf "$LAFIX"; mkdir -p "$LAFIX"
  for l in "$T"/*.la; do
    n=$(basename "$l" .la)
    sed -e "s|/opt/b14/toolchain/env/arm-riscos-gnueabihf/lib|$LAFIX|g" -e "s|^libdir=.*|libdir='$LAFIX'|" "$l" > "$LAFIX/$n.la"
    a=$(sed -n "s/^old_library='\(.*\)'/\1/p" "$l")
    [ -z "$a" ] || ln -sf "$T/$a" "$LAFIX/$a"
  done
  for l in "$STAGE"/lib/*.la; do
    sed -i "s|/opt/b14/toolchain/env/arm-riscos-gnueabihf/lib|$LAFIX|g" "$l"
  done
fi

# ---- AbiWord ----
V=3.0.8
AB=$SRC/AbiWord-release-$V
PLUGINS="opendocument openxml docbook latex mswrite applix clarisworks t602 wml xslfo epub eml hrtext iscii kword mif opml pdb sdw s5 passepartout"
RISCOS_FLAGS="-DABI_RISCOS"
if [ -z "${ABIWORD_RELINK:-}" ]; then
  log AbiWord
  rm -rf "$AB"; tar -xJf "$DL/abiword_$V+ds.orig.tar.xz" -C "$SRC"
  for p in $(cat "$REPO_DIR/patches/abiword-$V-debian/series"); do
    patch -d "$AB" -p1 -s < "$REPO_DIR/patches/abiword-$V-debian/$p"
  done
  patch -d "$AB" -p1 -s < "$REPO_DIR/patches/abiword-$V-riscos.diff"
  (cd "$AB" && ./autogen-common.sh > ../abiword.log 2>&1 && autoreconf -fi >> ../abiword.log 2>&1 &&
   CFLAGS="$RO_CFLAGS -Wno-deprecated-declarations $RISCOS_FLAGS" \
   CXXFLAGS="$RO_CFLAGS -Wno-deprecated-declarations $RISCOS_FLAGS" PKG_CONFIG="pkg-config --static" \
     ./configure --host=$HOST --build=$BUILD --prefix=$PREFIX --libdir=$PREFIX/lib \
     --disable-shared --enable-static --disable-default-plugins \
     --enable-plugins="$PLUGINS" --enable-builtin-plugins="$PLUGINS" \
     --enable-spell --with-goffice --with-boost="$BOOST" --without-redland \
     --without-evolution-data-server --without-libical --without-champlain \
     --disable-introspection --disable-maintainer-mode ac_cv_path_LIBGCRYPT_CONFIG=no \
     >> ../abiword.log 2>&1) || fail "$SRC/abiword.log"
  if [ "$TARGET" = riscos ]; then
    # libtool: the toolchain's .la files from $LAFIX (above)
    sed -i -e "s|^compiler_lib_search_path=\"|compiler_lib_search_path=\"-L$LAFIX |" \
           -e "s|^compiler_lib_search_dirs=\"|compiler_lib_search_dirs=\"$LAFIX |" \
           -e "s|^sys_lib_search_path_spec=\"|sys_lib_search_path_spec=\"$LAFIX |" \
           -e "s|^postdeps=\"-lstdc++ |postdeps=\"$LAFIX/libstdc++.la |" "$AB/libtool"
  fi
  # (the build machine's glib-compile-resources: configure finds the sysroot's)
  (cd "$AB" && make -j$JOBS GLIB_COMPILE_RESOURCES=glib-compile-resources >> ../abiword.log 2>&1) || fail "$SRC/abiword.log"
fi
LIBABI=$AB/src/.libs/libabiword-3.0.a
[ -f "$LIBABI" ] || { echo "build-abiword.sh: no $LIBABI" >&2; exit 1; }
# the include flags AbiWord's own sources are compiled with
# (relative to src: made absolute)
AB_CFLAGS="-DHAVE_CONFIG_H -I$AB -I$AB/src $(make -s -C "$AB/src" -f Makefile -f <(printf 'print-cppflags:\n\t@echo $(libabiword_3.0_la_CPPFLAGS)\n') print-cppflags |
  sed -e "s| -I\.\./| -I$AB/|g" -e "s| -I\./| -I$AB/src/|g")"
AB_LDLIBS="$(pkg-config --libs --static libgoffice-0.10 gtk+-3.0 libgsf-1 wv-1.0 libxslt fribidi librsvg-2.0 enchant-2) -ljpeg -lstdc++ -lm"

# ---- !AbiWord ----
VERSION=${VERSION:-3.0.8-riscos1-test5}
APP=$SRC/abiword-app
rm -rf "$APP"; cp -r "$REPO_DIR/apps/abiword/app" "$APP"
# AbiWord's data: dialogue .ui files, templates, the XSLT for MathML and
# Word's equations, the system profile; no strings files (AbiWord's
# built-in English strings are used: riscos-abiword.cpp)
I=$SRC/abiword-inst; rm -rf "$I"
make -C "$AB" install-data DESTDIR="$I" > /dev/null 2>&1 || make -C "$AB" install DESTDIR="$I" > /dev/null
D=$APP/share/abiword-3.0
mkdir -p "$D"
(cd "$I$PREFIX/share/abiword-3.0" && cp -r ui templates xsltml omml_xslt mime-info system.profile readme.abw "$D/")
# (no hicolor icons: the application icon, "abiword", is drawn in the
# RISC OS style with the toolbar's: ICON_PACK=abiword, below)
cp "$AB/COPYING" "$APP/Licence,fff"   # GPL 2
# the British English dictionary (hunspell finds it through XDG_DATA_DIRS:
# /<AbiWord$Dir>/share); others go in <Choices$Write>.GTK.Dictionaries
mkdir -p "$APP/share/hunspell"
cp "$SRC/dictionaries/en_GB.dic" "$SRC/dictionaries/en_GB.aff" "$APP/share/hunspell/"
cp "$SRC/dictionaries/README_en_GB.txt" "$APP/share/hunspell/LicenceDictionary,fff"
if [ "$TARGET" != riscos ]; then
  # the tests link these themselves (tests/gtk/abiword-fake.cpp)
  { printf 'AB_CFLAGS=%q\n' "$AB_CFLAGS"; printf 'AB_LIBS=%q\n' "$LIBABI"; } > "$SRC/abiword-build.txt"
  log done; exit 0
fi

$CXX -std=c++11 $RO_CFLAGS -Wall -Wno-deprecated-declarations $RISCOS_FLAGS $AB_CFLAGS \
  -DRISCOS_PORT_VERSION=\"$VERSION\" -c "$REPO_DIR/apps/abiword/riscos-abiword.cpp" -o "$SRC/riscos-abiword.o"
$CXX -std=c++11 $RO_CFLAGS "$SRC/riscos-abiword.o" "$LIBABI" -o "$SRC/abiword-prog" -static $AB_LDLIBS
export PREBUILT=$SRC/abiword-prog
# the toolbar's own icons and the application icon in the RISC OS style
# (tools/riscosicons_apps.py)
ICON_PACK=abiword exec "$HERE/build-app.sh" AbiWord abiword "$VERSION" "$APP"
