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

## First RISC OS run (!GLibTest 0.1, 2026-10-03, a Raspberry Pi 4, SDFS)
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

## Confirm every client resize with a configure event (2026-10-04)
GTK 3 freezes a mapped toplevel's updates after it asks for a new size
and thaws them on the next configure event. Broadway's server always
replies; ours now does too (window_move_resize). Without it a window that
grew after opening (the theme test, after its status line was set) never
drew. Also: set the screen resolution in the backend
(gdk_screen_set_resolution); GTK applies gtk-xft-dpi itself only when the
application sets it.

## Debugging trap: relative FONTCONFIG_FILE in the QEMU tests
A relative path left fontconfig with no configuration; Pango then gives
zero metrics and GTK sizes labels tens of thousands of pixels tall. Always
pass the app folder as an absolute path (as !Run does).

## Wimp menus and GTK (2026-10-04)
- Message_MenusDeleted comes only when a menu closes without a choice.
- Adjust: the Wimp closes the tree on a choice; call Wimp_CreateMenu with
  the same block before the next Wimp_Poll to reopen it as it was. GTK
  code mustn't run inside the GLib poll function, so the choice is handled
  in an idle callback and the backend skips Wimp_Poll (menu_hold) until
  gdk_riscos_display_menu_done().
- GtkComboBox selects its active item after popping up its menu, which
  activates the menu shell and adds a GTK grab on the (never shown) menu:
  events then reach unrealised widgets (Gtk-CRITICAL). The patch skips the
  grab in gtk_menu_shell_activate for a menu open as a Wimp menu.
- Combo box items are GtkCellViews, not labels: their text comes from the
  model row (first string column).

## Implicit grab and menu bar double pop-up (2026-10-05)
- X gives the window a button went down in all events until release; the
  Wimp doesn't. Without doing the same in the backend, a release outside
  our windows is lost and GDK's own implicit grab sends later presses to
  the old widget.
- GtkMenuBar pops its menu up on the press and again on the release: keep
  the Wimp menu open when the same GtkMenu is popped up again, and never
  free a menu block straight after replacing or closing it (the Wimp acts
  on Wimp_CreateMenu at its next poll).

## Selections, the clipboard and drag and drop (2026-10-05)
- Broadway's selection code was stubs, so a GtkEntry's menu never opened:
  it waits for the clipboard's TARGETS before popping up (30 s timeout).
  convert_selection must always answer with a GDK_SELECTION_NOTIFY.
- Without a GDK-managed drag (gtk_drag_is_managed: X11, Wayland, Win32),
  gtk_drag_begin_internal's GTK grab cancels the source widget's gestures
  and GtkEntry's trigger event is freed while still in use (a crash in
  gdk_event_get_screen); patched to copy the event.
- GTK's drag update runs in an idle (priority GDK_PRIORITY_REDRAW + 5) and
  needs motion events as they happen. The backend's poll function used to
  keep polling the Wimp after a null event that queued motion, so GDK
  compressed a whole drag into its last motion; now it returns at once.
- Wimp_DragBox's block layout isn't in the 2026 PRM text (only the SWI
  page); the classic layout is used: +0 window, +4 type (5 = fixed size
  box), +8 box x0 y0 x1 y1, +24 parent box.
- RISCOS_GTK_DEBUG traces the clipboard and DnD messages
  ("riscos-gtk transfer:", "riscos-gtk dnd:").

