#!/bin/bash -e
# !ClawsMail: Claws Mail 4.4.0 (GPL v3) on GTK 3 for RISC OS, its TLS
# through RISC OS's AcornSSL module.
#   RCD_DIR=... SRC=... SYSROOT=... build/build-claws.sh
#   TARGET=linuxarm ...: the same build for the tests (stops after make;
#   tests/gtk/claws-fake.c links it with apps/claws/riscos-claws.c)
# On RISC OS it then links apps/claws/riscos-claws.c (the icon bar, Choices,
# the Log) with Claws Mail's objects and packages out/!ClawsMail and
# out/ClawsMail-$VERSION.zip (build-app.sh). __riscos is defined for the
# linuxarm build too, so the tests run the RISC OS code paths (no fork()
# for name lookups, no Unix domain socket).
# Builds nettle into $STAGE if it isn't there (Claws Mail's DES, for its
# own stored-password encryption), unpacks Claws Mail into $SRC, applies
# patches/claws-mail-*-riscos.diff, builds libetpan 1.9.4 into $STAGE if it
# isn't there (IMAP), configures (no plugins; POP3, IMAP, SMTP; all TLS
# through src/common/ssl_acornssl.c, compiled against the patch's
# gnutls-acornssl headers, which libetpan is built against too) and makes it.
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/env.sh"
V=4.4.0
C=$SRC/claws-mail-$V
JOBS=${JOBS:-$(nproc)}

if [ ! -f "$STAGE/lib/libnettle.a" ]; then
  N=$SRC/nettle-3.10.2
  rm -rf "$N"; tar -xzf "$DL/nettle_3.10.2.orig.tar.gz" -C "$SRC"
  (cd "$N" && CFLAGS="$RO_CFLAGS" ./configure --host=$HOST --build=$BUILD --prefix=$PREFIX \
     --libdir=$PREFIX/lib --disable-shared --enable-static --disable-public-key --disable-assembler \
     --disable-documentation --disable-openssl > ../nettle.log 2>&1 &&
   make -j$JOBS libnettle.a >> ../nettle.log 2>&1 &&
   make install-headers install-static install-pkgconfig DESTDIR="$SYSROOT" >> ../nettle.log 2>&1)
fi

rm -rf "$C"
tar -xf "$DL/claws-mail_$V.orig.tar.xz" -C "$SRC"
patch -d "$C" -p1 -s < "$REPO_DIR/patches/claws-mail-$V-riscos.diff"

# libetpan (IMAP) into $STAGE if it isn't there, built for GnuTLS against
# Claws Mail's GnuTLS-subset headers (patched in above): its TLS is then
# AcornSSL too, through ssl_acornssl.c, which the final link supplies.
# No OpenSSL, SASL, Berkeley DB, curl, expat or zlib.
if [ ! -f "$STAGE/lib/libetpan.a" ]; then
  E=$SRC/libetpan-1.9.4
  rm -rf "$E"; tar -xf "$DL/libetpan_1.9.4.orig.tar.xz" -C "$SRC"
  (cd "$E" && CFLAGS="$RO_CFLAGS -Wno-deprecated-declarations" \
     CPPFLAGS="-I$C/src/common/gnutls-acornssl" ac_cv_lib_gnutls_gnutls_global_deinit=yes \
     ./configure --host=$HOST --build=$BUILD --prefix=$PREFIX --libdir=$PREFIX/lib \
     --disable-shared --enable-static --with-gnutls --without-openssl --without-sasl \
     --disable-db --without-curl --without-expat --without-zlib --disable-lockfile \
     > ../libetpan.log 2>&1 &&
   sed -i 's/^SUBDIRS = .*/SUBDIRS = include src/' Makefile &&
   # linked as C (automake picks the C++ linker for a Windows-only
   # dummy.cpp, and libtool's C++ run-time libraries then trip over the
   # toolchain's .la files)
   sed -i '/^libetpan_la_LINK/,/-o \$@/{s/--tag=CXX/--tag=CC/;s/\$(CXXLD)/$(CC)/;s/\$(AM_CXXFLAGS) \$(CXXFLAGS)/$(AM_CFLAGS) $(CFLAGS)/}' src/Makefile &&
   make -j$JOBS >> ../libetpan.log 2>&1 &&
   make install DESTDIR="$SYSROOT" >> ../libetpan.log 2>&1) ||
    { grep -E 'error|Error' "$SRC/libetpan.log" | head -20; exit 1; }
  # the GnuTLS functions are Claws Mail's (no -lgnutls); no build paths
  sed -i 's/ -lgnutls//; s/^Libs:.*/Libs: -L${libdir} -letpan/' "$STAGE/lib/pkgconfig/libetpan.pc"
  grep -q "define USE_GNUTLS 1" "$E/config.h" || { echo "build-claws.sh: libetpan without TLS" >&2; exit 1; }
