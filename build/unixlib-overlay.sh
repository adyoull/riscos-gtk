#!/bin/bash -e
# Make a copy of riscos-crossdev's toolchain with a newer UnixLib release
# dropped in, for testing a riscos-unixlib pre-release before riscos-crossdev
# ships it. Only libunixlib.a and the headers that release changed are
# replaced, and both are checked against the release:
#
#   build/unixlib-overlay.sh RELEASE_DIR OUT_DIR
#     RELEASE_DIR  the riscos-unixlib release files: libunixlib.a,
#                  unixlib-riscos.diff and SHA256SUMS (e.g. v5.0.3.2-rc2)
#     OUT_DIR      where to put the toolchain copy (build/env.sh uses
#                  $REPO_DIR/toolchain-unixlib by itself; elsewhere set
#                  RISCOS_TOOLCHAIN=OUT_DIR)
#
# The headers come from unixlib-riscos.diff: each header that diff changes
# differently from the UnixLib diff the toolchain was built with
# (riscos-crossdev's toolchain/SOURCES) is rebuilt from GCCSDK's original
# plus the new hunks. For 5.0.3.2-rc1/rc2 that is only include/limits.h.
# Not build/env.sh: that refuses a toolchain with the old UnixLib, which is
# exactly what this starts from.
REPO_DIR=$(cd "$(dirname "$0")/.." && pwd)
: "${RCD_DIR:=$REPO_DIR/../riscos-crossdev}"
. "$RCD_DIR/env.sh"
GCCSDK_ENV=$RISCOS_TOOLCHAIN
: "${DL:=$REPO_DIR/dl}"
REL=$(cd "$1" && pwd); OUT=$2
[ -n "$OUT" ] || { echo "usage: $0 RELEASE_DIR OUT_DIR" >&2; exit 2; }
( cd "$REL" && sha256sum -c --ignore-missing SHA256SUMS )
rm -rf "$OUT"; cp -a "$GCCSDK_ENV" "$OUT"
cp "$REL/libunixlib.a" "$OUT/arm-riscos-gnueabihf/lib/libunixlib.a"
# The toolchain also has shared libraries (the old UnixLib, libm and
# libstdc++ built against it) and libtool .la files that point libtool at
# them even when linking statically. riscos-gtk links everything
# statically and the release has no shared library, so this copy is made
# static-only: no link can pick up the old UnixLib.
rm -f "$OUT"/arm-riscos-gnueabihf/lib/*.so* "$OUT"/arm-riscos-gnueabihf/lib/*.la
# Headers: apply the new diff's include/ hunks to the toolchain's current
# headers by reversing the old diff's and applying the new one's.
OLD=$DL/unixlib-riscos-5.0.3.1.diff
[ -f "$OLD" ] || { echo "unixlib-overlay.sh: need the toolchain's UnixLib diff as $DL/unixlib-riscos-5.0.3.1.diff (riscos-crossdev toolchain/SOURCES)" >&2; exit 1; }
python3 - "$OLD" "$REL/unixlib-riscos.diff" "$OUT/arm-riscos-gnueabihf/include" <<'PY'
import sys, subprocess, os, re
def files(p):
    d = {}; cur = None
    for l in open(p, encoding='latin-1'):
        if l.startswith('diff '):
            cur = None
        elif l.startswith('+++ '):
            cur = l.split()[1]; d[cur] = []
        if cur is not None:
            d[cur].append(l)
    return d
old, new, inc = files(sys.argv[1]), files(sys.argv[2]), sys.argv[3]
for f in sorted(set(old) | set(new)):
    m = re.match(r'b/gcc4/recipe/files/gcc/libunixlib/include/(.*)', f)
    if not m or old.get(f) == new.get(f):
        continue
    target = os.path.join(inc, m.group(1))
    for diff, rev in ((old.get(f), True), (new.get(f), False)):
        if not diff:
            continue
        text = '--- a\n' + ''.join(diff)
        subprocess.run(['patch', '-s', '-p0'] + (['-R'] if rev else []) + [target],
                       input=text.encode('latin-1'), check=True)
    print('header updated:', m.group(1))
PY
echo "toolchain with $(basename "$REL") in $OUT: build with RISCOS_TOOLCHAIN=$OUT"
