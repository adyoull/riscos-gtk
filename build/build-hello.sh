#!/bin/bash -e
# !GTKHello (tests/gtk/hello.c): see build-app.sh.
HERE=$(cd "$(dirname "$0")" && pwd)
exec "$HERE/build-app.sh" GTKHello hello "${VERSION:-0.5}" "$HERE/../tests/gtk/app" "$HERE/../tests/gtk/hello.c"
