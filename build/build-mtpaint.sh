#!/bin/bash -e
# !mtPaint: mtPaint 3.50.14 (GPL v3), a painting program, on GTK 3 for
# RISC OS.
#   RCD_DIR=... SRC=... SYSROOT=... build/build-mtpaint.sh
# Unpacks the source into $SRC, applies patches/mtpaint-3.50.14-riscos.diff
# (a RISC OS Save/Open box as its file picker; Adjust as the right button;
# memory size from OS_ReadMemMapInfo), configures it with its own configure
# (GTK 3, PNG, JPEG, FreeType; no TIFF, WebP, JPEG 2000 or LittleCMS),
# builds it, links its objects with apps/mtpaint/riscos-mtpaint.c (main:
# the icon bar, the Log, RISC OS names) and packages out/!mtPaint and
# out/mtPaint-$VERSION.zip (build-app.sh).
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/env.sh"
[ "$TARGET" = riscos ] || { echo "build-mtpaint.sh: RISC OS only" >&2; exit 1; }
V=3.50.14
VERSION=${VERSION:-3.50.14-riscos10}
M=$SRC/mtpaint-$V
rm -rf "$M" "$SRC/mtPaint-"*
tar -xzf "$DL/mtpaint_$V.orig.tar.gz" -C "$SRC"
mv "$SRC"/mtPaint-* "$M"        # the tarball's folder is named by its commit
(cd "$M" && patch -p1 -s < "$REPO_DIR/patches/mtpaint-$V-riscos.diff")

# its configure asks pkg-config for each library separately: --static, so
# GTK's dependencies (libepoxy and the rest) are all on the link line
PKW=$SRC/mtpaint-pkg-config
printf '#!/bin/sh\nexec pkg-config --static "$@"\n' > "$PKW"
chmod +x "$PKW"
cd "$M"
CFLAGS="$RO_CFLAGS" ./configure gtk3 --host=$HOST cflags nojp2 notiff nowebp nolcms jpeg \
  PKG_CONFIG="$PKW" --prefix=/mtpaint > "$SRC/mtpaint-configure.log" 2>&1 \
  || { tail -30 "$SRC/mtpaint-configure.log"; exit 1; }
# make compiles and links; the link (stripped, with mtPaint's main) is
# done again below with our main
make -j"$(nproc)" > "$SRC/mtpaint-make.log" 2>&1 || { grep -E "error" "$SRC/mtpaint-make.log" | head -20; exit 1; }

CFLAG=$(sed -n 's/^CFLAG = //p' _conf.txt)
$HOST-objcopy --redefine-sym main=mtpaint_main src/main.o src/main-riscos.o
# beside mtPaint's sources: its "png.h" before libpng's
cp "$REPO_DIR/apps/mtpaint/riscos-mtpaint.c" src/
eval $CC $CFLAG -Wall -DRISCOS_PORT_VERSION=\\\"$VERSION\\\" \
  -c src/riscos-mtpaint.c -o src/riscos-mtpaint.o
$CC $RO_CFLAGS src/riscos-mtpaint.o src/main-riscos.o $(ls src/*.o | grep -v -e '/main\.o$' -e 'main-riscos\.o$' -e 'riscos-mtpaint\.o$') \
  -o "$SRC/mtpaint-prog" -static -ljpeg -lfreetype $(pkg-config --libs --static gtk+-3.0)

APP=$SRC/mtpaint-app
rm -rf "$APP"; cp -r "$REPO_DIR/apps/mtpaint/app" "$APP"
cp "$M/COPYING" "$APP/Licence,fff"
export PREBUILT=$SRC/mtpaint-prog
exec "$HERE/build-app.sh" mtPaint mtpaint "$VERSION" "$APP"