fi
# gumbo (the HTML parser litehtml uses) into $STAGE if it isn't there:
# its C files straight into libgumbo.a (the tarball has no configure)
if [ ! -f "$STAGE/lib/libgumbo.a" ]; then
  G=$SRC/gumbo-parser
  rm -rf "$G"; tar -xf "$DL/gumbo-parser_0.12.0+dfsg.orig.tar.xz" -C "$SRC"
  mkdir -p "$G/obj"
  (cd "$G/obj" && for f in ../src/*.c; do $CC $RO_CFLAGS -std=c99 -c "$f" || exit 1; done &&
   $AR rcs libgumbo.a *.o) > "$SRC/gumbo.log" 2>&1 || { tail -20 "$SRC/gumbo.log"; exit 1; }
  mkdir -p "$STAGE/lib/pkgconfig" "$STAGE/include"
  cp "$G/obj/libgumbo.a" "$STAGE/lib/"
  cp "$G/src/gumbo.h" "$G/src/tag_enum.h" "$STAGE/include/"
  printf 'prefix=%s\nlibdir=${prefix}/lib\nincludedir=${prefix}/include\n\nName: gumbo\nDescription: HTML5 parser\nVersion: 0.12.0\nLibs: -L${libdir} -lgumbo\nCflags: -I${includedir}\n' \
    "$PREFIX" > "$STAGE/lib/pkgconfig/gumbo.pc"
fi
cd "$C"
DISABLE_PLUGINS=$(./configure --help | grep -oE -- '--disable-[a-z_]+-plugin' | tr '\n' ' ')
# UnixLib 5.0.3.3 has fgets_unlocked (what configure checks) but not
# fputs_unlocked, which Claws Mail then uses too: the plain stdio calls
[ "$TARGET" = riscos ] && export ac_cv_func_fgets_unlocked=no
CFLAGS="$RO_CFLAGS -Wno-deprecated-declarations" \
CPPFLAGS="-D__riscos=1 -DUSE_GNUTLS=1 -DCLAWS_ACORNSSL=1 -I$C/src/common/gnutls-acornssl" \
LIBS="$(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g')" \
./configure --host=$HOST --build=$BUILD --prefix=$PREFIX --disable-shared --enable-static \
  --disable-nls --disable-manual --disable-libsm --disable-ipv6 --disable-gnutls --disable-oauth2 \
  --disable-enchant --disable-compface --disable-pthread --disable-startup-notification \
  --disable-dbus --disable-ldap --disable-jpilot --disable-networkmanager \
  --disable-valgrind --disable-svg --disable-tests --with-password-encryption=old \
  $DISABLE_PLUGINS > ../claws-configure.log 2>&1 || { tail -20 ../claws-configure.log; exit 1; }
M="-f Makefile -f $REPO_DIR/apps/claws/objects.mk"
{ make -j$JOBS -C src/common && make -j$JOBS -C src/gtk && make -j$JOBS -C src/etpan && make -C src $M claws-built &&
  make -j$JOBS -C src $M claws-objects; } > ../claws-make.log 2>&1 ||
  { grep -E 'error|Error' ../claws-make.log | head -30; exit 1; }
# The litehtml viewer plug-in (HTML mail), linked into the program, not
# loaded: riscos-claws.c calls its lh_plugin_init. Its libcurl image
# loader is replaced by apps/claws/litehtml/http.{h,cpp} (sockets, and
# AcornSSL for https).
L=$C/src/plugins/litehtml_viewer
cp "$REPO_DIR/apps/claws/litehtml/http.h" "$REPO_DIR/apps/claws/litehtml/http.cpp" "$L/"
LHF="-DHAVE_CONFIG_H -D__riscos=1 -DUSE_GNUTLS=1 -DCLAWS_ACORNSSL=1 -I$C/src/common/gnutls-acornssl
  -I$C -I$C/src -I$C/src/common -I$C/src/gtk -I$L -I$L/litehtml $(pkg-config --cflags gtk+-3.0 gumbo)"
rm -rf "$L/obj"; mkdir -p "$L/obj"
(cd "$L/obj" &&
 for f in ../litehtml/*.cpp; do echo "$f"; done | xargs -P$JOBS -I{} $CXX $RO_CFLAGS -std=c++11 -w -I$L -I$L/litehtml $(pkg-config --cflags gumbo) -c {} &&
 for f in container_linux container_linux_images http lh_widget; do
   $CXX $RO_CFLAGS -std=c++11 -Wno-deprecated-declarations $LHF -c ../$f.cpp || exit 1; done &&
 for f in lh_prefs lh_viewer; do
   $CC $RO_CFLAGS -std=gnu99 -Wno-deprecated-declarations $LHF -c ../$f.c || exit 1; done &&
 $CC $RO_CFLAGS -Wno-deprecated-declarations $LHF -Dplugin_init=lh_plugin_init -Dplugin_done=lh_plugin_done \
   -Dplugin_name=lh_plugin_name -Dplugin_desc=lh_plugin_desc -Dplugin_type=lh_plugin_type \
   -Dplugin_licence=lh_plugin_licence -Dplugin_version=lh_plugin_version \
   -Dplugin_provides=lh_plugin_provides -c ../plugin.c &&
 rm -f ../liblhviewer.a && $AR rcs ../liblhviewer.a *.o) > "$SRC/lh-make.log" 2>&1 ||
  { grep -E 'error|Error' "$SRC/lh-make.log" | head -30; exit 1; }
echo "build-claws.sh: built Claws Mail's objects in $C/src ($TARGET)"
[ "$TARGET" = riscos ] || exit 0

# Claws Mail's casts that could make unaligned loads or stores (RISC OS
# stops on them): the ones checked by hand are in unaligned-safe.txt
"$RCD_DIR/checks/check-unaligned.sh" -s "$REPO_DIR/apps/claws/unaligned-safe.txt" \
  -DHAVE_CONFIG_H -D__riscos=1 -DUSE_GNUTLS=1 -DCLAWS_ACORNSSL=1 -I"$C/src/common/gnutls-acornssl" \
  -I"$C" -I"$C/src" -I"$C/src/common" -I"$C/src/gtk" $(pkg-config --cflags gtk+-3.0) \
  $(ls "$C"/src/*.c "$C"/src/common/*.c "$C"/src/gtk/*.c | grep -v ssl_acornssl)

# !ClawsMail: Claws Mail's objects (main.o included) and its libclawsgtk,
# with riscos-claws.c
VERSION=${VERSION:-4.4.0-riscos21}
OBJS=$(ls "$C"/src/*.o)
exec "$HERE/build-app.sh" ClawsMail claws-mail "$VERSION" "$REPO_DIR/apps/claws/app" \
  -DHAVE_CONFIG_H -DRISCOS_PORT_VERSION=\"$VERSION\" -I"$C" -I"$C/src" -I"$C/src/common" -I"$C/src/gtk" \
  "$REPO_DIR/apps/claws/riscos-claws.c" $OBJS "$C/src/etpan/.libs/libclawsetpan.a" "$C/src/gtk/.libs/libclawsgtk.a" \
  "$L/liblhviewer.a" -letpan -lnettle -lgumbo -lstdc++
