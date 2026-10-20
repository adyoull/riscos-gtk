#!/bin/bash -e
# Build a riscos-gtk release (or release candidate) from a clean checkout:
# the programs, the devkit, the complete corresponding source, and a manifest.
#   RCD_DIR=... [SRC=... SYSROOT=...] build/make-release.sh 3.24.41-riscos1.0-rc3
# Needs the libraries already built (make deps) with the same toolchain.
# Writes out/release/<RELEASE>/:
#   L3afpad-<v>.zip, ClawsMail-<v>.zip, Geany-<v>.zip, mtPaint-<v>.zip,
#   Gnumeric-<v>.zip, TTF-Noto-Color-Emoji-<v>.zip (the optional emoji
#   font, a PackMan package: build-emoji.sh),
#   GTKThemeTest-<v>.zip, GTKGLTest-<v>.zip (needs riscos-mesa's devkit:
#   MESA_DEVKIT, default $RCD_DIR/devkit/env)
#   riscos-gtk-sdk-<RELEASE>.tar.gz   the devkit (build/make-sdk.sh)
#   riscos-gtk-<RELEASE>-src.tar   this repository at the release commit
#                                  (git archive) and every upstream tarball
#                                  in build/SHA256SUMS.txt (dl/): the source
#                                  the LGPL and GPL ask for (docs/SOURCE-AND-LICENCES.md)
#   MANIFEST.txt                   sha256 of each file, the commit, the
#                                  toolchain and UnixLib it was built with
# The programs' versions (the upstream version and -riscosN; the test
# programs take the release's) are in build/build-<app>.sh (or L3AFPAD_VERSION,
# CLAWS_VERSION, THEMETEST_VERSION, GLTEST_VERSION, GEANY_VERSION,
# MTPAINT_VERSION, GNUMERIC_VERSION). MIT licence.
HERE=$(cd "$(dirname "$0")" && pwd)
RELEASE=${1:?usage: build/make-release.sh RELEASE (e.g. 3.24.41-riscos1.0-rc3)}
export TARGET=riscos RELEASE
export SOURCE_URL=${SOURCE_URL:-https://github.com/adyoull/riscos-gtk/releases/tag/v$RELEASE}
. "$HERE/env.sh"
cd "$REPO_DIR"

# from a committed tree only: the source archive is the commit
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
  echo "make-release.sh: uncommitted changes; commit first" >&2; exit 1
fi
COMMIT=$(git rev-parse HEAD)
(cd "$DL" && sha256sum -c --quiet "$REPO_DIR/build/SHA256SUMS.txt")

L3V=${L3AFPAD_VERSION:-$(sed -n 's/.*VERSION:-\([^}]*\)}.*/\1/p' build/build-l3afpad.sh | head -1)}
TTV=${THEMETEST_VERSION:-$(sed -n 's/.*VERSION:-\([^}]*\)}.*/\1/p' build/build-themetest.sh | head -1)}
CMV=${CLAWS_VERSION:-$(sed -n 's/^VERSION=\${VERSION:-\([^}]*\)}.*/\1/p' build/build-claws.sh | head -1)}
GLV=${GLTEST_VERSION:-$(sed -n 's/.*VERSION:-\([^}]*\)}.*/\1/p' build/build-gltest.sh | head -1)}
GYV=${GEANY_VERSION:-$(sed -n 's/^VERSION=\${VERSION:-\([^}]*\)}.*/\1/p' build/build-geany.sh | head -1)}
MPV=${MTPAINT_VERSION:-$(sed -n 's/^VERSION=\${VERSION:-\([^}]*\)}.*/\1/p' build/build-mtpaint.sh | head -1)}
GNV=${GNUMERIC_VERSION:-$(sed -n 's/^VERSION=\${VERSION:-\([^}]*\)}.*/\1/p' build/build-gnumeric.sh | head -1)}
[ -n "$L3V" ] && [ -n "$TTV" ] && [ -n "$CMV" ] && [ -n "$GLV" ] && [ -n "$GYV" ] && [ -n "$MPV" ] && [ -n "$GNV" ] \
  || { echo "make-release.sh: can't read the programs' versions" >&2; exit 1; }

VERSION=$L3V build/build-l3afpad.sh
VERSION=$TTV build/build-themetest.sh
VERSION=$CMV build/build-claws.sh
VERSION=$GLV build/build-gltest.sh
VERSION=$GYV build/build-geany.sh
VERSION=$MPV build/build-mtpaint.sh
VERSION=$GNV build/build-gnumeric.sh
build/build-emoji.sh
EMV=$(sed -n 's/^VERSION=\${VERSION:-\$FONT_VERSION\(.*\)}$/\1/p' build/build-emoji.sh)
EMV=$(sed -n 's/^FONT_VERSION=//p' build/build-emoji.sh)$EMV

R=$REPO_DIR/out/release/$RELEASE
rm -rf "$R"; mkdir -p "$R"
cp "out/L3afpad-$L3V.zip" "out/GTKThemeTest-$TTV.zip" "out/ClawsMail-$CMV.zip" \
   "out/GTKGLTest-$GLV.zip" "out/Geany-$GYV.zip" "out/mtPaint-$MPV.zip" \
   "out/Gnumeric-$GNV.zip" "out/TTF-Noto-Color-Emoji-$EMV.zip" "$R/"
# the devkit, for other developers' programs (build/make-sdk.sh)
SOURCE_URL=$SOURCE_URL "$HERE/make-sdk.sh" "$RELEASE"
cp "out/sdk/riscos-gtk-sdk-$RELEASE.tar.gz" "$R/"

# the corresponding source: the repository and the upstream tarballs
S=$(mktemp -d); trap 'rm -rf "$S"' EXIT
git archive --format=tar --prefix="riscos-gtk-$RELEASE/" "$COMMIT" > "$S/repo.tar"
mkdir -p "$S/riscos-gtk-$RELEASE/dl"
for f in $(awk '{print $2}' build/SHA256SUMS.txt); do cp "$DL/$f" "$S/riscos-gtk-$RELEASE/dl/"; done
cp "$S/repo.tar" "$R/riscos-gtk-$RELEASE-src.tar"
tar -rf "$R/riscos-gtk-$RELEASE-src.tar" -C "$S" "riscos-gtk-$RELEASE/dl"

{
  echo "riscos-gtk $RELEASE"
  echo "commit $COMMIT"
  echo "toolchain $(cd "$RCD_DIR" && git describe --tags 2>/dev/null || echo "riscos-crossdev (no git)") at $RCD_DIR"
  echo "UnixLib ${UNIXLIB_VERSION:-unknown}"
  echo "built $(date -u +%Y-%m-%dT%H:%MZ)"
  echo
  (cd "$R" && sha256sum *.zip *.tar *.tar.gz)
} > "$R/MANIFEST.txt"
cat "$R/MANIFEST.txt"
