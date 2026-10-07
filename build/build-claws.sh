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
# patches/claws-mail-*-riscos.diff, configures (no plugins, no libetpan
# yet: POP3 and SMTP; TLS through src/common/ssl_acornssl.c, compiled
# against the patch's gnutls-acornssl headers) and makes it.
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
  --disable-dbus --disable-ldap --disable-jpilot --disable-networkmanager --disable-libetpan \
  --disable-valgrind --disable-svg --disable-tests --with-password-encryption=old \
  $DISABLE_PLUGINS > ../claws-configure.log 2>&1 || { tail -20 ../claws-configure.log; exit 1; }
M="-f Makefile -f $REPO_DIR/apps/claws/objects.mk"
{ make -j$JOBS -C src/common && make -j$JOBS -C src/gtk && make -C src $M claws-built &&
  make -j$JOBS -C src $M claws-objects; } > ../claws-make.log 2>&1 ||
  { grep -E 'error|Error' ../claws-make.log | head -30; exit 1; }
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
VERSION=${VERSION:-0.1}
OBJS=$(ls "$C"/src/*.o)
exec "$HERE/build-app.sh" ClawsMail claws-mail "$VERSION" "$REPO_DIR/apps/claws/app" \
  -DHAVE_CONFIG_H -DRISCOS_PORT_VERSION=\"$VERSION\" -I"$C" -I"$C/src" -I"$C/src/common" -I"$C/src/gtk" \
  "$REPO_DIR/apps/claws/riscos-claws.c" $OBJS "$C/src/gtk/.libs/libclawsgtk.a" -lnettle
