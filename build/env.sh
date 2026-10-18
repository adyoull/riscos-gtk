# Source this. Settings shared by the riscos-gtk build scripts.
# Adapted from riscos-warzone2100 build/env.sh (GPL version 2 or later; see LICENCES.md).
#
# Based on riscos-warzone2100's build/env.sh, with the toolchain taken from
# riscos-crossdev (a tagged release, never main):
#   RCD_DIR   a riscos-crossdev checkout at the tag .riscos-deps names (or the
#             container image's /opt/riscos-crossdev) with the
#             toolchain installed (make prebuilt; make devkit for the GL
#             test and !GTKGLTest). Default: ../riscos-crossdev.
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
# The scripts ask git for the commit (VERSION files, the devkit, the
# release). In a container the checkout often belongs to another user
# (GitHub's actions/checkout, a mounted folder) and git then refuses with
# "dubious ownership"; trust this repository and riscos-crossdev's for the
# build's git commands only, without touching the user's git config.
if [ "$RISCOS_GTK_GIT_SAFE" != "$REPO_DIR:$RCD_DIR" ]; then
  _n=${GIT_CONFIG_COUNT:-0}
  for _d in "$REPO_DIR" "$RCD_DIR"; do
    export GIT_CONFIG_KEY_$_n=safe.directory GIT_CONFIG_VALUE_$_n="$_d"
    _n=$((_n + 1))
  done
  export GIT_CONFIG_COUNT=$_n RISCOS_GTK_GIT_SAFE="$REPO_DIR:$RCD_DIR"
  unset _n _d
fi
case "$STAGE|$SRC|$DL|$GCCSDK_ENV" in *" "*)
  echo "env.sh: STAGE, SRC, DL and the toolchain path must not contain spaces" >&2
  return 1 2>/dev/null || exit 1;;
esac
[ -x "$GCCSDK_ENV/bin/arm-riscos-gnueabihf-gcc" ] || {
  echo "env.sh: no toolchain in $GCCSDK_ENV: run 'make prebuilt' in $RCD_DIR" >&2
  return 1 2>/dev/null || exit 1; }

# riscos-crossdev 1.3 (UnixLib 5.0.3.2: LLONG_MIN fixed, getservbyname_r,
# eventfd safe with threads; static-only toolchain) or later is needed:
# harfbuzz won't compile with 5.0.3.1's LLONG_MIN. Stop early otherwise.
if grep -q 'define LLONG_MIN 0x8000000000000000LL' "$GCCSDK_ENV/arm-riscos-gnueabihf/include/limits.h" ||
   [ -e "$GCCSDK_ENV/arm-riscos-gnueabihf/lib/libunixlib.so" ]; then
  echo "env.sh: $GCCSDK_ENV is older than riscos-crossdev 1.3 (UnixLib 5.0.3.2," >&2
  echo "  static only): use riscos-crossdev v1.8.2 (make prebuilt there)" >&2
  return 1 2>/dev/null || exit 1
fi

# Which UnixLib the programs link: riscos-crossdev 1.4 and later have
# UnixLib 5.0.3.3 (built from its release's unixlib-riscos.diff), which
# the releases need; 1.3 has 5.0.3.2. 5.0.3.3 is the one with
# __pthread_held_wait.
if "$GCCSDK_ENV/bin/arm-riscos-gnueabihf-nm" "$GCCSDK_ENV/arm-riscos-gnueabihf/lib/libunixlib.a" 2>/dev/null |
     grep -q ' T __pthread_held_wait$'; then
  UNIXLIB_VERSION=5.0.3.3
else
  UNIXLIB_VERSION=5.0.3.2
fi
export UNIXLIB_VERSION
if [ "$TARGET" = riscos ] && [ "$UNIXLIB_VERSION" != 5.0.3.3 ]; then
  echo "env.sh: $GCCSDK_ENV links UnixLib $UNIXLIB_VERSION; riscos-gtk needs 5.0.3.3:" >&2
  echo "  use riscos-crossdev v1.8.2 (or v1.4 and later), make prebuilt there" >&2
  return 1 2>/dev/null || exit 1
fi

export BUILD=$(gcc -dumpmachine)
if [ "$TARGET" = riscos ]; then
  export HOST=arm-riscos-gnueabihf TCBIN=$GCCSDK_ENV/bin MESON_SYSTEM=riscos CMAKE_SYSTEM=GNU
  # As riscos-warzone2100 / riscos-mesa (Pi 4 benchmarked).
  # -fstack-clash-protection is REQUIRED (ARMEABISupport's stack guard page).
  # -ffile-prefix-map keeps this machine's paths out of __FILE__ and debug
  # info: the repository's, and SRC's and SYSROOT's when they're elsewhere
  # (headers such as GLib's gobjectnotifyqueue.c are included from SYSROOT).
  # The last matching map wins, so the more specific ones come last.
  export RO_CFLAGS="-O3 -mtune=cortex-a72 -mfpu=$FPU -mfloat-abi=hard -fstack-clash-protection -ffile-prefix-map=$REPO_DIR=/riscos-gtk-src -ffile-prefix-map=$SRC=/riscos-gtk-src/src -ffile-prefix-map=$SYSROOT=/riscos-gtk-src/sysroot"
else
  export HOST=arm-linux-gnueabihf TCBIN=/usr/bin MESON_SYSTEM=linux CMAKE_SYSTEM=Linux
  # Cortex-A8/VFPv3 and no unaligned accesses, as on RISC OS; -g for gdb.
  export RO_CFLAGS="-O2 -g -mcpu=cortex-a8 -mfpu=vfpv3 -mfloat-abi=hard -mno-unaligned-access -fstack-clash-protection -ffile-prefix-map=$REPO_DIR=/riscos-gtk-src"
fi
export CC=$HOST-gcc CXX=$HOST-g++ AR=$HOST-ar RANLIB=$HOST-ranlib STRIP=$HOST-strip
export CFLAGS="$RO_CFLAGS" CXXFLAGS="$RO_CFLAGS"
export CPPFLAGS="-I$STAGE/include" LDFLAGS="-L$STAGE/lib"
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
