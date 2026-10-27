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
unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" $(for f in $DEJAVU_FACES; do echo "dejavu-fonts-ttf-2.37/ttf/$f"; done) -d "$HA/fonts"
"$REPO_DIR/tests/render/mkfontsconf.sh" "$HA/fonts" "$HA/fontcache" > "$HA/fonts/fonts.conf"
cp -r "$REPO_DIR/theme/gtk-3.0" "$HA/share/themes/RISCOS/"
# icons: the RISCOS theme (riscos-gtk's own, then Adwaita's, for the names
# GTK and programs use: build/mkicons.py) and an empty hicolor; the
# desktop's sprites are added at run time
ADW=$SRC/adwaita-icon-theme-3.36.1
[ -d "$ADW" ] || tar -xf "$DL/adwaita-icon-theme_3.36.1.orig.tar.xz" -C "$SRC"
python3 "$REPO_DIR/build/mkicons.py" "$ADW/Adwaita" "$REPO_DIR/theme/icons-list.txt" "$HA/share/icons"
# GTK's GSettings schemas (the file choosers, colour chooser, emoji
# chooser need them: GLib stops the program without), found through
# XDG_DATA_DIRS = the app's share directory
mkdir -p "$HA/share/glib-2.0/schemas"
glib-compile-schemas --strict --targetdir="$HA/share/glib-2.0/schemas" "$STAGE/share/glib-2.0/schemas"
# the MIME database, as build-app.sh adds
"$REPO_DIR/build/mkmime.sh" "$HA/share/mime"

# The devkit's template program (sdk/template/hello.c) on the fake Wimp
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations -I"$REPO_DIR/sdk/include" \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/template-fake.c" \
  -o "$REPO_DIR/out/template-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
TPL=$REPO_DIR/out/template-linuxarm
rm -rf "$TPL"; mkdir -p "$TPL"

# GtkGLArea (tests/gtk/gltest.c): with riscos-mesa's real RISC OS libEGL
# and libOSMesa linked in (their UnixLib symbols through
# tests/fake/mesa-shim.c, as riscos-mesa's own ARM checks do), and without
# them. Needs riscos-mesa's devkit in MESA_DEVKIT (default
# $RCD_DIR/devkit/env); without it the GL test is skipped.
: "${MESA_DEVKIT:=$RCD_DIR/devkit/env}"
GLLIBS=
if [ -f "$MESA_DEVKIT/lib/libEGL.a" ] && [ -f "$MESA_DEVKIT/lib/libOSMesa.a" ]; then
  ML=$REPO_DIR/out/mesa-linuxarm
  mkdir -p "$ML"
  for l in libEGL libOSMesa; do
    cp "$MESA_DEVKIT/lib/$l.a" "$ML/$l.a"
    # errno is thread-local in glibc; UnixLib's mutexes are laid out differently
    $HOST-objcopy --redefine-sym errno=ro_errno_shim \
      $(for f in mutex_init mutex_lock mutex_unlock mutex_destroy mutexattr_init mutexattr_settype \
                 mutexattr_destroy; do echo "--redefine-sym pthread_$f=ro_pthread_${f}_shim"; done) "$ML/$l.a"
  done
  GLLIBS="$REPO_DIR/tests/fake/mesa-shim.c -Wl,-u,eglGetProcAddress $ML/libEGL.a $ML/libOSMesa.a"
fi
for v in gltest-fake gltest-nogl; do
  [ $v = gltest-fake ] && [ -z "$GLLIBS" ] && continue
  $CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations -DGLTEST_NO_MAIN \
    $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/gltest-fake.c" "$REPO_DIR/tests/gtk/gltest.c" \
    $( [ $v = gltest-fake ] && echo "$GLLIBS") -o "$REPO_DIR/out/$v-linuxarm" \
    -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
    -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
done
GLT=$REPO_DIR/out/gltest-linuxarm
rm -rf "$GLT"; mkdir -p "$GLT"

# A program that ships no GSettings schemas can still make a file chooser
# (gdkschemas-riscos.c)
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/schemas-fake.c" \
  -o "$REPO_DIR/out/schemas-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread

# The Filer-style directory browser (RISCOS_GTK_CHOOSER=filer)
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/browser-fake.c" \
  -o "$REPO_DIR/out/browser-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
BRT=$REPO_DIR/out/browser-linuxarm
rm -rf "$BRT"; mkdir -p "$BRT"

