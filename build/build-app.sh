#!/bin/bash -e
# Build a GTK test program for RISC OS against $STAGE (GTK with the RISC OS
# backend), run riscos-crossdev's checks, and package out/NAME-VERSION.zip:
# the program, DejaVu Sans with a fonts.conf, the RISCOS theme and
# PThreadTicker.
#   build/build-app.sh NAME PROGRAM VERSION APPFILES SOURCE...
#   PREBUILT=program build/build-app.sh NAME PROGRAM VERSION APPFILES
# NAME (e.g. GTKHello) is the application (!NAME, NAME$Dir); APPFILES is a
# folder with !Run,feb (WimpSlot as @SLOT@) and !Help,fff.
. "$(dirname "$0")/env.sh"
[ "$TARGET" = riscos ] || { echo "build-app.sh: RISC OS only" >&2; exit 1; }
NAME=$1 PROG=$2 VERSION=$3 FILES=$4; shift 4
OUT=$REPO_DIR/out
# RISCOS_GTK_MIXED=1: linked mixed (build/mixed-cc), the program kept as
# ELF (&E1F, run by !SharedLibs) and !Run made to use !GTKLibs
# (tools/mixedrun.py); packaged in out/mixed
MIXED=; [ "${RISCOS_GTK_MIXED:-}" = 1 ] && { MIXED=1; OUT=$REPO_DIR/out/mixed; }
EXE=$PROG,ff8; [ -n "$MIXED" ] && EXE=$PROG,e1f
A=$OUT/!$NAME
mkdir -p "$OUT"
rm -rf "$A"; mkdir -p "$A/fonts" "$A/share/themes/RISCOS"

# EXTRA_CFLAGS, EXTRA_LIBS: more for the compile and link lines (e.g. GL:
# riscos-mesa's devkit, build-gltest.sh)
# PREBUILT: a program already linked (e.g. Geany, by its own build:
# build-geany.sh), packaged as it is
if [ -n "${PREBUILT:-}" ]; then
  cp "$PREBUILT" "$OUT/$PROG"
  "$RCD_DIR/checks/check-all.sh" "$OUT/$PROG"
else
  $CC $RO_CFLAGS -Wall -Wno-deprecated-declarations $(pkg-config --cflags gtk+-3.0) ${EXTRA_CFLAGS:-} \
    "$@" -o "$OUT/$PROG" -static ${EXTRA_LIBS:-} $(pkg-config --libs --static gtk+-3.0)
  "$RCD_DIR/checks/check-all.sh" "$OUT/$PROG"
  "$RCD_DIR/checks/check-unaligned.sh" $(pkg-config --cflags gtk+-3.0) "$@"
fi
$STRIP -o "$OUT/$PROG.stripped" "$OUT/$PROG"
if [ -n "$MIXED" ]; then
  cp "$OUT/$PROG.stripped" "$A/$EXE"
else
  "$GCCSDK_ENV/bin/elf2aif" -e "$OUT/$PROG.stripped" "$A/$EXE" > /dev/null
fi
if strings "$A/$EXE" | grep -q -e "$REPO_DIR" -e "$(cd "$RCD_DIR" && pwd -P)" -e "$SYSROOT" -i -e claude; then
  echo "build-app.sh: build paths found in $EXE" >&2; exit 1
fi

# WimpSlot: the program plus 4 MB
size=$(stat -c %s "$A/$EXE")
slot=$(( (size + 4 * 1024 * 1024 + 1023) / 1024 ))
sed "s/@SLOT@/$slot/g" "$FILES/!Run,feb" > "$A/!Run,feb"
# The hourglass from before the program loads (a big file, off an SD
# card): GDK takes it away once the program has started
# (gdkriscos-server.c). *X: no error where *HOn isn't known.
grep -q "^Run <$NAME\\\$Dir>\." "$A/!Run,feb" || { echo "build-app.sh: no 'Run <$NAME\$Dir>.' in !Run" >&2; exit 1; }
# (above the comment lines that go with the Run line)
awk -v pat="^Run <$NAME[$]Dir>[.]" '
  { line[NR] = $0 } $0 ~ pat && !at { at = NR }
  END { start = at; while (start > 1 && line[start - 1] ~ /^\|/) start--
        for (i = 1; i <= NR; i++) {
          if (i == start) { print "| The hourglass while the program loads (riscos-gtk takes it away once"
                            print "| the program has started)"; print "X HOn" }
          print line[i] } }' \
  "$A/!Run,feb" > "$A/!Run,feb.new" && mv "$A/!Run,feb.new" "$A/!Run,feb"
