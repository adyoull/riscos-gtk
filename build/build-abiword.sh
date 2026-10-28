#!/bin/bash -e
# !AbiWord: AbiWord 3.0.8 (GPL v2 or later), the word processor, on GTK 3
# for RISC OS.
#   RCD_DIR=... SRC=... SYSROOT=... build/build-abiword.sh
#   TARGET=linuxarm ...: the same build for the tests (stops after make)
# Builds what AbiWord needs into $STAGE if it isn't there:
#   libxml2, libxslt, libgsf, GOffice and librsvg (as for
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

# ---- Aiksaurus 1.2.1 (the thesaurus; GPL 2 or later): its library, with
# its data (words.dat, meanings.dat) found through AIK_DATA_DIR
# (riscos-abiword.cpp: share/aiksaurus in the application) ----
if [ ! -f "$STAGE/lib/libAiksaurus.a" ]; then
  log Aiksaurus
  K=$SRC/aiksaurus-1.2.1+dev-0.12
  rm -rf "$K"; tar -xzf "$DL/aiksaurus_1.2.1+dev-0.12.orig.tar.gz" -C "$SRC"
  (cd "$K/base" &&
   for f in Aiksaurus AiksaurusException AsciiCompare MeaningsFile WordsFile AiksaurusC; do
     $CXX $RO_CFLAGS -std=gnu++98 -w -DHAVE_GETENV -DAIK_DATA_DIR=\"/riscos-gtk/share/aiksaurus/\" -I.. -c $f.cpp -o $f.o || exit 1
   done &&
   rm -f "$STAGE/lib/libAiksaurus.a" && $AR rcs "$STAGE/lib/libAiksaurus.a" Aiksaurus.o AiksaurusException.o \
     AsciiCompare.o MeaningsFile.o WordsFile.o AiksaurusC.o &&
   mkdir -p "$STAGE/include/Aiksaurus" && cp Aiksaurus.h AiksaurusC.h "$STAGE/include/Aiksaurus/" &&
   printf 'prefix=%s\nlibdir=${prefix}/lib\nincludedir=${prefix}/include\n\nName: Aiksaurus\nDescription: English Thesaurus Library\nVersion: 1.2.1\nLibs: -L${libdir} -lAiksaurus\nCflags: -I${includedir}/Aiksaurus\n' \
     "$PREFIX" > "$STAGE/lib/pkgconfig/aiksaurus-1.0.pc") > "$SRC/aiksaurus.log" 2>&1 || fail "$SRC/aiksaurus.log"
fi