# Menu gives a menu anywhere in a window (gtk/riscos/gtkmenu-riscos.c)
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/menuclick-fake.c" \
  -o "$REPO_DIR/out/menuclick-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread

# Only what GTK repainted is copied and plotted, and the screen stays right
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/damage-fake.c" \
  -o "$REPO_DIR/out/damage-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread

# GLib's static module registry with tools/mkstaticmodules.py: two
# plugins with the same symbol names linked into one program
SMT=$REPO_DIR/out/staticmodule-linuxarm; rm -rf "$SMT"; mkdir -p "$SMT"
for m in moda moda2 modb; do $CC $RO_CFLAGS -c "$REPO_DIR/tests/gtk/staticmodule/$m.c" -o "$SMT/$m.o"; done
python3 "$REPO_DIR/tools/mkstaticmodules.py" --nm $HOST-nm --objcopy $HOST-objcopy --ar $AR --out "$SMT" \
  "plug-a/plugin=$SMT/moda.o,$SMT/moda2.o" "plug-b/plugin=$SMT/modb.o" > "$SMT/archives.txt"
$CC $RO_CFLAGS -no-pie -Wall $(pkg-config --cflags gmodule-2.0) "$REPO_DIR/tests/gtk/staticmodule/staticmodule-test.c" \
  "$SMT/staticmodules.c" $(cat "$SMT/archives.txt") -o "$REPO_DIR/out/staticmodule-test-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gmodule-2.0 | sed 's/-pthread//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -ldl -lm -pthread

# g_spawn_async*() as RISC OS tasks (gdk/riscos/gdkspawn-riscos.c)
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/spawn-fake.c" \
  -o "$REPO_DIR/out/spawn-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread

# Which fontconfig set-up a program gets (gdkfontconfig-riscos.c): with a
# stand-in for PackMan's FontConfig, without, and opted out
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/fontconfig-fake.c" \
  -o "$REPO_DIR/out/fontconfig-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
# The desktop's own fonts (gdkriscosfonts.c, gdkriscosotf.c) with a
# stand-in Font Manager
$CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations \
  $(pkg-config --cflags gtk+-3.0) "$REPO_DIR/tests/gtk/riscosfonts-fake.c" \
  -o "$REPO_DIR/out/riscosfonts-fake-linuxarm" \
  -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
  -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
FCT=$REPO_DIR/out/fontconfig-linuxarm
rm -rf "$FCT"; mkdir -p "$FCT/mono"
unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/DejaVuSerif.ttf' 'dejavu-fonts-ttf-2.37/ttf/DejaVuSansMono.ttf' -d "$FCT/sys"

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
# again in an EX0 EY0 (180 dpi) mode: GDK scale 2 (FAKE_EIG=0 doubles the
# fake screen's pixels; tests work in GDK pixels)
echo; echo "(EX0 EY0, 180 dpi: GDK scale 2)"
FAKE_EIG=0 run hellotest "$HA" "$REPO_DIR/out/hellotest-eig0.ppm"
TT0=$REPO_DIR/out/themetest-eig0; rm -rf "$TT0"; mkdir -p "$TT0"
FAKE_EIG=0 run themetest-fake "$HA" "$TT0"
run edittest-fake "$HA" "$ET"
run template-fake "$HA" "$TPL"
FAKE_EIG=0 run template-fake "$HA" "$TPL"
if [ -n "$GLLIBS" ]; then
  run gltest-fake "$HA" "$GLT"
  FAKE_EIG=0 run gltest-fake "$HA" "$GLT"
else
  echo; echo "=== linux-arm: gltest-fake skipped: no riscos-mesa devkit in MESA_DEVKIT=$MESA_DEVKIT"
