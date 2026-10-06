#!/bin/bash -e
# Until riscos-crossdev has a release with UnixLib 5.0.3.3 (crossdev 1.3 has
# 5.0.3.2): make a copy of a crossdev 1.3 checkout whose toolchain links the
# UnixLib 5.0.3.3 release's libunixlib.a instead. Its public headers are the
# same as 5.0.3.2's (no struct or argument changes), so only the library
# changes; the copy shares every other file with the original (hard links).
#   build/unixlib-overlay.sh CROSSDEV_1_3_DIR NEW_DIR LIBUNIXLIB_A
# then build with RCD_DIR=NEW_DIR. LIBUNIXLIB_A is release/v5.0.3.3/
# libunixlib.a from riscos-unixlib (sha256 checked).
SHA=e24f16c3f4612aae3871ceff521ab86b0d3ac4cc689d734e0e4085d02c774adf
[ $# = 3 ] || { echo "usage: $0 CROSSDEV_1_3_DIR NEW_DIR LIBUNIXLIB_A" >&2; exit 2; }
FROM=$1 TO=$2 LIB=$3
echo "$SHA  $LIB" | sha256sum -c --quiet || { echo "$LIB isn't UnixLib 5.0.3.3's libunixlib.a" >&2; exit 1; }
[ -d "$FROM/toolchain/env" ] || { echo "no toolchain in $FROM" >&2; exit 1; }
rm -rf "$TO"
cp -al "$FROM" "$TO"
T=$TO/toolchain/env/arm-riscos-gnueabihf/lib/libunixlib.a
rm "$T"
cp "$LIB" "$T"
echo "$TO: riscos-crossdev 1.3 with UnixLib 5.0.3.3"