## GSettings schemas for the file choosers (2026-10-05)
GtkFileChooserNative always builds a GtkFileChooserDialog (its fallback),
and setting its action to Save reads org.gtk.Settings.FileChooser. With no
compiled schemas GLib stops the program ("No GSettings schemas are
installed"). Apps ship share/glib-2.0/schemas/gschemas.compiled (compiled
from GTK's schema XML by build-app.sh) next to share/themes, found through
XDG_DATA_DIRS. Settings live in GLib's memory backend (nothing is kept).
The fallback dialog's places list also asked for an icon theme once
("Could not find the icon 'user-home-symbolic-ltr'"). Fixed in 0.23 by
shipping icon themes (share/icons/RISCOS and an empty hicolor). Note: a
theme set without image-missing makes GTK abort (gtkiconhelper.c asserts)
the first time an icon is missing, so RISCOS must keep image-missing
(mkicons.py refuses to build without it).

## Icons (2026-10-06)
gdk-pixbuf on RISC OS has no SVG loader, so icon themes must be PNG:
Adwaita 3.36 still ships full-colour PNGs (later versions are SVG and
symbolic only), and its symbolic SVGs are made into GTK's .symbolic.png
at build time with the host's gtk-encode-symbolic-svg (libgtk-3-bin).
At run time gtkriscostheme.c adds RISCOS-Wimp (the desktop's sprites) in
the theme's temporary directory and sets gtk-icon-theme-name to it.

## GtkBuilder and static linking (2026-10-06)
GtkBuilder resolves a type name it hasn't seen registered by looking up
`name_get_type` in the program with GModule (dlsym). A statically linked
RISC OS program can't, so a .ui file naming a type from outside GTK fails
("Unknown type PangoFontFamily specified in treemodel model") and the
widget can crash (the font chooser did). GTK's own types are covered by
gtk_test_register_all_types(); the others builder files use are listed in
the gtkbuilder.c patch. A program with its own .ui files naming its own
types must call g_type_ensure() on them first, and
gtk_builder_connect_signals() can't find handlers by name either (use
gtk_builder_add_callback_symbol()).

## The icon bar for a ported program (2026-10-06)
Every program gets an icon (sprite `!`+program name in the Wimp pool, so
the application's !Boot should IconSprites its !Sprites; else
"application") with a menu of Info, Choices... (only when it has
settings) and Quit, as Reel and !FFmpeg have. Without handlers, Select
presents the program's windows (or activates its GtkApplication),
Choices.../Quit use the GtkApplication's "preferences"/"quit" actions or
menu items labelled Preferences/Settings/Choices/Options, Quit; files from
the Filer go to g_application_open() if the application HANDLES_OPEN,
else an error box says it can't. For a proper RISC OS program, before
gtk_init():
- gdk_riscos_iconbar_set_handlers() with show / open_file / choices /
  quit / unsaved (see apps/l3afpad/riscos-l3afpad.c; Quit and PreQuit ask
  about unsaved work with a Cancel / Discard box before calling quit);
- gdk_riscos_iconbar_set_start_file() with the file from the command line
  (a second copy hands it to the running one);
- gdk_riscos_iconbar_set_info() for the Info box;
- gdk_riscos_iconbar_add_filetype() for each type it opens on a
  double-click;
- make the window's delete-event hide it (after asking to save), so the
  program stays on the icon bar; start without a window unless given a
  file.

## Printing (2026-10-06)
GtkPrintOperation works as it is: gtk_print_operation_run (PRINT_DIALOG,
PRINT or PREVIEW) shows the RISC OS Print box, prints through !Printers'
printer, and previews in riscos-gtk's own window (whose Print button
activates the program's menu item labelled "Print"). A program built with
printing switched off for want of print back ends (L3afpad's ENABLE_PRINT)
can switch it on. GtkPageSetupUnixDialog (Page Setup) isn't replaced: the
paper comes from the printer driver.

## Network programs (Claws Mail, 2026-10-07)
- **fork()**: UnixLib's is a vfork, for exec. Code that forks a child to
  do work (Claws Mail's name lookups: the child writes the answer to a
  pipe) must do the work in place instead and hand the answer over from
  an idle.
- **Sockets in the main loop**: riscos-gtk's poll function looks at
  GLib's fds every 2 cs while it waits in Wimp_PollIdle, so
  g_io_add_watch on a socket works. A blocking select() on a socket
  doesn't let the desktop go: keep sockets non-blocking.
- **TLS**: AcornSSL (RISC OS 5.28+) on the socket's RISC OS handle
  (`__get_ro_socket`). Decrypted data waits inside AcornSSL, not in the
  socket, so "is there input" must also ask AcornSSL (a 1-byte
  MSG_PEEK), as Claws Mail's GSource does through
  gnutls_record_check_pending.
- **No Unix domain sockets**: a program that keeps one copy through one
  should leave that to the icon bar (gdk_riscos_iconbar_set_start_file).
- UnixLib 5.0.3.3 has fgets_unlocked but not fputs_unlocked; it has no
  AI_ADDRCONFIG. Hard links (link()) aren't a RISC OS idea: expect them
  to fail (Claws Mail copies instead).
- **Threads and sockets**: UnixLib's select() yields to other threads
  while it waits, so a worker thread doing network I/O (Claws Mail's
  IMAP) keeps the desktop going. A blocking recv() on a socket is a plain
  SWI and doesn't: keep sockets non-blocking and wait in select().
- **libtool and the toolchain's .la files**: crossdev 1.3's .la files
  (libm, libstdc++, libunixlib...) name the machine they were built on;
  libtool reads them when it links with the C++ driver. Link C libraries
  as C.
