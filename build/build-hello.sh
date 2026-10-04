#!/bin/bash -e
# Build tests/gtk/hello.c for RISC OS against $STAGE (GTK with the RISC OS
# backend), run riscos-crossdev's checks, and package
# out/GTKHello-$VERSION.zip: the program, DejaVu Sans with a fonts.conf,
# the RISCOS theme, PThreadTicker.
. "$(dirname "$0")/env.sh"
[ "$TARGET" = riscos ] || { echo "build-hello.sh: RISC OS only" >&2; exit 1; }
VERSION=${VERSION:-0.4}
OUT=$REPO_DIR/out
A=$OUT/!GTKHello
T=$REPO_DIR/tests/gtk
mkdir -p "$OUT"
rm -rf "$A"; mkdir -p "$A/fonts" "$A/share/themes/RISCOS"

$CC $RO_CFLAGS -Wall -Wno-deprecated-declarations $(pkg-config --cflags gtk+-3.0) \
  "$T/hello.c" -o "$OUT/hello" -static $(pkg-config --libs --static gtk+-3.0)
"$RCD_DIR/checks/check-all.sh" "$OUT/hello"
"$RCD_DIR/checks/check-unaligned.sh" $(pkg-config --cflags gtk+-3.0) "$T/hello.c"
$STRIP -o "$OUT/hello.stripped" "$OUT/hello"
"$GCCSDK_ENV/bin/elf2aif" -e "$OUT/hello.stripped" "$A/hello,ff8" > /dev/null
if strings "$A/hello,ff8" | grep -q -e "$REPO_DIR" -e "$(cd "$RCD_DIR" && pwd -P)" -e "$SYSROOT" -i -e claude; then
  echo "build-hello.sh: build paths found in hello,ff8" >&2; exit 1
fi

# WimpSlot: the program plus 4 MB
size=$(stat -c %s "$A/hello,ff8")
slot=$(( (size + 4 * 1024 * 1024 + 1023) / 1024 ))
sed "s/@SLOT@/$slot/g" "$T/app/!Run,feb" > "$A/!Run,feb"
cp "$T/app/!Help,fff" "$A/"
cp "$GCCSDK_ENV"/riscos/PThrTicker,ffa "$GCCSDK_ENV"/riscos/PThreadTicker-ReadMe,fff \
   "$GCCSDK_ENV"/riscos/PThreadTicker-Licence,fff "$A/"
unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf' \
  'dejavu-fonts-ttf-2.37/ttf/DejaVuSans-Bold.ttf' 'dejavu-fonts-ttf-2.37/LICENSE' -d "$A/fonts"
mv "$A/fonts/LICENSE" "$A/fonts/Licence,fff"
"$REPO_DIR/tests/render/mkfontsconf.sh" '/<GTKHello$Dir>/fonts' '/<Wimp$ScrapDir>/GTKHello/fontcache' \
  > "$A/fonts/fonts.conf"
python3 -c 'import sys, xml.dom.minidom as m; m.parse(sys.argv[1])' "$A/fonts/fonts.conf"
cp -r "$REPO_DIR/theme/gtk-3.0" "$A/share/themes/RISCOS/"
rm -f "$OUT/GTKHello-$VERSION.zip"
python3 "$RCD_DIR/package/rozip.py" "$OUT/GTKHello-$VERSION.zip" -C "$OUT" '!GTKHello'
ls -la "$A/hello,ff8"
sha256sum "$OUT/GTKHello-$VERSION.zip"
