#!/bin/bash -e
# Build the same GLib (same sources, same patches) for arm-linux-gnueabihf
# with the code generation RISC OS gets (-mno-unaligned-access, Cortex-A8,
# VFPv3), static libraries linked into a -no-pie program, so that
# riscos-crossdev's qemu-arm-aligntrap checks GLib's own code for unaligned
# accesses and VFPv4 instructions (glibc itself is exempt). This is the
# "layer 4" test from riscos-crossdev: it can't see UnixLib or the Wimp.
#
#   build/build-linuxarm.sh            # libraries into $LXSTAGE, then tests
#   build/build-linuxarm.sh tests      # tests only
. "$(dirname "$0")/env.sh"
unset CC CXX AR RANLIB STRIP CFLAGS CXXFLAGS CPPFLAGS LDFLAGS PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR
: "${LXSTAGE:=$REPO_DIR/stage-linuxarm}"
: "${LXSRC:=$REPO_DIR/src-linuxarm}"
LX=arm-linux-gnueabihf
LXFLAGS="-O2 -g -mcpu=cortex-a8 -mfpu=vfpv3 -mfloat-abi=hard -mno-unaligned-access -fstack-clash-protection"
export PKG_CONFIG_LIBDIR="$LXSTAGE/lib/pkgconfig"
mkdir -p "$LXSTAGE/lib/pkgconfig" "$LXSRC"
have() { [ -f "$LXSTAGE/lib/$1" ] && echo "    (already built)"; }
step() { echo; echo "=== linux-arm: $*"; }

if [ "${1:-}" != tests ]; then
cd "$LXSRC"
step "zlib"
have libz.a || {
rm -rf zlib-*; tar xf "$DL"/zlib_1.3.dfsg+really1.3.1.orig.tar.gz; cd zlib-1.3.1
CC=$LX-gcc CFLAGS="$LXFLAGS" CHOST=$LX ./configure --static --prefix="$LXSTAGE" > ../zlib.log
make -j$JOBS libz.a >> ../zlib.log && make install >> ../zlib.log; cd ..; }

step "libffi"
have libffi.a || {
rm -rf libffi-*; tar xf "$DL"/libffi_3.4.6.orig.tar.gz; cd libffi-3.4.6
autoreconf -fi > ../ffi.log 2>&1
./configure --host=$LX --prefix="$LXSTAGE" --disable-shared --enable-static --disable-docs \
  --disable-multi-os-directory CFLAGS="$LXFLAGS" >> ../ffi.log
make -j$JOBS >> ../ffi.log 2>&1 && make install >> ../ffi.log; cd ..; }

step "pcre2"
have libpcre2-8.a || {
rm -rf pcre2-*; tar xf "$DL"/pcre2_10.42.orig.tar.gz; cd pcre2-10.42
./configure --host=$LX --prefix="$LXSTAGE" --disable-shared --enable-static --disable-jit \
  CFLAGS="$LXFLAGS" > ../pcre2.log
make -j$JOBS libpcre2-8.la >> ../pcre2.log
make install-libLTLIBRARIES install-includeHEADERS install-nodist_includeHEADERS install-pkgconfigDATA >> ../pcre2.log
cd ..; }

step "GLib 2.80.0 (patches/glib-2.80.0-riscos.diff)"
have libglib-2.0.a || {
rm -rf glib-2.80.0; tar xf "$DL"/glib2.0_2.80.0.orig.tar.xz
patch -d glib-2.80.0 -p1 -s < "$REPO_DIR"/patches/glib-2.80.0-riscos.diff
args=$(printf "'%s', " $LXFLAGS "-I$LXSTAGE/include")
cat > meson-cross-linuxarm.txt <<EOF
[binaries]
c = '$LX-gcc'
cpp = '$LX-g++'
ar = '$LX-ar'
strip = '$LX-strip'
pkg-config = 'pkg-config'
[properties]
pkg_config_libdir = '$LXSTAGE/lib/pkgconfig'
needs_exe_wrapper = true
[built-in options]
c_args = [${args%, }]
c_link_args = ['-L$LXSTAGE/lib']
default_library = 'static'
prefix = '$LXSTAGE'
[host_machine]
system = 'linux'
cpu_family = 'arm'
cpu = 'cortex-a8'
endian = 'little'
EOF
cd glib-2.80.0
meson setup _build --cross-file ../meson-cross-linuxarm.txt $GLIB_MESON_OPTS -Dlibmount=disabled > ../glib.log
ninja -C _build >> ../glib.log && ninja -C _build install >> ../glib.log; cd ..; }
fi

step "glibtest under qemu-arm-aligntrap"
mkdir -p "$REPO_DIR/out"
$LX-gcc $LXFLAGS -no-pie -Wall $(pkg-config --cflags gio-2.0) \
  "$REPO_DIR"/tests/glib/glibtest.c -o "$REPO_DIR"/out/glibtest-linuxarm \
  -Wl,-Bstatic $(pkg-config --libs --static gio-2.0 | sed 's/-pthread//g; s/-lresolv//g') \
  -Wl,-Bdynamic -lresolv -pthread
scratch=$(mktemp -d)
rc=0
GLIBTEST_SKIP_ICONV=1 QEMU=$GCCSDK_ENV/bin/qemu-arm-aligntrap "$RCD_DIR"/qemu/aligntrap.sh \
  "$REPO_DIR"/out/glibtest-linuxarm "$scratch" || rc=$?
rm -rf "$scratch"
exit $rc
