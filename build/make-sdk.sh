#!/bin/bash -e
# Build the riscos-gtk devkit: what another developer needs to build GTK 3
# programs for RISC OS with riscos-crossdev, without building riscos-gtk.
#   RCD_DIR=... [SRC=... SYSROOT=...] build/make-sdk.sh [RELEASE]
# Needs the libraries already built (make deps). Writes
# out/sdk/riscos-gtk-sdk-<RELEASE>.tar.gz, unpacking to
# riscos-gtk-sdk-<RELEASE>/:
#   env.sh            source it: CC, pkg-config, RISCOS_GTK_CFLAGS/LIBS
#   bin/              compiler wrappers, riscos-gtk-package,
#                     riscos-gtk-meson-cross, riscos-gtk-staticmodules
#                     (tools/mkstaticmodules.py)
#   sysroot/riscos-gtk/  headers, static libraries, .pc files, aclocal
#                     macros and GTK's GSettings schemas: $STAGE without
#                     the programs and the libraries only ClawsMail uses
#   share/riscos-gtk/ what riscos-gtk-package puts in an application:
#                     the RISCOS theme, icons, compiled schemas, the MIME
#                     database (build/mkmime.sh), the icon drawing
#                     (tools/riscosdraw.py, mkriscosicons.py), DejaVu
#                     fonts, the LGPL and the Source template; and a CMake
#                     toolchain file
#   docs/             README, API (the RISC OS functions), PORTING
#   template/         a small program, its !App files and a Makefile
#   examples/glarea/  OpenGL in a GtkGLArea (needs riscos-mesa's devkit)
#   VERSION           the release, commit, toolchain and UnixLib
# RELEASE defaults to "dev-<commit>". The libraries are LGPL: the devkit
# must be published with (or point to) riscos-gtk's source archive for
# the same commit (make-release.sh makes both). MIT licence.
HERE=$(cd "$(dirname "$0")" && pwd)
export TARGET=riscos
. "$HERE/env.sh"
cd "$REPO_DIR"
COMMIT=$(git rev-parse HEAD)
DIRTY=
[ -z "$(git status --porcelain --untracked-files=no)" ] || DIRTY=" (with uncommitted changes)"
RELEASE=${1:-dev-$(git rev-parse --short=12 HEAD)}
SOURCE_URL=${SOURCE_URL:-https://github.com/adyoull/riscos-gtk/releases/tag/v$RELEASE}
case $RELEASE in dev-*) SOURCE_URL=${SOURCE_URL_DEV:-https://github.com/adyoull/riscos-gtk/tree/$COMMIT} ;; esac
N=riscos-gtk-sdk-$RELEASE
O=$REPO_DIR/out/sdk
D=$O/$N
rm -rf "$D"; mkdir -p "$D"

# --- the libraries (sysroot/riscos-gtk, prefix /riscos-gtk as built) ---
P=$D/sysroot/riscos-gtk
mkdir -p "$P/lib" "$P/share/glib-2.0"
cp -r "$STAGE/include" "$P/"
cp "$STAGE"/lib/*.a "$P/lib/"
cp -r "$STAGE/lib/pkgconfig" "$STAGE/lib/glib-2.0" "$STAGE/lib/cmake" "$P/lib/"
cp -r "$STAGE/share/aclocal" "$P/share/"
cp -r "$STAGE/share/glib-2.0/schemas" "$P/share/glib-2.0/"
[ ! -d "$STAGE/share/pkgconfig" ] || cp -r "$STAGE/share/pkgconfig" "$P/share/"
# ClawsMail's own libraries (libetpan, Nettle, gumbo) aren't GTK's
rm -rf "$P"/lib/libetpan.* "$P"/lib/libnettle.* "$P"/lib/libhogweed.* "$P"/lib/libgumbo.* \
       "$P"/lib/pkgconfig/libetpan.pc "$P"/lib/pkgconfig/nettle.pc "$P"/lib/pkgconfig/hogweed.pc \
       "$P"/lib/pkgconfig/gumbo.pc "$P"/include/libetpan "$P"/include/libetpan.h \
       "$P"/include/nettle "$P"/include/gumbo.h "$P"/include/tag_enum.h
# nor Gnumeric's (GOffice is GPL; librsvg is riscos-gtk's stand-in)
rm -rf "$P"/lib/libgoffice-0.10.* "$P"/lib/libgsf-1.* "$P"/lib/libxml2.* "$P"/lib/libxslt.* \
       "$P"/lib/libexslt.* "$P"/lib/librsvg-2.* "$P"/lib/cmake/libxml2 "$P"/lib/xml2Conf.sh \
       "$P"/lib/xsltConf.sh "$P"/share/aclocal/libxml.m4 \
       "$P"/lib/pkgconfig/libgoffice-0.10.pc "$P"/lib/pkgconfig/libgsf-1.pc \
       "$P"/lib/pkgconfig/libxml-2.0.pc "$P"/lib/pkgconfig/libxslt.pc \
       "$P"/lib/pkgconfig/libexslt.pc "$P"/lib/pkgconfig/librsvg-2.0.pc \
       "$P"/include/libgoffice-0.10 "$P"/include/libgsf-1 "$P"/include/libxml2 \
       "$P"/include/libxslt "$P"/include/libexslt "$P"/include/librsvg-2.0
find "$P" -name '*.la' -delete
# without debug information: a quarter of the size (the programs are
# stripped when packaged anyway)
for a in "$P"/lib/*.a; do "$GCCSDK_ENV/bin/$HOST-strip" --strip-debug "$a"; done
mkdir -p "$P/include/gtk-3.0/riscos-gtk"
cp "$REPO_DIR/sdk/include/riscos-gtk/app.h" "$P/include/gtk-3.0/riscos-gtk/"
# no build-machine paths
if grep -rIl -e "$REPO_DIR" -e "$SYSROOT" -e "$SRC" -e "$(cd "$RCD_DIR" && pwd -P)" "$P" | grep .; then
  echo "make-sdk.sh: build paths in the files above" >&2; exit 1
fi

# --- the devkit's own files ---
cp "$REPO_DIR/sdk/env.sh" "$D/"
cp -r "$REPO_DIR/sdk/bin" "$REPO_DIR/sdk/template" "$REPO_DIR/sdk/examples" "$D/"
cp "$REPO_DIR/tools/mkstaticmodules.py" "$D/bin/riscos-gtk-staticmodules"
chmod +x "$D"/bin/*
mkdir -p "$D/docs"
cp "$REPO_DIR"/sdk/docs/*.md "$D/docs/"
cp "$REPO_DIR/LICENCES.md" "$D/docs/LICENCES.md"
cp "$REPO_DIR/sdk/docs/README.md" "$D/README.md"

S=$D/share/riscos-gtk
mkdir -p "$S/theme" "$S/fonts" "$S/schemas" "$S/licences"
cp "$REPO_DIR/sdk/share/riscos-gtk/riscos-gtk.cmake" "$S/"
cp -r "$REPO_DIR/theme/gtk-3.0" "$S/theme/"
ADW=$SRC/adwaita-icon-theme-3.36.1
[ -d "$ADW" ] || tar -xf "$DL/adwaita-icon-theme_3.36.1.orig.tar.xz" -C "$SRC"
python3 "$REPO_DIR/build/mkicons.py" "$ADW/Adwaita" "$REPO_DIR/theme/icons-list.txt" "$S/icons"
glib-compile-schemas --strict --targetdir="$S/schemas" "$STAGE/share/glib-2.0/schemas"
"$REPO_DIR/build/mkmime.sh" "$S/mime"
unzip -q -o -j "$DL/dejavu-fonts-ttf-2.37.zip" 'dejavu-fonts-ttf-2.37/ttf/*.ttf' \
  'dejavu-fonts-ttf-2.37/LICENSE' -d "$S/fonts"
mv "$S/fonts/LICENSE" "$S/fonts/Licence,fff"
cp "$REPO_DIR/tests/render/mkfontsconf.sh" "$S/"
mkdir -p "$S/tools"
cp "$REPO_DIR/tools/riscosdraw.py" "$REPO_DIR/tools/mkriscosicons.py" "$REPO_DIR/tools/riscosicons_apps.py" "$S/tools/"
cp "$REPO_DIR/riscos/licences/LGPL-2.1.txt" "$S/licences/"
sed -e "s|@RISCOS_GTK_RELEASE@|$RELEASE|g" -e "s|@RISCOS_GTK_SOURCE_URL@|$SOURCE_URL|g" \
  "$REPO_DIR/sdk/share/riscos-gtk/licences/Source.txt" > "$S/licences/Source.txt"

{
  echo "riscos-gtk devkit $RELEASE"
  echo "GTK 3.24.41, riscos-gtk commit $COMMIT$DIRTY"
  echo "built with riscos-crossdev $(cd "$RCD_DIR" && git describe --tags 2>/dev/null || echo 1.3), UnixLib ${UNIXLIB_VERSION:-unknown}"
  echo "source: $SOURCE_URL"
  echo "built $(date -u +%Y-%m-%dT%H:%MZ)"
} > "$D/VERSION"

rm -f "$O/$N.tar.gz"
tar -czf "$O/$N.tar.gz" -C "$O" --owner=0 --group=0 --sort=name "$N"
du -sh "$D" "$O/$N.tar.gz"
sha256sum "$O/$N.tar.gz"
