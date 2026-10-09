#!/bin/bash -e
# Build a riscos-gtk release (or release candidate) from a clean checkout:
# the three programs, the complete corresponding source, and a manifest.
#   RCD_DIR=... [SRC=... SYSROOT=...] build/make-release.sh 3.24.41-riscos1.0-rc3
# Needs the libraries already built (make deps) with the same toolchain.
# Writes out/release/<RELEASE>/:
#   L3afpad-<v>.zip, ClawsMail-<v>.zip, GTKThemeTest-<v>.zip
#   riscos-gtk-<RELEASE>-src.tar   this repository at the release commit
#                                  (git archive) and every upstream tarball
#                                  in build/SHA256SUMS.txt (dl/): the source
#                                  the LGPL and GPL ask for (docs/SOURCE-AND-LICENCES.md)
#   MANIFEST.txt                   sha256 of each file, the commit, the
#                                  toolchain and UnixLib it was built with
# The programs' versions are in build/build-<app>.sh (or L3AFPAD_VERSION,
# CLAWS_VERSION, THEMETEST_VERSION). MIT licence.
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
[ "$UNIXLIB_VERSION" = 5.0.3.3 ] || echo "make-release.sh: note: UnixLib ${UNIXLIB_VERSION:-unknown}, not 5.0.3.3" >&2
(cd "$DL" && sha256sum -c --quiet "$REPO_DIR/build/SHA256SUMS.txt")

L3V=${L3AFPAD_VERSION:-$(sed -n 's/.*VERSION:-\([0-9.]*\)}.*/\1/p' build/build-l3afpad.sh | head -1)}
TTV=${THEMETEST_VERSION:-$(sed -n 's/.*VERSION:-\([0-9.]*\)}.*/\1/p' build/build-themetest.sh | head -1)}
CMV=${CLAWS_VERSION:-$(sed -n 's/^VERSION=\${VERSION:-\([0-9.]*\)}.*/\1/p' build/build-claws.sh | head -1)}
[ -n "$L3V" ] && [ -n "$TTV" ] && [ -n "$CMV" ] || { echo "make-release.sh: can't read the programs' versions" >&2; exit 1; }

VERSION=$L3V build/build-l3afpad.sh
VERSION=$TTV build/build-themetest.sh
VERSION=$CMV build/build-claws.sh

R=$REPO_DIR/out/release/$RELEASE
rm -rf "$R"; mkdir -p "$R"
cp "out/L3afpad-$L3V.zip" "out/GTKThemeTest-$TTV.zip" "out/ClawsMail-$CMV.zip" "$R/"

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
  (cd "$R" && sha256sum *.zip *.tar)
} > "$R/MANIFEST.txt"
cat "$R/MANIFEST.txt"
