#!/bin/bash -e
# !GTKGLTest (tests/gtk/gltest.c): GtkGLArea with riscos-mesa's EGL and
# OSMesa linked in. Needs riscos-mesa's devkit (20.3.5-15 or later; crossdev 1.5 has 20.3.5-15) in
# MESA_DEVKIT (default: $RCD_DIR/devkit/env, where riscos-crossdev's
# devkit/install.sh puts it). See build-app.sh.
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/env.sh"
: "${MESA_DEVKIT:=$RCD_DIR/devkit/env}"
[ -f "$MESA_DEVKIT/lib/libEGL.a" ] && [ -f "$MESA_DEVKIT/lib/libOSMesa.a" ] || {
  echo "build-gltest.sh: no riscos-mesa devkit in MESA_DEVKIT=$MESA_DEVKIT" >&2; exit 1; }
F=$(mktemp -d); trap 'rm -rf "$F"' EXIT
cp "$HERE"/../tests/gtk/gltest-app/* "$F/"
cp "$MESA_DEVKIT/LICENCES.txt" "$F/LicenceMesa,fff"
export EXTRA_LIBS="-Wl,-u,eglGetProcAddress -L$MESA_DEVKIT/lib -lEGL -lOSMesa -lstdc++ -lz -lm"
exec "$HERE/build-app.sh" GTKGLTest gltest "${VERSION:-3.24.41-riscos1.0-rc5}" "$F" "$HERE/../tests/gtk/gltest.c"
