# Changelog

## Unreleased (GTKHello 0.4, 2026-10-04)
- Colours: on the Pi (TBGR desktop) red and blue were swapped:
  OS_SpriteOp doesn't convert a TRGB sprite when plotting on a TBGR screen.
  Backing sprites are now made in the screen's colour order; on TBGR
  screens cairo draws into its own buffer, copied into the sprite with R/B
  swapped before each plot. The log says which (with the sprite mode word).
- Toggle size / corner resize: GDK now takes sizes the Wimp started from
  the configure event (window size, new backing surface, relayout);
  Broadway never needed this.
- Redraws paint the part of a window the sprite doesn't cover in Wimp
  colour 1, so nothing stale shows or smears.
- Fake Wimp: `fake_sprite_plot_converts` (0 = the Pi's behaviour),
  Wimp_SetColour, OS_Plot rectangle fills. hellotest: colour order on a
  TBGR screen, toggle size relayout and painting (14/14; the old backend
  fails all three). dev-gdk.sh refreshes the fake on linuxarm.

## Unreleased (GTKHello 0.3, 2026-10-04)
- Fix: GTKHello 0.1/0.2 quit at once on the Pi with an empty log. hello.c
  freopen()ed the log for stdout and again for stderr; RISC OS allows one
  write-open per file, so the second failed and hello_main returned. Now
  the log is opened once (stderr) and dup2()ed onto stdout, and a failed
  log no longer stops the program.
- hellotest runs hello with a log as !Run does, linked with
  `-Wl,--wrap=freopen` emulating RISC OS's one-write-open rule; it checks
  the log has both streams (11/11; the old hello.c fails it).

## Unreleased (theme revision 3, 2026-10-04)
- Theme toward the RISC OS 5 default desktop (owner's choice: fonts and
  sizes; buttons and fields; scroll bars, menus, lists): Style Guide heights
  in OS units (buttons/writables 52 = 26px, menu entries and option rows 44
  = 22px), R6 default-button channel, Wimp-style scroll bars (colour 3 well,
  colour 1 slider, raised arrows, 20px), Wimp menus (1px black outline,
  22px entries), 23px list rows, radio icons lit from the top left, all 16
  Wimp colours. See theme/README.md.
- The backend returns the desktop's GTK settings (`gdkscreen-riscos.c`):
  RISCOS theme, DejaVu Sans 12 at 90 dpi (Homerton.Medium 12pt), no
  animations, no overlay scroll bars or dialog header bars. hello.c no
  longer sets them; hellotest checks them (10/10).
- GTKHello 0.2 (the new theme and defaults); theme/shoot.sh renders with
  the backend's settings in a private HOME.

## Unreleased (M2 started, 2026-10-04)
- GTK 3.24.41 cross-builds with a new `riscos` GDK backend
  (`patches/gtk+3.0-3.24.41-riscos.diff`: meson option, backend
  registration; print backends allowed to be empty; IM modules built in).
- `gdk/riscos/`: GTK's Broadway client code renamed, with
  `gdkriscos-server.c` implementing Broadway's server interface on the
  Wimp: Wimp windows with native furniture, TRGB sprite backing stores
  drawn into by cairo directly, GLib's poll function waiting in
  Wimp_PollIdle, clicks/releases/motion/crossings from Mouse_Click and
  pointer polling, keys with synthesised releases, caret focus, titles in
  Latin-1, open requests as configure events, close as delete.
- `tests/gtk/hellotest.c`: GTK + the backend on the fake Wimp under
  qemu-arm-aligntrap: the window draws in the RISCOS theme, a click on the
  button fires "clicked", typing reaches the entry, the check box toggles,
  the title arrives in Latin-1 (9/9).
- !GTKHello 0.1 for the Pi (`make hello`).
- `build/dev-gdk.sh`: incremental rebuild of the backend.
- riscos-crossdev 1.3 (UnixLib 5.0.3.2, static-only toolchain): the
  UnixLib overlay toolchain (`make toolchain-unixlib`) and the wrapper's
  `-static` are gone.

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