# ---- WordPerfect and Microsoft Works files: librevenge 0.0.5, libwpd
# 0.10.3 and libwps 0.4.14 (MPL 2.0 / LGPL 2.1+), C++, for AbiWord's
# wordperfect plugin. Built after Boost (librevenge uses its headers);
# libtool can't link C++ here, so each library's objects are archived ----
wpdlib() {  # NAME TARBALL DIR CONFIGURE-ARGS...: configure, make src/lib, archive, install headers and .pc
  local n=$1 tb=$2 d=$SRC/$3; shift 3
  log "$n"
  rm -rf "$d"; tar -xf "$DL/$tb" -C "$SRC"
  (cd "$d" && CFLAGS="$RO_CFLAGS" CXXFLAGS="$RO_CFLAGS -std=gnu++11 -Wno-deprecated-declarations -include cstddef" \
     CPPFLAGS="-I$BOOST/include" PKG_CONFIG="pkg-config --static" ./configure --host=$HOST --build=$BUILD \
     --prefix=$PREFIX --libdir=$PREFIX/lib --disable-shared --enable-static --without-docs \
     --disable-werror --disable-tests "$@" > "../$n.log" 2>&1 &&
   { make -k -j$JOBS -C src/lib >> "../$n.log" 2>&1 || true; } &&
   # (libtool's own link fails here, on the toolchain's .la files: any
   # other error is a source that didn't compile)
   { ! grep " error: " "../$n.log" | grep -v "is not a valid libtool archive" | grep -q . ||
     { grep " error: " "../$n.log" | head -5; exit 1; }; } &&
   # the libraries libtool would have made: objects named after each
   # (NAME_la-x.o) when the folder makes more than one (librevenge), else
   # all of them, for the first lib_LTLIBRARIES (libwpd, libwps)
   if ls src/lib/*_la-*.o > /dev/null 2>&1; then
     for pre in $(cd src/lib && ls *_la-*.o | sed 's/_la-.*//' | sort -u); do
       lib=$(echo "$pre" | sed -e 's/_\([0-9]*\)_\([0-9]*\)$/-\1.\2/' -e 's/_/-/g')
       [ "$(ls src/lib/${pre}_la-*.lo | wc -l)" = "$(ls src/lib/${pre}_la-*.o | wc -l)" ] ||
         { echo "$n: $pre: not all compiled" >&2; exit 1; }
       rm -f "$STAGE/lib/$lib.a"; $AR rcs "$STAGE/lib/$lib.a" src/lib/${pre}_la-*.o
     done
   else
     lib=$(sed -n 's/^lib_LTLIBRARIES = \([^ ]*\)\.la.*/\1/p' src/lib/Makefile)
     [ -n "$lib" ] && [ "$(ls src/lib/*.lo | wc -l)" = "$(ls src/lib/*.o | wc -l)" ] ||
       { echo "$n: not all compiled" >&2; exit 1; }
     rm -f "$STAGE/lib/$lib.a"; $AR rcs "$STAGE/lib/$lib.a" src/lib/*.o
   fi &&
   make -C inc install DESTDIR="$SYSROOT" >> "../$n.log" 2>&1 &&
   for pc in *.pc; do case $pc in *-uninstalled.pc|*generators*) ;; *) cp "$pc" "$STAGE/lib/pkgconfig/" ;; esac; done &&
   # (librevenge's headers aren't installed by make here: copied)
   inc=$(sed -n 's|^Cflags: -I${includedir}/\([^ ]*\).*|\1|p' $(ls *.pc | grep -v uninstalled | head -1)) &&
   for h in inc/*/; do
     [ -d "$STAGE/include/$inc/$(basename "$h")" ] || { mkdir -p "$STAGE/include/$inc" && cp -r "$h" "$STAGE/include/$inc/"; }
   done &&
   find "$STAGE/include/$inc" -name 'Makefile*' -delete) ||
     fail "$SRC/$n.log"
}
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

# ---- WordPerfect and Works: the libraries (wpdlib, above) ----
if [ ! -f "$STAGE/lib/librevenge-0.0.a" ]; then
  wpdlib librevenge librevenge_0.0.5.orig.tar.bz2 librevenge-0.0.5 --disable-generators --disable-fuzzers
fi
if [ ! -f "$STAGE/lib/libwpd-0.10.a" ]; then
  wpdlib libwpd libwpd_0.10.3.orig.tar.xz libwpd-0.10.3 --disable-fuzzers --disable-tools
fi
if [ ! -f "$STAGE/lib/libwps-0.4.a" ]; then
  wpdlib libwps libwps_0.4.14.orig.tar.xz libwps-0.4.14 --disable-fuzzers --disable-tools
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

