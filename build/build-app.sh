#!/bin/bash -e
# Build a GTK test program for RISC OS against $STAGE (GTK with the RISC OS
# backend), run riscos-crossdev's checks, and package out/NAME-VERSION.zip:
# the program, DejaVu Sans with a fonts.conf, the RISCOS theme and
# PThreadTicker.
#   build/build-app.sh NAME PROGRAM VERSION APPFILES SOURCE...
# NAME (e.g. GTKHello) is the application (!NAME, NAME$Dir); APPFILES is a
# folder with !Run,feb (WimpSlot as @SLOT@) and !Help,fff.
. "$(dirname "$0")/env.sh"
[ "$TARGET" = riscos ] || { echo "build-app.sh: RISC OS only" >&2; exit 1; }
NAME=$1 PROG=$2 VERSION=$3 FILES=$4; shift 4
OUT=$REPO_DIR/out
A=$OUT/!$NAME
mkdir -p "$OUT"
rm -rf "$A"; mkdir -p "$A/fonts" "$A/share/themes/RISCOS"

$CC $RO_CFLAGS -Wall -Wno-deprecated-declarations $(pkg-config --cflags gtk+-3.0) \
  "$@" -o "$OUT/$PROG" -static $(pkg-config --libs --static gtk+-3.0)
"$RCD_DIR/checks/check-all.sh" "$OUT/$PROG"
"$RCD_DIR/checks/check-unaligned.sh" $(pkg-config --cflags gtk+-3.0) "$@"
$STRIP -o "$OUT/$PROG.stripped" "$OUT/$PROG"
"$GCCSDK_ENV/bin/elf2aif" -e "$OUT/$PROG.stripped" "$A/$PROG,ff8" > /dev/null
if strings "$A/$PROG,ff8" | grep -q -e "$REPO_DIR" -e "$(cd "$RCD_DIR" && pwd -P)" -e "$SYSROOT" -i -e claude; then
  echo "build-app.sh: build paths found in $PROG,ff8" >&2; exit 1
fi

# WimpSlot: the program plus 4 MB
size=$(stat -c %s "$A/$PROG,ff8")
slot=$(( (size + 4 * 1024 * 1024 + 1023) / 1024 ))
sed "s/@SLOT@/$slot/g" "$FILES/!Run,feb" > "$A/!Run,feb"
sed "s/@VERSION@/$VERSION/g" "$FILES/!Help,fff" > "$A/!Help,fff"
# anything else the application has (!Boot, !Sprites, ...)
for f in "$FILES"/*; do
  case "$(basename "$f")" in '!Run,feb'|'!Help,fff') ;; *) cp -r "$f" "$A/" ;; esac
done
cp "$GCCSDK_ENV"/riscos/PThrTicker,ffa "$GCCSDK_ENV"/riscos/PThreadTicker-ReadMe,fff \
   "$GCCSDK_ENV"/riscos/PThreadTicker-Licence,fff "$A/"
unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf' \
  'dejavu-fonts-ttf-2.37/ttf/DejaVuSans-Bold.ttf' 'dejavu-fonts-ttf-2.37/ttf/DejaVuSans-Oblique.ttf' \
  'dejavu-fonts-ttf-2.37/LICENSE' -d "$A/fonts"
# more DejaVu faces an application wants (e.g. EXTRA_FONTS="DejaVuSansMono.ttf")
for f in ${EXTRA_FONTS:-}; do
  unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" "dejavu-fonts-ttf-2.37/ttf/$f" -d "$A/fonts"
done
mv "$A/fonts/LICENSE" "$A/fonts/Licence,fff"
"$REPO_DIR/tests/render/mkfontsconf.sh" "/<$NAME\$Dir>/fonts" "/<Wimp\$ScrapDir>/$NAME/fontcache" \
  > "$A/fonts/fonts.conf"
python3 -c 'import sys, xml.dom.minidom as m; m.parse(sys.argv[1])' "$A/fonts/fonts.conf"
cp -r "$REPO_DIR/theme/gtk-3.0" "$A/share/themes/RISCOS/"
# icons: the RISCOS theme (Adwaita's, for the names GTK and programs use;
# needs the host's gtk-encode-symbolic-svg, libgtk-3-bin) and an empty
# hicolor; the desktop's sprites are added at run time
ADW=$SRC/adwaita-icon-theme-3.36.1
[ -d "$ADW" ] || tar -xf "$DL/adwaita-icon-theme_3.36.1.orig.tar.xz" -C "$SRC"
python3 "$REPO_DIR/build/mkicons.py" "$ADW/Adwaita" "$REPO_DIR/theme/icons-list.txt" "$A/share/icons"
# GTK's GSettings schemas (the file choosers, colour chooser, emoji
# chooser need them: GLib stops the program without), found through
# XDG_DATA_DIRS = the app's share directory
mkdir -p "$A/share/glib-2.0/schemas"
glib-compile-schemas --strict --targetdir="$A/share/glib-2.0/schemas" "$STAGE/share/glib-2.0/schemas"
rm -f "$OUT/$NAME-$VERSION.zip"
python3 "$RCD_DIR/package/rozip.py" "$OUT/$NAME-$VERSION.zip" -C "$OUT" "!$NAME"
ls -la "$A/$PROG,ff8"
sha256sum "$OUT/$NAME-$VERSION.zip"
