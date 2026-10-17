# riscos-gtk devkit: source this from bash to build GTK 3 programs for
# RISC OS with riscos-crossdev.
#
#   export RCD_DIR=/path/to/riscos-crossdev
#   . /path/to/riscos-gtk-sdk/env.sh
#
# RCD_DIR is riscos-crossdev v1.8.1 (or v1.4 and later), whose toolchain has UnixLib
# 5.0.3.3: the devkit's libraries are built against it (docs/README.md).
# Afterwards:
#   $CC $RISCOS_GTK_CFLAGS app.c -o app $RISCOS_GTK_LIBS
#   pkg-config --cflags --libs gtk+-3.0       (the devkit's, not the host's)
#   ./configure --host=arm-riscos-gnueabihf   (autotools: CC and
#                                              PKG_CONFIG_* are set)
#   meson setup build --cross-file "$(riscos-gtk-meson-cross)"
#   cmake -DCMAKE_TOOLCHAIN_FILE=$RISCOS_GTK_SDK/share/riscos-gtk/riscos-gtk.cmake
# OpenGL (GtkGLArea) needs riscos-mesa's devkit (20.3.5-15 or later; crossdev 1.8.1 has 20.3.5-16) in
# RISCOS_MESA_DEVKIT (default: crossdev's RISCOS_DEVKIT, where its
# devkit/install.sh puts it). Then RISCOS_GTK_GL_LIBS, and pkg-config's
# riscos-gtk-gl, add it to a program's link (docs/API.md, "OpenGL").
# MIT licence.
RISCOS_GTK_SDK=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
case $RISCOS_GTK_SDK in *" "*)
  echo "riscos-gtk env.sh: the devkit's path must not contain spaces" >&2
  return 1 2>/dev/null || exit 1;;
esac
[ -n "${RCD_DIR:-}" ] && [ -f "$RCD_DIR/env.sh" ] || {
  echo "riscos-gtk env.sh: set RCD_DIR to riscos-crossdev v1.8.1 first" >&2
  return 1 2>/dev/null || exit 1; }
. "$RCD_DIR/env.sh"
[ -x "$RISCOS_TOOLCHAIN/bin/arm-riscos-gnueabihf-gcc" ] || {
  echo "riscos-gtk env.sh: no toolchain in $RISCOS_TOOLCHAIN: run 'make prebuilt' in $RCD_DIR" >&2
  return 1 2>/dev/null || exit 1; }

# UnixLib 5.0.3.3 (crossdev 1.4 and later) is the one with __pthread_held_wait
if "$RISCOS_TOOLCHAIN/bin/arm-riscos-gnueabihf-nm" "$RISCOS_TOOLCHAIN/arm-riscos-gnueabihf/lib/libunixlib.a" 2>/dev/null |
     grep -q ' T __pthread_held_wait$'; then
  RISCOS_GTK_UNIXLIB=5.0.3.3
else
  RISCOS_GTK_UNIXLIB=other
  echo "riscos-gtk env.sh: warning: this toolchain's UnixLib isn't 5.0.3.3, which the devkit's" >&2
  echo "  libraries need: use riscos-crossdev v1.8.1 (or v1.4 and later)" >&2
fi

