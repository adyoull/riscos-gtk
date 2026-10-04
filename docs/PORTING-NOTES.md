# Porting notes: UnixLib and the libraries

Facts found while building, so nobody has to find them twice.

## UnixLib 5.0.3.1 as GLib sees it
- **Stubs are marked**: `statfs`, `fstatfs`, `link`, `lchown` exist in
  libunixlib.a but are declared `__stub_*` in features.h, so meson's
  `has_function` correctly says no. GLib then reports no filesystem
  size/free space and no hard links.
- `getservbyname_r` was declared but missing before 5.0.3.2 (see below).
- **No mount table**: GIO's `gunixmounts.c` needs one; patched to report no
  Unix mounts on RISC OS. The file chooser will list discs itself (M4).
- **No O_NOFOLLOW** (no symbolic links): defined as 0 for gtrashportal.c.
- **-pthread**: GCCSDK's driver rejects it (threads are in UnixLib's C
  library). meson adds it for `dependency('threads')` and GLib's .pc files
  carry it: build-deps.sh wraps the compilers to drop it and strips it from
  the installed .pc files.
- **eventfd** exists (Lee Noar's, 2020) and works with select()/poll(), so
  GLib's GWakeup uses it (thread-safe since 5.0.3.2, see below).
- **iconv** is UnixLib's front end to the Iconv module (PackMan); UnixLib
  RMEnsures `System:Modules.Iconv` 0.04 itself on the first iconv_open.
- **No libintl**: `riscos/intl` provides the gettext functions (English
  only). Real translations later need GNU gettext's libintl or catalogues
  read by our own code.
- **dlopen**: the header exists, the library doesn't; GLib's gmodule
  builds without module loading (fine: everything is static).
- Not used / not available, so GLib's fallbacks are used: inotify (file
  monitors poll), pipe2, posix_spawn, statvfs, getmntent, close_range,
  getauxval, futex.

## UnixLib 5.0.3.2 and riscos-crossdev 1.3
riscos-gtk builds with **riscos-crossdev 1.3**: UnixLib 5.0.3.2 and a
static-only toolchain. Both came out of riscos-gtk's handoffs
(`RiscOS/UnixLib/handoffs/2026-10-03-gtk-headers-reply.md`,
`RiscOS/Crossdev-public/handoffs/2026-10-04-unixlib-gtk-crossdev-1.3-ready.md`):
- `LLONG_MIN` was `0x8000000000000000LL` (unsigned, positive): harfbuzz
  didn't compile. Fixed in 5.0.3.2; env.sh refuses an older toolchain.
- `getservbyname_r` (and `_byport_r`, `getservent_r`) now exist, so GLib's
  compile-only check is right.
- eventfd's counter is updated with thread switching held off, and a
  blocking eventfd read no longer stops with "pthread_yield called with
  context switching disabled". GLib opens its wake-up eventfd non-blocking.
- crossdev 1.2's toolchain had shared libraries (libunixlib.so, libm.so,
  libstdc++.so, .la files): any link without `-static` (GLib's tools,
  configure tests, and libtool, which drops -static and follows libm.la)
  used the old shared UnixLib. 1.3 has no shared libraries, its driver adds
  `-static`, and its `check-all.sh` includes `check-static.sh`.
  (Interim, 2026-10-03/04: a copy of the 1.2 toolchain with the rc2 library
  dropped in, made static-only, and a wrapper adding -static; both gone.)
- `dlopen` and friends: not UnixLib's (GCCSDK's libdl comes with the shared
  library loader, not in this toolchain). Keep `riscos/dl`. RTLD_NOLOAD
  won't be added, so libepoxy's fallback (our patch) stays.

## The other libraries
- **meson and static libraries**: the cross file sets `prefer_static = true`
  so pkg-config is asked for `--static` and private dependencies (expat
  under fontconfig, libpng under cairo) get linked.
- **gdk-pixbuf** (`patches/gdk-pixbuf-2.42.10-riscos.diff`), all loaders
  built in (`-Dbuiltin_loaders=all`): (1) the built-in loaders' own
  libraries (libpng, libjpeg) weren't passed on to programs or the .pc file;
  (2) with no GModule, `gdk_pixbuf_io_init()` passed an uninitialised
  pointer to `gdk_pixbuf_io_init_modules()` and `g_free()`d it; (3) with
  GModule but no loaders.cache it reported failure, so every call to
  `gdk_pixbuf_get_formats()` registered the built-in loaders again (found
  by rendertest: 12, 24, 36… formats).
- **libepoxy** (`patches/libepoxy-1.5.10-riscos.diff`): RTLD_NOLOAD
  fallback. Built without GLX/EGL: GtkGLArea has no backend yet.
