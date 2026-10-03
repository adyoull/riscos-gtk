# Porting notes: UnixLib and the libraries

Facts found while building, so nobody has to find them twice.

## UnixLib 5.0.3.1 as GLib sees it
- **Stubs are marked**: `statfs`, `fstatfs`, `link`, `lchown` exist in
  libunixlib.a but are declared `__stub_*` in features.h, so meson's
  `has_function` correctly says no. GLib then reports no filesystem
  size/free space and no hard links.
- **Declared but missing**: `getservbyname_r` is in `<netdb.h>` but not in
  the library. GLib checked it with `compiles`; patched to `links`.
- **No mount table**: GIO's `gunixmounts.c` needs one; patched to report no
  Unix mounts on RISC OS. The file chooser will list discs itself (M4).
- **No O_NOFOLLOW** (no symbolic links): defined as 0 for gtrashportal.c.
- **-pthread**: GCCSDK's driver rejects it (threads are in UnixLib's C
  library). meson adds it for `dependency('threads')` and GLib's .pc files
  carry it: build-deps.sh wraps the compilers to drop it and strips it from
  the installed .pc files.
- **eventfd** exists (Lee Noar's, 2020) and works with select()/poll(), so
  GLib's GWakeup uses it. Its counter isn't updated atomically; with
  ticker-switched threads on one core a write racing a read could in
  theory lose a wake-up. Watch for main-loop stalls in threaded tests.
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

## UnixLib header bugs (reported: RiscOS/UnixLib/handoffs/2026-10-03-gtk-unixlib-headers.md)
- **`LLONG_MIN` is `0x8000000000000000LL`**, which is `unsigned long long`:
  positive, so C++ rejects it as narrowing (harfbuzz) and C comparisons
  against it go wrong. `riscos/fixincludes/limits.h` redefines it; env.sh
  puts that directory first with `-isystem`.
- `<dlfcn.h>` declares dlopen and friends (no RTLD_NOLOAD) but nothing
  implements them. `riscos/dl` is a libdl whose dlopen always fails, built
  after GLib (so gmodule still has no module loading) for libepoxy.

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
1. `meson.build`: getservbyname_r checked with `links`.
2. `gio/gunixmounts.c`: a RISC OS branch with no mounts.
3. `gio/gtrashportal.c`: O_NOFOLLOW fallback.
4. `gio/glocalfile.c`: two upstream bugs that only show on a system with
   neither statfs nor statvfs (an unguarded `statfs_buffer` use, and
   `fstype` read uninitialised).
All four are generic enough to offer upstream.

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
