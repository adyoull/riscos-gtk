#!/bin/bash -e
# The tests on Linux: build the whole dependency stack again as ARM Linux
# code (TARGET=linuxarm: same sources, same patches, Cortex-A8/VFPv3,
# -mno-unaligned-access; see env.sh), link each test statically against it
# (-no-pie, glibc dynamic) and run it under riscos-crossdev's
# qemu-arm-aligntrap, which stops on an unaligned access in the program's
# own code (all our libraries; glibc is exempt) or a VFPv4 instruction.
# This is riscos-crossdev's "layer 4": it can't see UnixLib or the Wimp.
#
#   build/build-linuxarm.sh            # libraries (if not built), then tests
#   build/build-linuxarm.sh tests      # tests only
export TARGET=linuxarm
HERE=$(cd "$(dirname "$0")" && pwd)
[ "${1:-}" = tests ] || "$HERE/build-deps.sh"
. "$HERE/env.sh"
mkdir -p "$REPO_DIR/out"
FONTS=$REPO_DIR/out/fonts-linuxarm

# run TEST [ARGS]: one test under qemu-arm-aligntrap; failures counted.
failed=0
run() {
  local t=$1; shift
  echo; echo "=== linux-arm: $t under qemu-arm-aligntrap"
  QEMU=$GCCSDK_ENV/bin/qemu-arm-aligntrap "$RCD_DIR"/qemu/aligntrap.sh \
    "$REPO_DIR/out/$t-linuxarm" "$@" || failed=$((failed + 1))
}
build() {  # NAME SOURCE PKGS...
  local n=$1 s=$2; shift 2
  $CC $RO_CFLAGS -no-pie -Wall $(pkg-config --cflags "$@") "$s" \
    -o "$REPO_DIR/out/$n-linuxarm" \
    -Wl,-Bstatic $(pkg-config --libs --static "$@" | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
    -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
}

build glibtest "$REPO_DIR/tests/glib/glibtest.c" gio-2.0
build rendertest "$REPO_DIR/tests/render/rendertest.c" pangocairo gdk-pixbuf-2.0

# DejaVu Sans and a fonts.conf for the render test (as on RISC OS, where
# the app ships the same font).
# The font folder's name has < > & in it, like the RISC OS paths
# (/<GTKTest$Dir>/fonts), so the XML escaping in mkfontsconf.sh is tested.
rm -rf "$FONTS"; mkdir -p "$FONTS"
FD="$FONTS/<App\$Dir>&fonts"
unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf' -d "$FD"
"$REPO_DIR/tests/render/mkfontsconf.sh" "$FD" "$FONTS/cache" > "$FONTS/fonts.conf"

# M2: GTK with the RISC OS backend on the fake Wimp (libgdk has the fake in
# it on this target). The app folder is laid out like !GTKHello's.

$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations -DHELLO_NO_MAIN \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/hellotest.c" "$REPO_DIR/tests/gtk/hello.c" "$REPO_DIR/tests/gtk/riscos-freopen.c" \
  -o "$REPO_DIR/out/hellotest-linuxarm" -Wl,--wrap=freopen \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
HA=$REPO_DIR/out/hello-linuxarm-app
rm -rf "$HA"; mkdir -p "$HA/fonts" "$HA/share/themes/RISCOS"
unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf' \
  'dejavu-fonts-ttf-2.37/ttf/DejaVuSans-Bold.ttf' -d "$HA/fonts"
"$REPO_DIR/tests/render/mkfontsconf.sh" "$HA/fonts" "$HA/fontcache" > "$HA/fonts/fonts.conf"
cp -r "$REPO_DIR/theme/gtk-3.0" "$HA/share/themes/RISCOS/"
# icons: the RISCOS theme (Adwaita's, for the names GTK and programs use;
# needs the host's gtk-encode-symbolic-svg, libgtk-3-bin) and an empty
# hicolor; the desktop's sprites are added at run time
ADW=$SRC/adwaita-icon-theme-3.36.1
[ -d "$ADW" ] || tar -xf "$DL/adwaita-icon-theme_3.36.1.orig.tar.xz" -C "$SRC"
python3 "$REPO_DIR/build/mkicons.py" "$ADW/Adwaita" "$REPO_DIR/theme/icons-list.txt" "$HA/share/icons"
# GTK's GSettings schemas (the file choosers, colour chooser, emoji
# chooser need them: GLib stops the program without), found through
# XDG_DATA_DIRS = the app's share directory
mkdir -p "$HA/share/glib-2.0/schemas"
glib-compile-schemas --strict --targetdir="$HA/share/glib-2.0/schemas" "$STAGE/share/glib-2.0/schemas"

# The theme test on the fake Wimp: every page and the File menu.
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations -DTHEMETEST_NO_MAIN \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/themetest-fake.c" "$REPO_DIR/tests/gtk/themetest.c" "$REPO_DIR/tests/gtk/riscos-freopen.c" \
  -o "$REPO_DIR/out/themetest-fake-linuxarm" -Wl,--wrap=freopen \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
TT=$REPO_DIR/out/themetest-linuxarm
rm -rf "$TT"; mkdir -p "$TT"

# Text fields on the fake Wimp: their menu, the clipboard with other
# tasks, dragging text in, out and between fields.
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations -DTHEMETEST_NO_MAIN \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/edittest-fake.c" "$REPO_DIR/tests/gtk/themetest.c" "$REPO_DIR/tests/gtk/riscos-freopen.c" \
  -o "$REPO_DIR/out/edittest-fake-linuxarm" -Wl,--wrap=freopen \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
ET=$REPO_DIR/out/edittest-linuxarm
rm -rf "$ET"; mkdir -p "$ET"

# A theme's tool sprites stored as the Raspberry theme's (8bpp, no
# palette, coloured by the Wimp's translation tables).
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations $(pkg-config --cflags gtk+-3.0) \
  "$REPO_DIR/tests/gtk/toolsprites-fake.c" -o "$REPO_DIR/out/toolsprites-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread

# Printing: the page split into Draw vectors and images (cairo/).
$CC $RO_CFLAGS -no-pie -Wall $(pkg-config --cflags cairo) \
  "$REPO_DIR/tests/gtk/printvec-fake.c" -o "$REPO_DIR/out/printvec-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static cairo | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread

# L3afpad (the first real GTK program) on the fake Wimp: its sources with
# its RISC OS patch, main renamed, driven by tests/gtk/l3afpad-fake.c
L3V=0.8.18.1.11
L3=$SRC/l3afpad-$L3V
rm -rf "$L3"; tar -xzf "$DL/l3afpad_$L3V.orig.tar.gz" -C "$SRC"
(cd "$L3" && patch -p1 -s < "$REPO_DIR/patches/l3afpad-$L3V-riscos.diff")
cp "$REPO_DIR/apps/l3afpad/config.h" "$L3/src/"
L3O=$REPO_DIR/out/l3afpad-objs-linuxarm
rm -rf "$L3O"; mkdir -p "$L3O"
for f in "$L3"/src/*.c "$REPO_DIR/apps/l3afpad/riscos-l3afpad.c"; do
  (cd "$L3O" && $CC $RO_CFLAGS -c -w -DHAVE_CONFIG_H -D__riscos=1 -Dmain=l3afpad_main \
    -DICONDIR="\"$REPO_DIR/apps/l3afpad/app/share/icons\"" -I"$L3/src" $(pkg-config --cflags gtk+-3.0) "$f")
done
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations $(pkg-config --cflags gtk+-3.0) \
  "$REPO_DIR/tests/gtk/l3afpad-fake.c" "$L3O"/*.o -o "$REPO_DIR/out/l3afpad-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
L3T=$REPO_DIR/out/l3afpad-linuxarm
rm -rf "$L3T"; mkdir -p "$L3T"

# Claws Mail on the fake Wimp (tests/gtk/claws-fake.c), fetching and
# sending through the fake AcornSSL to tests/claws/mailserver.py: its
# objects from build-claws.sh, main.c again with main renamed, and
# apps/claws/riscos-claws.c
"$REPO_DIR/build/build-claws.sh"
CS=$SRC/claws-mail-4.4.0/src
CO=$REPO_DIR/out/claws-objs-linuxarm
rm -rf "$CO"; mkdir -p "$CO"
rm -f "$CS/main.o"
make -s -C "$CS" main.o CPPFLAGS="-D__riscos=1 -DUSE_GNUTLS=1 -DCLAWS_ACORNSSL=1 -I$CS/common/gnutls-acornssl -Dmain=claws_main" > /dev/null
mv "$CS/main.o" "$CO/main.o"
(cd "$CO" && $CC $RO_CFLAGS -c -Wall -Wno-deprecated-declarations -DHAVE_CONFIG_H -D__riscos=1 \
  -I"$CS/.." -I"$CS" -I"$CS/common" -I"$CS/gtk" $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/apps/claws/riscos-claws.c" \
  "$REPO_DIR/tests/gtk/claws-fake-claws.c")
$CC $RO_CFLAGS -no-pie -Wall $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/claws-fake.c" \
  "$CO"/*.o $(ls "$CS"/*.o | grep -v '/main\.o$') "$CS/etpan/.libs/libclawsetpan.a" "$CS/gtk/.libs/libclawsgtk.a" "$CS/plugins/litehtml_viewer/liblhviewer.a" \
  -o "$REPO_DIR/out/claws-fake-linuxarm" -Wl,-Bstatic -letpan -lnettle -lgumbo $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
CT=$REPO_DIR/out/claws-linuxarm
rm -rf "$CT"; mkdir -p "$CT"
CP1=$((20000 + RANDOM % 20000)); CP2=$((CP1 + 1)); CP3=$((CP1 + 2)); CP4=$((CP1 + 3))

scratch=$(mktemp -d)
GLIBTEST_SKIP_ICONV=1 run glibtest "$scratch"
run rendertest "$scratch" "$FONTS/fonts.conf"
run hellotest "$HA" "$REPO_DIR/out/hellotest.ppm"
run themetest-fake "$HA" "$TT"
run edittest-fake "$HA" "$ET"
run toolsprites-fake
run printvec-fake
run l3afpad-fake "$HA" "$L3T"
python3 "$REPO_DIR/tests/claws/mailserver.py" "$CT" $CP1 $CP2 $CP3 $CP4 & mailserver=$!
for i in $(seq 50); do [ -f "$CT/server.ready" ] && break; sleep 0.1; done
CLAWS_RISCOS_THEME="$REPO_DIR/apps/claws/app/share/claws-mail/themes/RISCOS" run claws-fake "$HA" "$CT" $CP1 $CP2 $CP3 $CP4
kill $mailserver 2>/dev/null || true
rm -rf "$scratch"
echo; [ $failed = 0 ] && echo "linux-arm: all tests passed" || echo "linux-arm: $failed test(s) FAILED"
exit $failed