- **pixman**: plain C. pixman-arm.c has no RISC OS CPU detection, so its
  ARMv6 SIMD/NEON paths aren't built. Adding detection (every RISC OS 5
  machine is ARMv7; NEON on the Pi) is a later speed-up worth testing under
  qemu-arm-aligntrap first.
- **libjpeg-turbo**: C only (`-DWITH_SIMD=OFF`), same reason.
- **fontconfig**: riscos-warzone2100's patch (`;` as the path-list
  separator, touch stack buffers before read()). The read() part has been
  unnecessary since UnixLib 5.0.3 (read() touches stack pages itself) but is
  harmless.

## The GLib patch (patches/glib-2.80.0-riscos.diff)
1. `gio/gunixmounts.c`: a RISC OS branch with no mounts.
2. `gio/gtrashportal.c`: O_NOFOLLOW fallback.
3. `gio/glocalfile.c`: two upstream bugs that only show on a system with
   neither statfs nor statvfs (an unguarded `statfs_buffer` use, and
   `fstype` read uninitialised).
All three are generic enough to offer upstream. (A fourth, checking
getservbyname_r with `links`, was dropped once UnixLib 5.0.3.2 added it.)

## First RISC OS run (!GLibTest 0.1, 2026-10-03, owner's Pi, SDFS)
**29/29 passed** (`tests/glib/results/GLibTest-0.1-pi-2026-10-03.txt`):
main loop, nested loop, GCond, GThreadPool with g_main_context_invoke back
to the main thread (eventfd wake-up), GIO sync and async (GTask worker
pool), GObject/libffi, GVariant, GRegex, iconv through the Iconv module,
GKeyFile, GDateTime, printf.
- 3 × 50 ms timeouts took 177 ms (151 under QEMU): fine for a UI.
- Home comes from !UnixHome (`/SDFS::ePic.$/Apps/Support/!UnixHome/home`),
  config dir `<home>/.config`; tmp is `/tmp` (UnixLib maps it to the
  scrap directory).
- **Locale charset is ISO-8859-1, filename charset UTF-8.** So GLib will
  treat RISC OS (Latin-1) filenames as UTF-8: names with accented letters
  would be invalid UTF-8 to GTK. Plan for M4: set
  `G_FILENAME_ENCODING=@locale` (or ISO-8859-1) in !Run, and check how
  UnixLib's own path translation handles top-bit characters.

## GTK 3.24.41 (M2)
- The backend is GTK's Broadway backend, client side, renamed (`gdk/riscos/`).
  Broadway's GDK code talks to a separate server through ~25 functions and
  gets input back as messages; `gdkriscos-server.c` implements those
  functions on the Wimp in the same process. Broadway's ifdefs in gtk/
  (forced client-side decorations, clipboard workarounds) don't apply
  because the backend has its own name.
- Build-machine tools: glib-compile-resources, glib-mkenums,
  glib-genmarshal (Ubuntu's libglib2.0-dev-bin, 2.80) are named in the meson
  cross file — otherwise meson takes the sysroot's RISC OS binaries from
  glib's .pc file. gdk-pixbuf-pixdata (libgdk-pixbuf2.0-bin) must be on PATH.
- No dlopen: print backends (normally modules; GTK insists on 'file' on
  Unix — patched to allow none for riscos) and IM modules
  (`-Dbuiltin_immodules=all`).
- GDK passes a toplevel's title in the window attributes at creation
  (GDK_WA_TITLE); Broadway ignored it, the riscos backend sets it.
- Titles are converted UTF-8 → Latin-1 without iconv (characters beyond
  Latin-1 become '?').
- The QEMU test build puts the fake RISC OS (tests/fake/mesa-egl) into
  libgdk, so GTK's own tools and the tests link on arm-linux.

## One write-open per file (2026-10-04)
RISC OS refuses to open a file for writing while it is already open for
writing. Never freopen() one log file for both stdout and stderr: open it
once and dup2() the descriptor (Warzone 2100 and Freeciv hit the same
bug). hellotest emulates the rule with `-Wl,--wrap=freopen`.

## Sprites are plotted without colour-order conversion (2026-10-04)
On a Pi 4 with RISC OS 5 and a TBGR (normal) desktop, a 32bpp TRGB sprite
plotted with OS_SpriteOp 34 shows red and blue swapped: no conversion.
Make sprites in the screen's colour order (ModeFlags from
OS_ReadVduVariables), swapping channels yourself if the renderer can't
produce that order. riscos-mesa's fake converted, so tests passed.

## The Wimp resizes windows, not the client (2026-10-04)
Broadway's client side only expects echoes of its own move/resize, so its
configure handler ignored the size. On RISC OS the user resizes with the
window furniture: the configure handler sets the size, updates GDK and
makes a new backing surface (as X11 does). Paint all of every redraw
rectangle: a window with no work-area colour shows stale screen elsewhere.