# ---- GtkMathView 0.8.0 (equations: the mathview plugin) ----
# Its core and libxml2 front end only (no GTK 2 widget, SVG, PostScript,
# t1lib or popt), with Debian's patches (but the man page's and ppc64el's)
if [ ! -f "$STAGE/lib/libmathview_frontend_libxml2.a" ]; then
  log GtkMathView
  M=$SRC/gtkmathview-0.8.0
  rm -rf "$M" "$SRC/gtkmathview-debian"; tar -xzf "$DL/gtkmathview_0.8.0.orig.tar.gz" -C "$SRC"
  mkdir -p "$SRC/gtkmathview-debian"; tar -xJf "$DL/gtkmathview_0.8.0-14.debian.tar.xz" -C "$SRC/gtkmathview-debian"
  for p in $(grep -v -e '^#' -e ppc64el -e 0002 "$SRC/gtkmathview-debian/debian/patches/series"); do
    patch -d "$M" -p1 -s < "$SRC/gtkmathview-debian/debian/patches/$p"
  done
  fresh_config_sub "$M"
  (cd "$M" && CFLAGS="$RO_CFLAGS" CXXFLAGS="$RO_CFLAGS -std=gnu++98 -Wno-deprecated-declarations" \
     PKG_CONFIG="pkg-config --static" ./configure --host=$HOST --build=$BUILD --prefix=$PREFIX --libdir=$PREFIX/lib \
     --disable-shared --enable-static --disable-gtk --disable-svg --disable-ps --disable-gmetadom \
     --enable-libxml2 --disable-libxml2-reader --disable-custom-reader --without-t1lib --without-popt \
     --disable-gcc-pch > ../gtkmathview.log 2>&1 &&
   # (static libraries: no C++ run-time libraries for libtool to read the
   # toolchain's .la files for)
   sed -i 's/^postdeps=.*/postdeps=""/' libtool &&
   sed -i 's/^  -lstdc++ \\$/  \\/' src/Makefile src/view/Makefile &&
   { make -j$JOBS -k >> ../gtkmathview.log 2>&1 || true; }) &&
  # the front end's library: its libtool link reads libxml2.la, which
  # names the toolchain's machine; made from its objects instead
  (cd "$M" && [ -f src/.libs/libmathview.a ] && [ -f src/frontend/libxml2/.libs/libfrontend_libxml2.a ] &&
   ! grep -q " error: " ../gtkmathview.log &&
   { make -k install DESTDIR="$SYSROOT" >> ../gtkmathview.log 2>&1 || true; }) &&
  (cd "$M" && O=$SRC/gtkmathview-fe && rm -rf "$O" && mkdir "$O" && cd "$O" &&
   $AR x "$M/src/frontend/libxml2/.libs/libfrontend_libxml2.a" &&
   rm -f "$STAGE/lib/libmathview_frontend_libxml2.a" &&
   $AR rcs "$STAGE/lib/libmathview_frontend_libxml2.a" *.o "$M"/src/view/libmathview_frontend_libxml2_la-*.o &&
   cp "$M/src/.libs/libmathview.a" "$STAGE/lib/" && rm -rf "$O" "$STAGE"/lib/libmathview*.la) ||
    fail "$SRC/gtkmathview.log"
  grep -q " error: " "$SRC/gtkmathview.log" && fail "$SRC/gtkmathview.log"
  # its .pc files without the build machine's paths
  sed -i -e 's|^Libs:.*|Libs: -L${libdir} -lmathview_frontend_libxml2|' \
         -e 's|^Cflags: .*-I${includedir}/gtkmathview|Cflags: -I${includedir}/gtkmathview|' \
         "$STAGE/lib/pkgconfig/mathview-frontend-libxml2.pc"
fi

