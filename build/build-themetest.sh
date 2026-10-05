#!/bin/bash -e
# !GTKThemeTest (tests/gtk/themetest.c): see build-app.sh.
HERE=$(cd "$(dirname "$0")" && pwd)
exec "$HERE/build-app.sh" GTKThemeTest themetest "${VERSION:-0.8}" "$HERE/../tests/gtk/themetest-app" \
  "$HERE/../tests/gtk/themetest.c"
