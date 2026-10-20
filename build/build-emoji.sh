#!/bin/bash -e
# The optional colour emoji font: out/TTF-Noto-Color-Emoji-<version>.zip, a
# PackMan (RiscPkg) package that puts Noto Color Emoji 2.051 into PackMan's
# UnixFont, where every fontconfig program on the machine finds it through
# FontConfig (UnixFC): GTK programs, Folio, Warzone 2100...
#
# It is laid out as PackMan's own font packages are (TTF-DejaVu-Core):
#   Apps/Fonts/!UnixFont/truetype/noto-color-emoji/NotoColorEmoji.ttf
#   Apps/Fonts/!UnixFont/doc/noto-color-emoji/  the licence and a ReadMe
#   RiscPkg/Control, Copyright, Triggers/PostInstall and PostRemove (they
#   run fc-cache, which the FontConfig package provides)
# and depends on UnixFont and FontConfig. Without PackMan, copying the
# Apps.Fonts.!UnixFont.truetype.noto-color-emoji folder into !UnixFont's
# truetype folder (FontConfig installed) does the same.
#
# The programs themselves don't carry an emoji font (10 MB each): GDK's
# fontconfig rules (gdk/riscos/gdkfontconfig-riscos.c) put this font in
# the "emoji" family and scale it to the text's size; without it the Log's
# fonts line says to install this package.
#   build/build-emoji.sh            (VERSION=2.051.0.1-1 by default)
REPO_DIR=$(cd "$(dirname "$0")/.." && pwd)
: "${DL:=$REPO_DIR/dl}"
: "${RCD_DIR:=$REPO_DIR/../riscos-crossdev}"
FONT_VERSION=2.051
# PackMan version: <upstream>.0.<build>-<package revision> (as riscos-packages)
VERSION=${VERSION:-$FONT_VERSION.0.1-1}
NAME=TTF-Noto-Color-Emoji
OUT=$REPO_DIR/out
P=$OUT/emoji-package
F=$P/Apps/Fonts/!UnixFont

for f in "NotoColorEmoji-$FONT_VERSION.ttf" "NotoColorEmoji-$FONT_VERSION-OFL.txt"; do
  [ -s "$DL/$f" ] || { echo "build-emoji.sh: $DL/$f is missing (build/fetch-sources.sh)" >&2; exit 1; }
done
(cd "$DL" && grep -E " NotoColorEmoji-$FONT_VERSION(\.ttf|-OFL\.txt)\$" "$REPO_DIR/build/SHA256SUMS.txt" | sha256sum -c --quiet)

rm -rf "$P"
mkdir -p "$F/truetype/noto-color-emoji" "$F/doc/noto-color-emoji" "$P/RiscPkg/Triggers"
cp "$DL/NotoColorEmoji-$FONT_VERSION.ttf" "$F/truetype/noto-color-emoji/NotoColorEmoji.ttf,fff"
cp "$DL/NotoColorEmoji-$FONT_VERSION-OFL.txt" "$F/doc/noto-color-emoji/LICENSE,fff"

cat > "$F/doc/noto-color-emoji/README,fff" <<EOF
Noto Color Emoji $FONT_VERSION
=====================

Google's colour emoji font (https://github.com/googlefonts/noto-emoji,
the font file from its v$FONT_VERSION tag): the emoji of recent Unicode
versions in colour, faces, people with their skin tones, families, flags.

It is a font for fontconfig programs: GTK programs (ClawsMail, Gnumeric,
Geany, L3afpad, mtPaint...) show emoji with it, in colour at the size of
the text around them. It is installed in UnixFont, where PackMan's
FontConfig (UnixFC) finds it, so it needs FontConfig; the riscos-gtk
programs use FontConfig's fonts when it is installed and booted.

It isn't a RISC OS outline font: !Edit, !Draw and other programs that
use the RISC OS Font Manager can't use it.

Licence: SIL Open Font License 1.1 (LICENSE, in this folder).
Packaged by riscos-gtk (https://github.com/adyoull/riscos-gtk).
EOF

cat > "$P/RiscPkg/Control,fff" <<EOF
Package: $NAME
Version: $VERSION
Section: Fonts
Priority: Optional
Licence: Free
Maintainer: Andrew Youll <adyoull@users.noreply.github.com>
Standards-Version: 0.4.0
Environment: any
Depends: UnixFont, FontConfig
Homepage: https://github.com/googlefonts/noto-emoji
Description: Noto Color Emoji font for fontconfig programs
 Google's colour emoji font, with the emoji of recent Unicode versions:
 faces, people with their skin tones, families, flags. GTK programs
 (ClawsMail, Gnumeric, Geany, L3afpad, mtPaint) and other fontconfig
 programs show emoji with it, in colour, at the size of the text.
 .
 It is installed in UnixFont and found through FontConfig. It is not a
 RISC OS outline font, so Font Manager programs can't use it.
EOF

{
  echo "TTF-Noto-Color-Emoji"
  echo
  echo "Noto Color Emoji $FONT_VERSION, the font file from"
  echo "https://github.com/googlefonts/noto-emoji (tag v$FONT_VERSION),"
  echo "packaged for PackMan by riscos-gtk (https://github.com/adyoull/riscos-gtk)."
  echo
  echo "Copyright 2022 Google Inc. Licensed under the SIL Open Font License,"
  echo "Version 1.1, which follows."
  echo
  cat "$DL/NotoColorEmoji-$FONT_VERSION-OFL.txt"
} > "$P/RiscPkg/Copyright,fff"

for t in PostInstall PostRemove; do
  cat > "$P/RiscPkg/Triggers/$t,feb" <<'EOF'
| Bring FontConfig's font cache up to date (once, whatever else is
| installed or removed at the same time)
Set PkgTrigger$ReturnCode 0
If "<PkgTrigger$Abort>" = "1" Then Obey
If "<PkgTrigger$S_FcCache_Run>" = "yes" Then Obey
Set PkgTrigger$S_FcCache_Run yes
Echo Updating the Unix font cache
fc-cache
EOF
done

rm -f "$OUT/$NAME-$VERSION.zip"
python3 "$RCD_DIR/package/rozip.py" "$OUT/$NAME-$VERSION.zip" -C "$P" Apps RiscPkg
sha256sum "$OUT/$NAME-$VERSION.zip"
