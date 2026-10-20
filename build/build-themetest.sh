#!/bin/bash -e
# !GTKThemeTest (tests/gtk/themetest.c): see build-app.sh.
HERE=$(cd "$(dirname "$0")" && pwd)
exec "$HERE/build-app.sh" GTKThemeTest themetest "${VERSION:-3.24.41-riscos1.0-rc9}" "$HERE/../tests/gtk/themetest-app" \
  "$HERE/../tests/gtk/themetest.c"