# The toolchain's .la files name the machine crossdev was built on
# (/opt/b14/... in 1.4 and 1.5). libtool reads them when it links C++
# (autotools programs with C++ in them, Geany for one) and stops:
# "cannot find the library '/opt/b14/.../libm.la'".
for la in "$RISCOS_TOOLCHAIN"/arm-riscos-gnueabihf/lib/*.la; do
  d=$(sed -n "s/^libdir='\(.*\)'/\1/p" "$la" 2>/dev/null)
  if [ -n "$d" ] && [ ! -d "$d" ]; then
    echo "riscos-gtk env.sh: note: the toolchain's .la files name $d, which isn't here, so" >&2
    echo "  libtool's C++ links will fail. To fix them (only the paths change):" >&2
    echo "  sed -i 's|${d%/arm-riscos-gnueabihf/lib}|$RISCOS_TOOLCHAIN|g' $RISCOS_TOOLCHAIN/arm-riscos-gnueabihf/lib/*.la" >&2
    break
  fi
done

RISCOS_GTK_SYSROOT=$RISCOS_GTK_SDK/sysroot
RISCOS_GTK_PREFIX=$RISCOS_GTK_SYSROOT/riscos-gtk
export RISCOS_GTK_SDK RISCOS_GTK_SYSROOT RISCOS_GTK_PREFIX RISCOS_GTK_UNIXLIB

# The devkit's bin first: compiler wrappers (they drop -pthread, which
# GCCSDK doesn't take; UnixLib's threads need no flag) and pkg-config
export PATH="$RISCOS_GTK_SDK/bin:$PATH"
export CC=arm-riscos-gnueabihf-gcc CXX=arm-riscos-gnueabihf-g++
export AR=arm-riscos-gnueabihf-ar RANLIB=arm-riscos-gnueabihf-ranlib STRIP=arm-riscos-gnueabihf-strip
# pkg-config with --static (bin/arm-riscos-gnueabihf-pkg-config): every
# link is static, so autotools must get the libraries' own dependencies
export PKG_CONFIG="$RISCOS_GTK_SDK/bin/arm-riscos-gnueabihf-pkg-config"
export PKG_CONFIG_LIBDIR="$RISCOS_GTK_PREFIX/lib/pkgconfig:$RISCOS_GTK_PREFIX/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR=$RISCOS_GTK_SYSROOT
unset PKG_CONFIG_PATH

export CFLAGS="${CFLAGS:-$RISCOS_CFLAGS}" CXXFLAGS="${CXXFLAGS:-$RISCOS_CFLAGS}"
export LDFLAGS="${LDFLAGS:--static}"
export RISCOS_GTK_CFLAGS="$RISCOS_CFLAGS $(pkg-config --cflags gtk+-3.0)"
# (pkg-config repeats -L and -lz -lm many times over: keep one -L, and
# drop a library named twice running)
export RISCOS_GTK_LIBS="-static $(pkg-config --libs --static gtk+-3.0 | tr ' ' '\n' |
  awk 'NF && !($1 ~ /^-L/ && seenL[$1]++) && $1 != last { printf "%s ", $1; last = $1 }')"

# OpenGL for GtkGLArea: riscos-mesa's EGL and OSMesa, linked only into
# programs that ask (GDK refers to EGL weakly; -u pulls libEGL in). Full
# paths, not -L; in the .pc file written out in full as -Wl, because
# pkg-config puts the devkit's sysroot in front of a -L, a bare path or
# a path variable.
: "${RISCOS_MESA_DEVKIT:=${RISCOS_DEVKIT:-}}"
if [ -n "$RISCOS_MESA_DEVKIT" ] && [ -f "$RISCOS_MESA_DEVKIT/lib/libEGL.a" ] &&
   [ -f "$RISCOS_MESA_DEVKIT/lib/libOSMesa.a" ]; then
  RISCOS_MESA_DEVKIT=$(cd "$RISCOS_MESA_DEVKIT" && pwd)
  export RISCOS_MESA_DEVKIT
  export RISCOS_GTK_GL_LIBS="-Wl,-u,eglGetProcAddress $RISCOS_MESA_DEVKIT/lib/libEGL.a $RISCOS_MESA_DEVKIT/lib/libOSMesa.a -lstdc++"
  cat > "$RISCOS_GTK_PREFIX/lib/pkgconfig/riscos-gtk-gl.pc" <<EOT
# Written by the riscos-gtk devkit's env.sh: OpenGL for GtkGLArea
Name: riscos-gtk-gl
Description: OpenGL for GtkGLArea on RISC OS (riscos-mesa's EGL and OSMesa)
Version: $(sed -n 's/^riscos-mesa devkit //p' "$RISCOS_MESA_DEVKIT/VERSION" 2>/dev/null)
Requires: gtk+-3.0 epoxy
Libs: -Wl,-u,eglGetProcAddress -Wl,$RISCOS_MESA_DEVKIT/lib/libEGL.a -Wl,$RISCOS_MESA_DEVKIT/lib/libOSMesa.a -lstdc++
EOT
else
  unset RISCOS_GTK_GL_LIBS
  rm -f "$RISCOS_GTK_PREFIX/lib/pkgconfig/riscos-gtk-gl.pc"
fi
