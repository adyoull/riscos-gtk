# Porting notes: UnixLib and GLib

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

## The GLib patch (patches/glib-2.80.0-riscos.diff)
1. `meson.build`: getservbyname_r checked with `links`.
2. `gio/gunixmounts.c`: a RISC OS branch with no mounts.
3. `gio/gtrashportal.c`: O_NOFOLLOW fallback.
4. `gio/glocalfile.c`: two upstream bugs that only show on a system with
   neither statfs nor statvfs (an unguarded `statfs_buffer` use, and
   `fstype` read uninitialised).
All four are generic enough to offer upstream.

## Still to check on RISC OS (the !GLibTest run)
- GLib's default poll() in a command-line program, and timing: GLib's
  monotonic clock comes from UnixLib's clock_gettime (high resolution
  since riscos-unixlib 5.0.x).
- Threads: GThreadPool workers, GCond timed waits, g_main_context_invoke
  from a worker (eventfd wake-up).
- GIO's async read on its worker pool.
- Filename and locale character sets (`# locale charset=... filename
  charset=...` in the output). RISC OS filenames are usually Latin-1 and
  GLib assumes UTF-8 unless G_FILENAME_ENCODING says otherwise: decide in
  M4 (likely `G_FILENAME_ENCODING=@locale` or ISO-8859-1 from !Run).
