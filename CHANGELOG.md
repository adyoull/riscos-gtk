# Changelog

## Unreleased (M1: all dependencies, 2026-10-03)
- libpng 1.6.37, libjpeg-turbo 2.1.5, FreeType 2.10.1, expat 2.2.9,
  fontconfig 2.14.1 (Warzone's patch), fribidi 1.0.13, harfbuzz 8.3.0,
  pixman 0.42.2, cairo 1.18.0, pango 1.52.1, gdk-pixbuf 2.42.10 (all loaders
  built in; patch for three bugs), ATK 2.36.0, libepoxy 1.5.10 (patch).
- `riscos/fixincludes/limits.h` (UnixLib's LLONG_MIN), `riscos/dl` (libdl).
- `TARGET=linuxarm`: the whole stack again as ARM Linux code; build-linuxarm.sh
  runs every test under qemu-arm-aligntrap.
- `tests/render/rendertest.c` (21 checks): fontconfig, pango (harfbuzz
  kerning, fribidi), cairo/pixman, gdk-pixbuf PNG/JPEG. Passes under QEMU.
- !GTKTest 0.2 replaces !GLibTest: both tests, DejaVu Sans, fonts.conf.
- build-deps.sh stops on the first failure (set -e didn't see failures
  inside "x || { ... }" blocks).

## Unreleased (M0 + M1 started, 2026-10-03)
- Repo created. Theme revision 2 imported as `theme/` (from
  RISCOS-gtk3-theme.zip, sha256 9b32281c…d11); font changed to DejaVu Sans.
- Build: riscos-crossdev 1.2 toolchain (GCC 10.2, UnixLib 5.0.3.1); build
  scripts adapted from riscos-warzone2100; static libraries installed into
  `sysroot/riscos-gtk` with no build paths compiled in.
- M1: zlib 1.3.1, libffi 3.4.6, PCRE2 10.42, a minimal libintl, GLib 2.80.0
  (four small patches) cross-built for RISC OS.
- `tests/glib/glibtest.c` (29 checks): passes under qemu-arm-aligntrap
  (arm-linux build of the same GLib); !GLibTest 0.1 built for the Pi,
  passes riscos-crossdev's checks; on the owner's Pi: 29/29.
- `tests/fake/`: riscos-mesa's fake Wimp/screen and event harnesses, for
  the GDK backend's host tests.
