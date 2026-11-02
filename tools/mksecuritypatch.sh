#!/bin/bash
# mksecuritypatch.sh ORIG DIR DEBIAN [EXCLUDE] [EXTRA]: make
# patches/security/DIR.diff from a Ubuntu/Debian package's debian/patches.
#   ORIG     the original tarball, as in dl/ (its top folder is DIR)
#   DIR      the folder the tarball unpacks to
#   DEBIAN   the package's .debian.tar.xz (from archive.ubuntu.com)
#   EXCLUDE  a regular expression: patches to leave out ("^$" for none)
#   EXTRA    patch names (without .patch) to include besides those whose
#            names contain CVE or security, space separated
# The patches are applied to a copy of ORIG in debian/patches/series order
# and the two trees compared. See patches/security/README.md. MIT licence.
set -eu
O=$1; D=$2; T=$3; EX=${4:-'^$'}; EXTRA=${5:-}
REPO_DIR=$(cd "$(dirname "$0")/.." && pwd)
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
mkdir -p "$W/a" "$W/b" "$W/deb"
tar xf "$O" -C "$W/a"; tar xf "$O" -C "$W/b"; tar xf "$T" -C "$W/deb"
S=$W/deb/debian/patches
list=$(grep -v '^#' "$S/series" | awk 'NF {print $1}' | while read -r p; do
  n=${p%.patch}; n=${n%.diff}
  if echo "$p" | grep -qiE 'CVE|security' || echo " $EXTRA " | grep -q " $n "; then echo "$p"; fi
done | grep -vE "$EX" || true)
for p in $list; do patch -d "$W/b/$D" -p1 -s < "$S/$p"; done
find "$W/b" -name '*.orig' -delete
ver=$(basename "$T" | sed -E 's/^[^_]+_(.+)\.debian\.tar\..*$/\1/')
out=$REPO_DIR/patches/security/$D.diff
{ echo "riscos-gtk: security fixes for $D, from Ubuntu's package $ver"
  echo "($(basename "$T"): debian/patches), applied in its series order:"
  for p in $list; do echo "  $p"; done
  echo "Made by tools/mksecuritypatch.sh from $(basename "$O")."
  echo
  (cd "$W/a" && diff -ruN "$D" "../b/$D" |
     sed -E "s#^diff -ruN $D/(.*) \.\./b/$D/(.*)#diff -ruN a/\1 b/\2#; s#^--- $D/#--- a/#; s#^\+\+\+ \.\./b/$D/#+++ b/#; s#^(---|\+\+\+) ([^\t]*)\t.*#\1 \2#") || true
} > "$out"
echo "$D: $(echo $list | wc -w) patches, $(grep -c '^+++' "$out") files -> patches/security/$D.diff"
