#!/bin/bash -e
# Spell checking for GTK programs on RISC OS: hunspell 1.7.2 (LGPL/MPL/GPL)
# and enchant 2.3.3 (LGPL), built into $STAGE as static libraries, with
# enchant's hunspell provider linked into libenchant-2.a (patches/
# enchant-2.3.3-riscos.diff: no dlopen; dictionaries in
# <Choices$Write>.GTK.Dictionaries for every program, then each program's
# share/hunspell). Also unpacks the British English dictionary (LGPL,
# LibreOffice's, from Ubuntu's hunspell-en-gb) into $SRC/dictionaries for
# the packages to carry.
#   RCD_DIR=... SRC=... SYSROOT=... build/build-spell.sh  (TARGET=linuxarm too)
# Programs then use pkg-config --static enchant-2.
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/env.sh"
JOBS=${JOBS:-$(nproc)}
log() { echo "build-spell.sh: $*"; }
fail() { grep -E 'error|Error' "$1" | head -20; echo "build-spell.sh: failed, see $1" >&2; exit 1; }

# ---- hunspell ----
if [ ! -f "$STAGE/lib/libhunspell-1.7.a" ] || [ ! -f "$STAGE/lib/pkgconfig/hunspell.pc" ]; then
  log hunspell
  H=$SRC/hunspell-1.7.2
  rm -rf "$H"; tar -xzf "$DL/hunspell_1.7.2+really1.7.2.orig.tar.gz" -C "$SRC"
  (cd "$H" && CFLAGS="$RO_CFLAGS" CXXFLAGS="$RO_CFLAGS" ./configure --host=$HOST --build=$BUILD \
     --prefix=$PREFIX --libdir=$PREFIX/lib --disable-shared --enable-static --disable-nls \
     --without-ui --without-readline > ../hunspell.log 2>&1 &&
   # (libtool can't link C++ with crossdev's .la files: the objects are
   # archived here)
   { make -k -j$JOBS -C src/hunspell libhunspell-1.7.la >> ../hunspell.log 2>&1 || true; } &&
   n_lo=$(ls src/hunspell/*.lo | wc -l) && [ "$n_lo" -ge 10 ] && [ "$n_lo" = "$(ls src/hunspell/*.o | wc -l)" ] &&
   rm -f "$STAGE/lib/libhunspell-1.7.a" && $AR rcs "$STAGE/lib/libhunspell-1.7.a" src/hunspell/*.o &&
   make -C src/hunspell install-libhunspell_1_7_includeHEADERS DESTDIR="$SYSROOT" >> ../hunspell.log 2>&1 &&
   mkdir -p "$STAGE/lib/pkgconfig" && cp hunspell.pc "$STAGE/lib/pkgconfig/" &&
   # C++: RISC OS's static links want libstdc++ after it (the tests'
   # programs link the Linux one themselves, dynamically)
   { [ "$TARGET" != riscos ] || echo 'Libs.private: -lstdc++' >> "$STAGE/lib/pkgconfig/hunspell.pc"; }) ||
     fail "$SRC/hunspell.log"
fi

# ---- enchant, with the hunspell provider built in ----
if [ ! -f "$STAGE/lib/libenchant-2.a" ] || [ ! -f "$STAGE/lib/pkgconfig/enchant-2.pc" ]; then
  log enchant
  E=$SRC/enchant-2.3.3
  rm -rf "$E"; tar -xzf "$DL/enchant-2_2.3.3.orig.tar.gz" -C "$SRC"
  patch -d "$E" -p1 -s < "$REPO_DIR/patches/enchant-2.3.3-riscos.diff"
  (cd "$E" && CFLAGS="$RO_CFLAGS -DENCHANT_RISCOS" CXXFLAGS="$RO_CFLAGS -DENCHANT_RISCOS" \
     PKG_CONFIG="pkg-config --static" ./configure --host=$HOST --build=$BUILD \
     --prefix=$PREFIX --libdir=$PREFIX/lib --disable-shared --enable-static \
     --with-hunspell --without-nuspell --without-aspell --without-hspell --without-voikko \
     --without-applespell --without-zemberek > ../enchant.log 2>&1 &&
   { make -k -j$JOBS -C lib >> ../enchant.log 2>&1 || true; } &&
   { make -k -j$JOBS -C src libenchant-2.la >> ../enchant.log 2>&1 || true; } &&
   # the provider, its entry point renamed (lib.c calls them directly)
   $CXX $RO_CFLAGS -DENCHANT_RISCOS -DHAVE_CONFIG_H -I. -Isrc -Ilib $(pkg-config --cflags glib-2.0 hunspell) \
     -Dinit_enchant_provider=init_enchant_provider_hunspell \
     -c providers/enchant_hunspell.cpp -o providers/enchant_hunspell_builtin.o >> ../enchant.log 2>&1 &&
   n_lo=$(ls src/*.lo | wc -l) && [ "$n_lo" -ge 2 ] &&
   rm -f "$STAGE/lib/libenchant-2.a" &&
   $AR rcs "$STAGE/lib/libenchant-2.a" $(ls src/*.lo | sed 's/\.lo$/.o/') $(ls lib/*.o 2>/dev/null) \
     providers/enchant_hunspell_builtin.o &&
   make -C src install-libenchant_includeHEADERS DESTDIR="$SYSROOT" >> ../enchant.log 2>&1 &&
   make install-pkgconfigDATA DESTDIR="$SYSROOT" >> ../enchant.log 2>&1) || fail "$SRC/enchant.log"
  # static linking: the provider's library comes with it
  sed -i 's|^Requires\.private:.*|& hunspell|' "$STAGE/lib/pkgconfig/enchant-2.pc"
  grep -q '^Requires.private:' "$STAGE/lib/pkgconfig/enchant-2.pc" ||
    echo 'Requires.private: glib-2.0 hunspell' >> "$STAGE/lib/pkgconfig/enchant-2.pc"
fi

# ---- the British English dictionary ----
DD=$SRC/dictionaries
if [ ! -f "$DD/en_GB.dic" ]; then
  log "en_GB dictionary"
  T=$(mktemp -d)
  (cd "$T" && ar x "$DL/hunspell-en-gb_24.2.1-1_all.deb" && tar --zstd -xf data.tar.zst)
  rm -rf "$DD"; mkdir -p "$DD"
  cp "$T/usr/share/hunspell/en_GB.dic" "$T/usr/share/hunspell/en_GB.aff" "$DD/"
  zcat "$T/usr/share/doc/hunspell-en-gb/README_en_GB.txt.gz" > "$DD/README_en_GB.txt"
  rm -rf "$T"
fi
log done
