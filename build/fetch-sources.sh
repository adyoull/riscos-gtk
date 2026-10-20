#!/bin/bash -e
# Download the sources listed in build/SHA256SUMS.txt into $DL and check them.
# Adapted from riscos-warzone2100 build/fetch-sources.sh (GPL version 2 or later; see LICENCES.md).
# Upstream tarballs are Ubuntu's orig tarballs from archive.ubuntu.com (the
# cloud build machine can't reach download.gnome.org or GitHub web);
# fontconfig 2.14.1 (PackMan's version) is Debian bookworm's. DejaVu comes
# from GitHub releases: if this machine can't reach GitHub, copy it from
# riscos-warzone2100's dl-cache into $DL by hand (same file, same sha256);
# the same goes for fontconfig if deb.debian.org can't be reached.
# The libpng/freetype/expat/fontconfig/DejaVu set and its checksums are the
# ones riscos-warzone2100 uses.
# Only needs DL, so not build/env.sh (which also wants a toolchain).
REPO_DIR=$(cd "$(dirname "$0")/.." && pwd)
: "${DL:=$REPO_DIR/dl}"
mkdir -p "$DL"
cd "$DL"
U=https://archive.ubuntu.com/ubuntu/pool/main
get() {  # file url
  [ -s "$1" ] && return 0
  echo "fetch $1"; curl -fsSL -o "$1.part" "$2" && mv "$1.part" "$1" || { rm -f "$1.part"; echo "  FAILED: $2" >&2; }
}
get zlib_1.3.dfsg+really1.3.1.orig.tar.gz $U/z/zlib/zlib_1.3.dfsg+really1.3.1.orig.tar.gz
# libffi's own release tarball (it has its configure script; Ubuntu's orig
# is a git export that needs autoconf 2.71, which riscos-crossdev's image
# hasn't: it has 2.69, for GCCSDK)
get libffi-3.4.6.tar.gz            https://github.com/libffi/libffi/releases/download/v3.4.6/libffi-3.4.6.tar.gz
get pcre2_10.42.orig.tar.gz         $U/p/pcre2/pcre2_10.42.orig.tar.gz
get glib2.0_2.80.0.orig.tar.xz      $U/g/glib2.0/glib2.0_2.80.0.orig.tar.xz
get libpng1.6_1.6.37.orig.tar.gz    $U/libp/libpng1.6/libpng1.6_1.6.37.orig.tar.gz
get freetype_2.10.1.orig.tar.gz     $U/f/freetype/freetype_2.10.1.orig.tar.gz
get expat_2.2.9.orig.tar.gz         $U/e/expat/expat_2.2.9.orig.tar.gz
get fontconfig_2.14.1.orig.tar.xz   https://deb.debian.org/debian/pool/main/f/fontconfig/fontconfig_2.14.1.orig.tar.xz
get libjpeg-turbo_2.1.5.orig.tar.gz  $U/libj/libjpeg-turbo/libjpeg-turbo_2.1.5.orig.tar.gz
get fribidi_1.0.13.orig.tar.xz      $U/f/fribidi/fribidi_1.0.13.orig.tar.xz
get harfbuzz_8.3.0.orig.tar.xz      $U/h/harfbuzz/harfbuzz_8.3.0.orig.tar.xz
get pixman_0.42.2.orig.tar.gz       $U/p/pixman/pixman_0.42.2.orig.tar.gz
get cairo_1.18.0.orig.tar.xz        $U/c/cairo/cairo_1.18.0.orig.tar.xz
get pango1.0_1.52.1+ds.orig.tar.xz  $U/p/pango1.0/pango1.0_1.52.1+ds.orig.tar.xz
get gdk-pixbuf_2.42.10+dfsg.orig.tar.xz $U/g/gdk-pixbuf/gdk-pixbuf_2.42.10+dfsg.orig.tar.xz
get atk1.0_2.36.0.orig.tar.xz       $U/a/atk1.0/atk1.0_2.36.0.orig.tar.xz
get libepoxy_1.5.10.orig.tar.gz     $U/libe/libepoxy/libepoxy_1.5.10.orig.tar.gz
# GTK itself (M2)
get gtk+3.0_3.24.41.orig.tar.xz     $U/g/gtk+3.0/gtk+3.0_3.24.41.orig.tar.xz
# the RISCOS icon theme is taken from Adwaita's PNGs (build/mkicons.py)
get adwaita-icon-theme_3.36.1.orig.tar.xz $U/a/adwaita-icon-theme/adwaita-icon-theme_3.36.1.orig.tar.xz
# L3afpad, the first GTK program ported (build/build-l3afpad.sh; universe)
get l3afpad_0.8.18.1.11.orig.tar.gz https://archive.ubuntu.com/ubuntu/pool/universe/l/l3afpad/l3afpad_0.8.18.1.11.orig.tar.gz
# Claws Mail, the second (build/build-claws.sh; universe), and Nettle for
# its stored-password DES
get claws-mail_4.4.0.orig.tar.xz    https://archive.ubuntu.com/ubuntu/pool/universe/c/claws-mail/claws-mail_4.4.0.orig.tar.xz
# Geany, the demo programmer's editor (build/build-geany.sh; universe)
get geany_2.1.orig.tar.gz          https://archive.ubuntu.com/ubuntu/pool/universe/g/geany/geany_2.1.orig.tar.gz
# mtPaint, the painting program (build/build-mtpaint.sh; universe)
get mtpaint_3.50.14.orig.tar.gz    https://archive.ubuntu.com/ubuntu/pool/universe/m/mtpaint/mtpaint_3.50.14.orig.tar.gz
get nettle_3.10.2.orig.tar.gz       $U/n/nettle/nettle_3.10.2.orig.tar.gz
# libetpan, for Claws Mail's IMAP
get libetpan_1.9.4.orig.tar.xz      https://archive.ubuntu.com/ubuntu/pool/universe/libe/libetpan/libetpan_1.9.4.orig.tar.xz
# gumbo, the HTML parser of the litehtml viewer (Claws Mail's HTML mail)
get gumbo-parser_0.12.0+dfsg.orig.tar.xz https://archive.ubuntu.com/ubuntu/pool/universe/g/gumbo-parser/gumbo-parser_0.12.0+dfsg.orig.tar.xz
# Gnumeric, the spreadsheet (build/build-gnumeric.sh; GOffice and Gnumeric in
# universe), and what it needs:
# libxml2, libxslt (GOffice's MathML), libgsf (structured files), GOffice
get libxml2_2.9.14+dfsg.orig.tar.xz $U/libx/libxml2/libxml2_2.9.14+dfsg.orig.tar.xz
get libxslt_1.1.39.orig.tar.xz      $U/libx/libxslt/libxslt_1.1.39.orig.tar.xz
get libgsf_1.14.51.orig.tar.xz      $U/libg/libgsf/libgsf_1.14.51.orig.tar.xz
get goffice_0.10.57.orig.tar.xz     https://archive.ubuntu.com/ubuntu/pool/universe/g/goffice/goffice_0.10.57.orig.tar.xz
get gnumeric_1.12.57.orig.tar.xz    https://archive.ubuntu.com/ubuntu/pool/universe/g/gnumeric/gnumeric_1.12.57.orig.tar.xz
# the MIME database every application carries (build/mkmime.sh)
get shared-mime-info_2.4.orig.tar.bz2 $U/s/shared-mime-info/shared-mime-info_2.4.orig.tar.bz2
get dejavu-fonts-ttf-2.37.zip       https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-fonts-ttf-2.37.zip
# Noto Color Emoji 2.051 (SIL Open Font Licence 1.1), the colour emoji font
# every program carries: the font and its licence from the noto-emoji
# repository at the v2.051 tag (Ubuntu's deb is zstd-compressed and built
# from the source tarball, 150 MB)
get NotoColorEmoji-2.051.ttf        https://raw.githubusercontent.com/googlefonts/noto-emoji/v2.051/fonts/NotoColorEmoji.ttf
get NotoColorEmoji-2.051-OFL.txt    https://raw.githubusercontent.com/googlefonts/noto-emoji/v2.051/fonts/LICENSE
# Every file present must match; all but GTK (not built yet) must be there.
sha256sum -c --ignore-missing "$REPO_DIR/build/SHA256SUMS.txt"
for f in $(awk '{print $2}' "$REPO_DIR/build/SHA256SUMS.txt" | grep -v '^gtk+3.0_'); do
  [ -s "$f" ] || { echo "fetch-sources.sh: $f is missing" >&2; exit 1; }
done
