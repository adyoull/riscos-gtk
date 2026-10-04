# Changelog

## M1 done (2026-10-04)
- GTKTest 0.5 on the owner's Pi (UnixLib 5.0.3.2-rc2): glibtest 29/29,
  rendertest 22/22 (DejaVu Sans through fontconfig, harfbuzz kerning, cairo,
  pixman, gdk-pixbuf). First text layout and draw, font loading included:
  127 ms. Results and the picture in tests/glib/results/.
- fonts.conf escapes < > & (RISC OS paths); rendertest takes the fonts.conf
  as an argument (no *Set: it GSTrans'd the path).

## Unreleased (UnixLib 5.0.3.2-rc2, 2026-10-03)
- Built against riscos-unixlib 5.0.3.2-rc2 (`make toolchain-unixlib`:
  crossdev 1.2's toolchain plus the release's libunixlib.a and limits.h).
- Dropped the workarounds UnixLib fixed: `riscos/fixincludes/limits.h`
  (LLONG_MIN) and GLib's getservbyname_r meson change.
- !GTKTest 0.3: the same tests linked with UnixLib 5.0.3.2-rc2.

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