# ---- AbiWord ----
V=3.0.8
AB=$SRC/AbiWord-release-$V
PLUGINS="opendocument openxml docbook latex mswrite applix clarisworks t602 wml xslfo epub eml hrtext iscii kword mif opml pdb sdw s5 passepartout goffice hancom openwriter presentation urldict google wikipedia aiksaurus wordperfect mathview"
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
# GOffice's own plugins (the chart types, trend lines...) for the goffice
# plugin's charts, linked in and registered with GLib's static module
# registry (tools/mkstaticmodules.py, as for Gnumeric, whose build
# compiled them)
GO=$SRC/goffice-0.10.57
module_file() {  # PLUGIN.XML: its loader's module_file
  grep -o 'value="[^"]*" name="module_file"\|name="module_file" value="[^"]*"' "$1" |
    sed -E 's/.*value="([^"]*)".*/\1/' | head -1
}
GOMODS=()
for x in "$GO"/plugins/*/plugin.xml; do
  d=$(dirname "$x"); n=$(basename "$d"); mf=$(module_file "$x")
  [ "$n" != lasem ] || continue
  [ -n "$mf" ] && ls "$d"/*.o > /dev/null 2>&1 || continue
  GOMODS+=("$n/$mf=$(ls "$d"/*.o | tr '\n' ,)")
done
[ ${#GOMODS[@]} -gt 5 ] || { echo "build-abiword.sh: GOffice's plugins aren't built (build-gnumeric.sh)" >&2; exit 1; }
ST=$SRC/abiword-static
rm -rf "$ST"
python3 "$REPO_DIR/tools/mkstaticmodules.py" --nm $HOST-nm --objcopy $HOST-objcopy --ar $AR \
  --out "$ST" "${GOMODS[@]}" > "$ST.archives" || { echo "build-abiword.sh: GOffice's plugins" >&2; exit 1; }
$CC $RO_CFLAGS $(pkg-config --cflags gmodule-2.0) -c "$ST/staticmodules.c" -o "$ST/staticmodules.o"
LIBABI="$LIBABI $ST/staticmodules.o $(cat "$ST.archives" | tr '\n' ' ')"
log "${#GOMODS[@]} GOffice plugins built in"
# the include flags AbiWord's own sources are compiled with
# (relative to src: made absolute)
AB_CFLAGS="-DHAVE_CONFIG_H -I$AB -I$AB/src $(make -s -C "$AB/src" -f Makefile -f <(printf 'print-cppflags:\n\t@echo $(libabiword_3.0_la_CPPFLAGS)\n') print-cppflags |
  sed -e "s| -I\.\./| -I$AB/|g" -e "s| -I\./| -I$AB/src/|g")"
AB_LDLIBS="$(pkg-config --libs --static riscos-gtk-svg libgoffice-0.10 gtk+-3.0 libgsf-1 wv-1.0 libxslt fribidi librsvg-2.0 enchant-2 aiksaurus-1.0 libwpd-0.10 libwps-0.4 librevenge-0.0 mathview-frontend-libxml2) -ljpeg -lstdc++ -lm"

# ---- !AbiWord ----
VERSION=${VERSION:-3.0.8-riscos3}
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
# GtkMathView's licence (LGPL 3, which is the GPL 3 with its additions:
# both texts)
cp "$REPO_DIR/riscos/licences/LGPL-3.0.txt" "$APP/LicenceLGPL3,fff"
cp "$REPO_DIR/riscos/licences/GPL-3.0.txt" "$APP/LicenceGPL3,fff"
# the thesaurus's data (Aiksaurus, GPL 2 or later)
mkdir -p "$APP/share/aiksaurus"
# (its source tree may have been cleared since the library was built)
[ -f "$SRC/aiksaurus-1.2.1+dev-0.12/data/words.dat" ] ||
  tar -xzf "$DL/aiksaurus_1.2.1+dev-0.12.orig.tar.gz" -C "$SRC" aiksaurus-1.2.1+dev-0.12/data
cp "$SRC/aiksaurus-1.2.1+dev-0.12/data/words.dat" "$SRC/aiksaurus-1.2.1+dev-0.12/data/meanings.dat" "$APP/share/aiksaurus/"
# TeX's Computer Modern fonts (BaKoMa's TrueType ones, from LyX's
# package), which the equations' TeX shaper draws with, in the
# application's fonts (its fonts.conf reads the whole folder)
mkdir -p "$APP/fonts"
LX=$(mktemp -d)
(cd "$LX" && ar x "$DL/fonts-lyx_2.4.0~RC3-1build4_all.deb" && tar -xf data.tar.* ./usr/share/fonts/truetype/lyx ./usr/share/doc/fonts-lyx/copyright)
for f in cmr10 cmmi10 cmsy10 cmex10; do cp "$LX/usr/share/fonts/truetype/lyx/$f.ttf" "$APP/fonts/"; done
sed -n '/^License: BaKoMa/,/malyshev@mail.ihep.ru/p' "$LX/usr/share/doc/fonts-lyx/copyright" | sed 's/^ \.$//; s/^ //' > "$APP/fonts/LicenceBaKoMa,fff"
rm -rf "$LX"
# the equations' (GtkMathView's) settings and operator dictionaries,
# where the mathview plugin looks (ABIWORD_DATADIR/math)
mkdir -p "$APP/share/abiword-3.0/math"
cp "$AB"/plugins/mathview/data/*.xml "$AB"/plugins/mathview/data/*.dtd "$APP/share/abiword-3.0/math/"
# GOffice's plugins' descriptions and the files they read (the chart
# types: plot-types.xml...), where GOffice looks (GOFFICE_DIR:
# riscos-abiword.cpp)
for spec in "${GOMODS[@]}"; do
  n=${spec%%/*}; P=$GO/plugins/$n; D=$APP/goffice/lib/plugins/$n
  mkdir -p "$D"; cp "$P/plugin.xml" "$D/"
  for f in $(sed -n 's|.*<file>\([^<]*\)</file>.*|\1|p' "$P/plugin.xml"); do
    cp "$P/$f" "$D/"
  done
done
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
$CXX -std=c++11 $RO_CFLAGS "$SRC/riscos-abiword.o" $LIBABI -o "$SRC/abiword-prog" -static $AB_LDLIBS
export PREBUILT=$SRC/abiword-prog
# the toolbar's own icons and the application icon in the RISC OS style
# (tools/riscosicons_apps.py)
ICON_PACK=abiword exec "$HERE/build-app.sh" AbiWord abiword "$VERSION" "$APP"
