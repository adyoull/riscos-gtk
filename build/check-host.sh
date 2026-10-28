#!/bin/bash
# check-host.sh [--missing] [check]: the build machine's tools riscos-gtk
# needs, and the Ubuntu 24.04 package for each one missing. With "check",
# also those for the Linux (QEMU) tests. --missing: just print the missing
# packages' names. MIT licence.
list=
[ "${1:-}" = --missing ] && { list=1; shift; }
missing=
need() {  # TEST PACKAGE
  eval "$1" > /dev/null 2>&1 || missing="$missing $2"
}
need 'command -v meson' meson
need 'command -v ninja' ninja-build
need 'command -v pkg-config' pkg-config
need 'command -v automake' automake
need 'test -f /usr/share/aclocal/ax_check_compile_flag.m4' autoconf-archive
need 'command -v libtoolize' libtool
need 'command -v cmake' cmake
need 'command -v gperf' gperf
need 'command -v curl' curl
need 'command -v unzip' unzip
need 'command -v xz' xz-utils
need 'command -v zstd' zstd
need 'command -v glib-compile-schemas' libglib2.0-bin
need 'command -v glib-compile-resources' libglib2.0-dev-bin
need 'command -v update-mime-database' shared-mime-info
need 'command -v gdk-pixbuf-pixdata' libgdk-pixbuf2.0-bin
need 'python3 -c "import packaging"' python3-packaging
need 'python3 -c "import PIL"' python3-pil
need 'command -v xsltproc' xsltproc
need 'test -f /usr/share/xml/docbook/stylesheet/docbook-xsl/html/chunk.xsl' docbook-xsl
if [ "${1:-}" = check ]; then
  need 'command -v arm-linux-gnueabihf-gcc' gcc-arm-linux-gnueabihf
  need 'command -v arm-linux-gnueabihf-g++' g++-arm-linux-gnueabihf
fi
if [ -n "$list" ]; then
  echo $missing
  exit 0
fi
if [ -n "$missing" ]; then
  echo "check-host.sh: this machine lacks what riscos-gtk's build uses. On Ubuntu 24.04:" >&2
  echo "  sudo apt-get install$missing" >&2
  exit 1
fi
