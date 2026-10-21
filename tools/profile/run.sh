#!/bin/bash
# tools/profile/run.sh MODE N: prof_gtk under QEMU with insprof.so, its
# profile and memory map in $OUT (default /tmp/prof/out) as MODE-N.txt and
# MODE-N.maps. Needs the linuxarm GTK (TARGET=linuxarm SRC= SYSROOT=),
# INSPROF (the built insprof.so), MAPSDUMP (mapsdump.so) and PROF_GTK.
# MIT licence.
set -e
. "$(dirname "$0")/../../build/env.sh" > /dev/null
OUT=${OUT:-/tmp/prof/out}; mkdir -p "$OUT"
QEMU_SET_ENV="LD_PRELOAD=$MAPSDUMP,INSPROF_MAPS=$OUT/$1-$2.maps" \
QEMU_ARGS="-plugin $INSPROF,out=$OUT/$1-$2.txt" QEMU=$GCCSDK_ENV/bin/qemu-arm-aligntrap \
  timeout 1200 "$RCD_DIR"/qemu/aligntrap.sh "$PROF_GTK" "$REPO_DIR/out/hello-linuxarm-app" "$1" "$2" 2>&1 | grep '^prof_gtk'