[ -n "$MIXED" ] && python3 -B "$REPO_DIR/tools/mixedrun.py" "$A/!Run,feb" "$NAME" "$PROG" "$slot"
sed "s/@VERSION@/$VERSION/g" "$FILES/!Help,fff" > "$A/!Help,fff"
# anything else the application has (!Boot, !Sprites, ...)
for f in "$FILES"/*; do
  case "$(basename "$f")" in '!Run,feb'|'!Help,fff'|FileTypes) ;; *) cp -r "$f" "$A/" ;; esac
done
# The file types the program opens (FILES/FileTypes: TYPE NAME ICON per
# line, ICON KIND:LABEL:COLOUR for tools/mkfiletypesprites.py or - where
# the desktop's themes have one): !Boot and !Run give each its name, its
# icon and its double-click action, each only where nothing on the
# desktop has set it already. !Boot runs when the Filer first sees the
# application (PackMan's LookAt runs it at every start-up).
if [ -f "$FILES/FileTypes" ]; then
  ft=$(mktemp) ftd=$(mktemp -d)
  {
    echo "| The file types !$NAME opens: their names, icons and double-click"
    echo "| action, where nothing on the desktop has set them already"
  } > "$ft"
  specs=()
  while read -r typ tname icon; do
    case "$typ" in ''|'#'*) continue ;; esac
    typ=$(echo "$typ" | tr a-f A-F)
    if [ "$icon" != - ]; then
      specs+=("$typ:$icon")
      echo "If \"<File\$Type_$typ>\"=\"\" Then IconSprites <$NAME\$Dir>.Types.$typ" >> "$ft"
    fi
    echo "If \"<File\$Type_$typ>\"=\"\" Then Set File\$Type_$typ $tname" >> "$ft"
    echo "If \"<Alias\$@RunType_$typ>\"=\"\" Then Set Alias\$@RunType_$typ Run <$NAME\$Dir>.!Run %%*0" >> "$ft"
  done < "$FILES/FileTypes"
  if [ ${#specs[@]} -gt 0 ]; then
    unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/DejaVuSansCondensed-Bold.ttf' -d "$ftd"
    python3 -B "$REPO_DIR/tools/mkfiletypesprites.py" "$ftd" "$ftd" "${specs[@]}"
    mkdir -p "$A/Types"
    for s in "${specs[@]}"; do
      typ=${s%%:*}; t=$(echo "$typ" | tr A-F a-f)
      python3 "$REPO_DIR/tools/png2sprite.py" "$A/Types/$typ,ff9" \
        "file_$t=$ftd/$t/file.png" "small_$t=$ftd/$t/small.png"
      python3 "$REPO_DIR/tools/png2sprite.py" --dpi 180 "$A/Types/${typ}11,ff9" \
        "file_$t=$ftd/$t/file11.png" "small_$t=$ftd/$t/small11.png"
    done
  fi
  # after the line that sets NAME$Dir, in both
  for f in '!Boot,feb' '!Run,feb'; do
    [ -f "$A/$f" ] || continue
    grep -q "^Set $NAME\\\$Dir " "$A/$f" || { echo "build-app.sh: no 'Set $NAME\$Dir' in $f" >&2; exit 1; }
    awk -v ins="$ft" -v pat="^Set $NAME[$]Dir " '
      { print } $0 ~ pat && !done { while ((getline l < ins) > 0) print l; close(ins); done = 1 }' \
      "$A/$f" > "$A/$f.new" && mv "$A/$f.new" "$A/$f"
  done
  rm -rf "$ft" "$ftd"
fi
# Every package: the LGPL (GTK, GLib and the libraries are linked into
# it) and where its complete source is (docs/SOURCE-AND-LICENCES.md)
cp "$REPO_DIR/riscos/licences/LGPL-2.1.txt" "$A/LicenceLGPL,fff"
COMMIT=$(git -C "$REPO_DIR" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)
[ -z "$(git -C "$REPO_DIR" status --porcelain --untracked-files=no 2>/dev/null)" ] || COMMIT="$COMMIT (with changes)"
sed -e "s|@NAME@|$NAME|g" -e "s|@VERSION@|$VERSION|g" -e "s|@COMMIT@|$COMMIT|g" \
    -e "s|@RELEASE@|${RELEASE:-a development build}|g" \
    -e "s|@SOURCE_URL@|${SOURCE_URL:-https://github.com/adyoull/riscos-gtk}|g" \
    "$REPO_DIR/riscos/licences/Source.txt" > "$A/Source,fff"
cp "$GCCSDK_ENV"/riscos/PThrTicker,ffa "$GCCSDK_ENV"/riscos/PThreadTicker-ReadMe,fff \
   "$GCCSDK_ENV"/riscos/PThreadTicker-Licence,fff "$A/"
# DejaVu Sans (DEJAVU_FACES, env.sh); the rest come from UnixFC
unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" $(for f in $DEJAVU_FACES; do echo "dejavu-fonts-ttf-2.37/ttf/$f"; done) \
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
# icons: the RISCOS theme (riscos-gtk's own, then Adwaita's, for the names
# GTK and programs use: build/mkicons.py) and an empty hicolor; the
# desktop's sprites are added at run time
ADW=$SRC/adwaita-icon-theme-3.36.1
[ -d "$ADW" ] || tar -xf "$DL/adwaita-icon-theme_3.36.1.orig.tar.xz" -C "$SRC"
python3 "$REPO_DIR/build/mkicons.py" "$ADW/Adwaita" "$REPO_DIR/theme/icons-list.txt" "$A/share/icons"
# the program's own icons in the RISC OS style (ICON_PACK=gnumeric, geany:
# tools/riscosicons_apps.py), ahead of its upstream ones
if [ -n "${ICON_PACK:-}" ]; then
  IPF=$(mktemp -d)
  unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/*.ttf' -d "$IPF"
  python3 -B "$REPO_DIR/tools/mkriscosicons.py" --pack "$ICON_PACK" "$IPF" "$A/share/icons/RISCOS"
  rm -rf "$IPF"
fi
# GTK's GSettings schemas (the file choosers, colour chooser, emoji
# chooser need them: GLib stops the program without), found through
# XDG_DATA_DIRS = the app's share directory
mkdir -p "$A/share/glib-2.0/schemas"
glib-compile-schemas --strict --targetdir="$A/share/glib-2.0/schemas" "$STAGE/share/glib-2.0/schemas"
# the MIME database: file types from names (build/mkmime.sh)
"$REPO_DIR/build/mkmime.sh" "$A/share/mime"
rm -f "$OUT/$NAME-$VERSION.zip"
python3 "$RCD_DIR/package/rozip.py" "$OUT/$NAME-$VERSION.zip" -C "$OUT" "!$NAME"
ls -la "$A/$EXE"
sha256sum "$OUT/$NAME-$VERSION.zip"