fi
GLTEST_NO_MESA=1 run gltest-nogl "$HA" "$GLT"
run schemas-fake "$HA" none
run schemas-fake "$HA" installed
run browser-fake "$HA" "$BRT"
# Geany 2.1 (the demo) on the fake desktop: Save, Open box, Save box, quit;
# again with the directory browser; Build > Compile and Execute. Needs
# Geany (patched, -DGEANY_RISCOS_UI) built as libgeany.a for this target
# in GEANY_LA (see tests/gtk/geany-fake.c)
if [ -n "${GEANY_LA:-}" ] && [ -f "$GEANY_LA/src/.libs/libgeany.a" ]; then
  $CXX $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations $(pkg-config --cflags gtk+-3.0) \
    -x c "$REPO_DIR/tests/gtk/geany-fake.c" -x none "$GEANY_LA/src/.libs/libgeany.a" \
    -o "$REPO_DIR/out/geany-fake-linuxarm" \
    -Wl,-Bstatic $(pkg-config --libs --static gtk+-3.0 gmodule-2.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
    -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
  for chooser in box filer; do
    GYT=$REPO_DIR/out/geany-linuxarm-$chooser; rm -rf "$GYT"; mkdir -p "$GYT/out" "$GYT/config"
    echo "hello from a file" > "$GYT/note.txt"
    FONTCONFIG_FILE=$HA/fonts/fonts.conf XDG_DATA_DIRS=$HA/share XDG_CONFIG_HOME=$GYT/xdg \
      RISCOS_GTK_CHOOSER=$chooser run geany-fake "$GYT/out" -c "$GYT/config" "$GYT/note.txt"
  done
else
  echo; echo "=== linux-arm: geany-fake skipped: no Geany build in GEANY_LA=${GEANY_LA:-}"
fi
# Gnumeric (the port) on the fake desktop, with its plugins built in:
# plugin functions, .xlsx and .ods, a chart plot. Needs build-gnumeric.sh
# run for this target first ($SRC/gnumeric-libs.txt)
if [ -f "$SRC/gnumeric-libs.txt" ]; then
  GN=$SRC/gnumeric-1.12.57; GNT=$REPO_DIR/out/gnumeric-linuxarm
  rm -rf "$GNT"; mkdir -p "$GNT/out"
  cp -r "$HA"/. "$GNT/"; cp -r "$SRC/gnumeric-app/lib" "$SRC/gnumeric-app/goffice" "$GNT/"
  cp -rn "$SRC/gnumeric-app/share"/. "$GNT/share/"
  python3 "$REPO_DIR/build/mkicons.py" "$ADW/Adwaita" "$REPO_DIR/theme/icons-list.txt" "$GNT/share/icons" > /dev/null
  IPF=$(mktemp -d)
  unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/*.ttf' -d "$IPF"
  python3 -B "$REPO_DIR/tools/mkriscosicons.py" --pack gnumeric "$IPF" "$GNT/share/icons/RISCOS" > /dev/null
  rm -rf "$IPF"
  GNC="-I$GN -I$GN/src $(pkg-config --cflags libgoffice-0.10 gtk+-3.0 libgsf-1)"
  $CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations $GNC -Dmain=riscos_gnumeric_main \
    -c "$REPO_DIR/apps/gnumeric/riscos-gnumeric.c" -o "$GNT/riscos-gnumeric.o"
  $CC $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations $GNC "$REPO_DIR/tests/gtk/gnumeric-fake.c" \
    "$GNT/riscos-gnumeric.o" "$SRC/gnumeric-static/staticmodules.c" "$SRC/gnumeric-main.o" $(cat "$SRC/gnumeric-libs.txt") \
    -o "$REPO_DIR/out/gnumeric-fake-linuxarm" \
    -Wl,-Bstatic $(pkg-config --libs --static libgoffice-0.10 gtk+-3.0 libgsf-1 libxml-2.0 libxslt gmodule-2.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
    -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
  run gnumeric-fake "$GNT" "$GNT/out" "$FCT/sys/DejaVuSerif.ttf" "$FCT/sys/DejaVuSansMono.ttf"
else
  echo; echo "=== linux-arm: gnumeric-fake skipped: run build-gnumeric.sh for this target first"
fi
# AbiWord (the port) on the fake desktop: typing, its file formats (the
# built-in plugins), the icon bar. Needs build-abiword.sh run for this
# target first ($SRC/abiword-build.txt)
if [ -f "$SRC/abiword-build.txt" ]; then
  . "$SRC/abiword-build.txt"
  ABT=$REPO_DIR/out/abiword-linuxarm
  rm -rf "$ABT"; mkdir -p "$ABT/out"
  cp -r "$HA"/. "$ABT/"; cp -rn "$SRC/abiword-app/share"/. "$ABT/share/"
  python3 "$REPO_DIR/build/mkicons.py" "$ADW/Adwaita" "$REPO_DIR/theme/icons-list.txt" "$ABT/share/icons" > /dev/null
  IPF=$(mktemp -d)
  unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/*.ttf' -d "$IPF"
  python3 -B "$REPO_DIR/tools/mkriscosicons.py" --pack abiword "$IPF" "$ABT/share/icons/RISCOS" > /dev/null
  rm -rf "$IPF"
  $CXX -std=c++11 $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations -Wno-overloaded-virtual -DABI_RISCOS -DABI_RISCOS_TEST $AB_CFLAGS \
    -c "$REPO_DIR/apps/abiword/riscos-abiword.cpp" -o "$ABT/riscos-abiword.o"
  $CXX -std=c++11 $RO_CFLAGS -no-pie -Wall -Wno-deprecated-declarations -Wno-overloaded-virtual -DABI_RISCOS $AB_CFLAGS \
    -c "$REPO_DIR/tests/gtk/abiword-fake.cpp" -o "$ABT/abiword-fake.o"
  $CXX -no-pie "$ABT/abiword-fake.o" "$ABT/riscos-abiword.o" $AB_LIBS -o "$REPO_DIR/out/abiword-fake-linuxarm" \
    -Wl,-Bstatic $(pkg-config --libs --static libgoffice-0.10 gtk+-3.0 libgsf-1 wv-1.0 libxslt fribidi librsvg-2.0 enchant-2 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') -ljpeg \
    -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
  run abiword-fake "$ABT" "$ABT/out"
else
  echo; echo "=== linux-arm: abiword-fake skipped: run build-abiword.sh for this target first"
fi
run menuclick-fake "$HA"
run damage-fake "$HA"
run damage-fake "$HA" trgb
FAKE_EIG=0 run damage-fake "$HA"
run spawn-fake "$HA"
run staticmodule-test
# mtPaint (the port) on the fake desktop: Menu everywhere, its RISC OS save
# and open dialogue. Needs mtPaint's objects built for this target with
# -DMT_RISCOS_UI in MTPAINT_LA (see tests/gtk/mtpaint-fake.c)
if [ -n "${MTPAINT_LA:-}" ] && [ -f "$MTPAINT_LA/src/main.o" ]; then
  MTO=$(mktemp -d)
  arm-linux-gnueabihf-objcopy --redefine-sym main=mtpaint_main "$MTPAINT_LA/src/main.o" "$MTO/main.o"
  $CC $RO_CFLAGS -no-pie -Wall $(pkg-config --cflags gtk+-3.0) -c "$REPO_DIR/tests/gtk/mtpaint-fake.c" -o "$MTO/test.o"
  $CC -no-pie "$MTO/test.o" "$MTO/main.o" $(ls "$MTPAINT_LA"/src/*.o | grep -v '/main\.o$') \
    -o "$REPO_DIR/out/mtpaint-fake-linuxarm" \
    -Wl,-Bstatic -ljpeg -lfreetype $(pkg-config --libs --static gtk+-3.0 | sed 's/-pthread//g; s/-lresolv//g; s/-ldl//g; s/-lm / /g') \
    -Wl,-Bdynamic -lstdc++ -lresolv -ldl -lm -pthread
  MTT=$REPO_DIR/out/mtpaint-linuxarm; rm -rf "$MTT"; mkdir -p "$MTT/home"
  python3 "$REPO_DIR/tests/gtk/mkpng.py" "$MTT/drop.png"
  FONTCONFIG_FILE=$HA/fonts/fonts.conf XDG_DATA_DIRS=$HA/share HOME=$MTT/home XDG_CONFIG_HOME=$MTT/home/.config \
    run mtpaint-fake "$MTT"
  rm -rf "$MTO"
else
  echo; echo "=== linux-arm: mtpaint-fake skipped: no mtPaint build in MTPAINT_LA=${MTPAINT_LA:-}"
fi
for m in unixfc own forced bare; do
  rm -rf "$FCT/$m"; mkdir -p "$FCT/$m"
  # the system's colour emoji font (as PackMan's TTF-Noto-Color-Emoji puts
  # it in UnixFont: build/build-emoji.sh) and fontconfig's scaling rules
  EMO="$DL/NotoColorEmoji-2.051.ttf $SRC/fontconfig-2.14.1/conf.d/10-scale-bitmap-fonts.conf"
  run fontconfig-fake "$HA" "$FCT/$m" $m "$FCT/sys/DejaVuSerif.ttf" "$FCT/sys/DejaVuSansMono.ttf" $EMO
done
RFT=$REPO_DIR/out/riscosfonts-linuxarm
rm -rf "$RFT"; mkdir -p "$RFT"
run riscosfonts-fake "$HA" "$RFT"
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
