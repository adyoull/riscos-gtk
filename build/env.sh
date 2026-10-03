# Source this. Settings shared by the riscos-gtk build scripts.
# Adapted from riscos-warzone2100 build/env.sh (GPL version 2 or later; see LICENCES.md).
#
# Based on riscos-warzone2100's build/env.sh, with the toolchain taken from
# riscos-crossdev (a tagged release, never main):
#   RCD_DIR   a riscos-crossdev checkout at tag v1.2 with the toolchain
#             installed (make prebuilt). Default: ../riscos-crossdev.
#   SYSROOT   the libraries built here are installed into $SYSROOT$PREFIX
#             (= STAGE). PREFIX (/riscos-gtk) is what the libraries are
#             configured with, so that no build-machine path is compiled
#             into them; pkg-config adds SYSROOT back (PKG_CONFIG_SYSROOT_DIR).
#             At run time on RISC OS the !Run files point GLib/GTK at their
#             data with environment variables instead.
#   SRC       unpacked, patched sources.
#   DL        downloaded source tarballs (build/fetch-sources.sh).
# None of these may contain spaces (autoconf/libtool/meson can't cope).
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
: "${RCD_DIR:=$REPO_DIR/../riscos-crossdev}"
# UnixLib 5.0.3.2 (LLONG_MIN fixed, getservbyname_r added, eventfd safe with
# threads) isn't in a riscos-crossdev release yet. Until it is, the build
# uses a copy of crossdev 1.2's toolchain with the riscos-unixlib release
# dropped in (build/unixlib-overlay.sh; `make toolchain-unixlib`), in
# $REPO_DIR/toolchain-unixlib, when that exists.
[ -z "${RISCOS_TOOLCHAIN:-}" ] && [ -x "$REPO_DIR/toolchain-unixlib/bin/arm-riscos-gnueabihf-gcc" ] \
  && export RISCOS_TOOLCHAIN=$REPO_DIR/toolchain-unixlib
[ -f "$RCD_DIR/env.sh" ] || { echo "env.sh: no riscos-crossdev in RCD_DIR=$RCD_DIR" >&2; return 1 2>/dev/null || exit 1; }
. "$RCD_DIR/env.sh"
GCCSDK_ENV=$RISCOS_TOOLCHAIN
# TARGET=riscos (default) builds for RISC OS. TARGET=linuxarm builds the
# same libraries, from the same sources and patches, as ARM Linux code with
# the code generation RISC OS gets, for the tests under riscos-crossdev's
# qemu-arm-aligntrap (build/build-linuxarm.sh).
: "${TARGET:=riscos}"
case $TARGET in
  riscos)   SUFFIX= ;;
  linuxarm) SUFFIX=-linuxarm ;;
  *) echo "env.sh: TARGET must be riscos or linuxarm" >&2; return 1 2>/dev/null || exit 1;;
esac
: "${SYSROOT:=$REPO_DIR/sysroot$SUFFIX}"
PREFIX=/riscos-gtk
STAGE=$SYSROOT$PREFIX
: "${SRC:=$REPO_DIR/src$SUFFIX}"
: "${DL:=$REPO_DIR/dl}"
export TARGET REPO_DIR RCD_DIR GCCSDK_ENV SYSROOT PREFIX STAGE SRC DL
case "$STAGE|$SRC|$DL|$GCCSDK_ENV" in *" "*)
  echo "env.sh: STAGE, SRC, DL and the toolchain path must not contain spaces" >&2
  return 1 2>/dev/null || exit 1;;
esac
[ -x "$GCCSDK_ENV/bin/arm-riscos-gnueabihf-gcc" ] || {
  echo "env.sh: no toolchain in $GCCSDK_ENV: run 'make prebuilt' in $RCD_DIR" >&2
  return 1 2>/dev/null || exit 1; }

# The libraries need UnixLib 5.0.3.2's headers (harfbuzz won't compile with
# 5.0.3.1's LLONG_MIN): stop early with a clear message otherwise.
if grep -q 'define LLONG_MIN 0x8000000000000000LL' "$GCCSDK_ENV/arm-riscos-gnueabihf/include/limits.h"; then
  echo "env.sh: $GCCSDK_ENV has UnixLib 5.0.3.1 or older; make a toolchain with" >&2
  echo "  UnixLib 5.0.3.2 first: make toolchain-unixlib UNIXLIB_RELEASE=<release dir>" >&2
  return 1 2>/dev/null || exit 1
fi

export BUILD=$(gcc -dumpmachine)
if [ "$TARGET" = riscos ]; then
  export HOST=arm-riscos-gnueabihf TCBIN=$GCCSDK_ENV/bin MESON_SYSTEM=riscos CMAKE_SYSTEM=GNU
  # As riscos-warzone2100 / riscos-mesa (Pi 4 benchmarked).
  # -fstack-clash-protection is REQUIRED (ARMEABISupport's stack guard page).
  # -ffile-prefix-map keeps this machine's paths out of __FILE__ and debug info.
  export RO_CFLAGS="-O3 -mtune=cortex-a72 -mfpu=$FPU -mfloat-abi=hard -fstack-clash-protection -ffile-prefix-map=$REPO_DIR=/riscos-gtk-src"
else
  export HOST=arm-linux-gnueabihf TCBIN=/usr/bin MESON_SYSTEM=linux CMAKE_SYSTEM=Linux
  # Cortex-A8/VFPv3 and no unaligned accesses, as on RISC OS; -g for gdb.
  export RO_CFLAGS="-O2 -g -mcpu=cortex-a8 -mfpu=vfpv3 -mfloat-abi=hard -mno-unaligned-access -fstack-clash-protection -ffile-prefix-map=$REPO_DIR=/riscos-gtk-src"
fi
export CC=$HOST-gcc CXX=$HOST-g++ AR=$HOST-ar RANLIB=$HOST-ranlib STRIP=$HOST-strip
export CFLAGS="$RO_CFLAGS" CXXFLAGS="$RO_CFLAGS"
export CPPFLAGS="-I$STAGE/include" LDFLAGS="-L$STAGE/lib"
[ "$TARGET" = riscos ] && LDFLAGS="$LDFLAGS -static"
export PKG_CONFIG_LIBDIR="$STAGE/lib/pkgconfig:$STAGE/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR=$SYSROOT
mkdir -p "$STAGE/lib/pkgconfig" "$STAGE/include" "$SRC" "$DL"

# configure scripts older than 2018 don't know "riscos": give them a current
# config.sub/config.guess (from the host's automake).
fresh_config_sub() {   # dir
  local d=$1 f
  for f in config.sub config.guess; do
    find "$d" -name $f -exec cp "$(ls /usr/share/automake-*/$f | tail -1)" {} \;
  done
}

# GLib's meson options, shared by build-deps.sh (RISC OS) and
# build-linuxarm.sh (the qemu test build). No subprojects are downloaded.
export GLIB_MESON_OPTS="--wrap-mode=nofallback -Dselinux=disabled -Dxattr=false \
 -Dman-pages=disabled -Ddtrace=false -Dsystemtap=false -Dsysprof=disabled \
 -Ddocumentation=false -Dtests=false -Dinstalled_tests=false -Dnls=disabled \
 -Dlibelf=disabled -Dintrospection=disabled -Dglib_debug=disabled"
