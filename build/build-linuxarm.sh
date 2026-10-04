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
    -Wl,-Bdynamic -lresolv -ldl -lm -pthread
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
  -Wl,-Bdynamic -lresolv -ldl -lm -pthread
HA=$REPO_DIR/out/hello-linuxarm-app
rm -rf "$HA"; mkdir -p "$HA/fonts" "$HA/share/themes/RISCOS"
unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf' \
  'dejavu-fonts-ttf-2.37/ttf/DejaVuSans-Bold.ttf' -d "$HA/fonts"
"$REPO_DIR/tests/render/mkfontsconf.sh" "$HA/fonts" "$HA/fontcache" > "$HA/fonts/fonts.conf"
cp -r "$REPO_DIR/theme/gtk-3.0" "$HA/share/themes/RISCOS/"

# The theme test on the fake Wimp: every page and the File menu.
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations -DTHEMETEST_NO_MAIN \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/themetest-fake.c" "$REPO_DIR/tests/gtk/themetest.c" "$REPO_DIR/tests/gtk/riscos-freopen.c" \
  -o "$REPO_DIR/out/themetest-fake-linuxarm" -Wl,--wrap=freopen \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lresolv -ldl -lm -pthread
TT=$REPO_DIR/out/themetest-linuxarm
rm -rf "$TT"; mkdir -p "$TT"

scratch=$(mktemp -d)
GLIBTEST_SKIP_ICONV=1 run glibtest "$scratch"
run rendertest "$scratch" "$FONTS/fonts.conf"
run hellotest "$HA" "$REPO_DIR/out/hellotest.ppm"
run themetest-fake "$HA" "$TT"
rm -rf "$scratch"
echo; [ $failed = 0 ] && echo "linux-arm: all tests passed" || echo "linux-arm: $failed test(s) FAILED"
exit $failed
