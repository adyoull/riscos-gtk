# Porting a GTK program to RISC OS

A GTK 3 program usually builds with the devkit as it is and runs as a
Wimp task. This page lists what is different on RISC OS, why, and what
to do about it. riscos-gtk's own ports, L3afpad and Claws Mail, went
through all of it; their RISC OS code is in riscos-gtk's `apps/`
folder.

## The steps

1. **Configure for the cross-compiler.** Source `env.sh`, then:
   - autotools:
     `./configure --host=arm-riscos-gnueabihf --disable-shared`
   - meson: `--cross-file "$(riscos-gtk-meson-cross)"`
   - CMake: the devkit's toolchain file

   With autotools, `--disable-shared --enable-static` too. pkg-config
   is already set to give static libraries (`--static`).
   Switch off what can't work: D-Bus, plug-ins and spell checkers that
   load modules, and anything that runs helper programs. A program
   with OpenGL needs riscos-mesa's devkit too (API.md, "OpenGL").
2. **Add the start-up call:** `riscos_gtk_app_init ("Name", &argc, &argv)`
   at the top of `main()` (see API.md). If the program uses
   `GtkApplication`, give it `G_APPLICATION_NON_UNIQUE`.
3. **Link it.** Link statically, `-static` plus
   `pkg-config --libs --static gtk+-3.0`. Run riscos-crossdev's checks:
   `$RCD_DIR/checks/check-all.sh prog.elf`.
4. **Package it:** `riscos-gtk-package Name prog.elf 1.00 app out`, with
   `!Run`, `!Boot`, `!Help` and sprites in `app/`, copied from the
   template.
5. **Make it a RISC OS program** (optional, but it's what users expect):
   - the icon bar handlers and Info box;
   - the file types it opens;
   - start on the icon bar without a window;
   - closing the window hides it after asking to save.

   See "The icon bar" in API.md, and L3afpad's `riscos-l3afpad.c`.
6. **Test it on the machine.** If it stops, read the Log
   (`<Wimp$ScrapDir>.Name.Log`). Run with `*Set RISCOS_GTK_DEBUG 1`
   for more detail.

## What UnixLib can and can't do

UnixLib is GCCSDK's C library: POSIX on top of RISC OS.

- **No concurrent processes.** `fork()` is a vfork, good only for an
  immediate `exec()`, and the parent waits until the child ends.
  - **`g_spawn_async*()` starts a RISC OS task instead** (once GTK is
    running): with the output read through pipes, the command runs in a
    TaskWindow and its output comes back as it's written; without, it is
    started with Wimp_StartTask. Either way it runs alongside the
    program, and child watches see it end. The command must be one RISC
    OS understands (a `*` command, a program, an Obey file); Unix file
    names in it are given as RISC OS names. See API.md, "Running other
    programs". `g_spawn_sync()` still runs the program in place.
  - **Code that forks a worker** (for a name lookup, say) must do the
    work in place, or in a thread, and hand the answer back from an
    idle.
  - **To start another desktop program,** use Wimp_StartTask.
  - **To open a web address,** use `riscos_gtk_open_uri()`.
- **Threads are user-space,** switched by PThreadTicker on one core.
  `GThread`, `GTask`, `GThreadPool` and GIO's async file operations
  work.
  - **Socket waits:** a thread blocked in `select()` lets the others
    run, but a blocking `recv()` is a plain SWI and stops them all. Keep
    sockets non-blocking.
- **Never block the main thread.** riscos-gtk's main loop waits in
  Wimp_PollIdle so the desktop keeps going.
  - Don't call `sleep()`, or `poll()` or `select()` with a timeout, in
    the main thread. Use `g_timeout_add`, `g_io_add_watch` and GIO's
    async calls.
  - riscos-gtk checks GLib's file descriptors every 2 cs while it
    waits, so a watched socket works.
- **No D-Bus**, so none of these: `GDBus` session services,
  `GApplication` uniqueness, dconf, portals, gvfs, `GNotification`, the
  session manager or accessibility (ATK has no bridge). riscos-gtk's
  icon bar code keeps one copy of a program running.
- **No dynamic loading.** Everything is linked statically and `dlopen`
  isn't there. So:
  - **GModule plug-ins** must be linked in; Claws Mail's LiteHTML viewer
    is.
  - **GtkBuilder can't look up `my_widget_get_type`** by name. Call
    `g_type_ensure (MY_TYPE_WIDGET)` for each of your own types used in
    a `.ui` file before loading it.
  - **`gtk_builder_connect_signals()` can't find handlers by name.**
    Register them with `gtk_builder_add_callback_symbol()`.
- **No Unix domain sockets, no shared memory between programs, no ptys,
  no inotify.** `GFileMonitor` polls.
- **No symbolic or hard links.** `link()` fails: copy instead.
- **Network:** sockets and DNS work. GIO's `GTlsConnection` has no
  backend (no glib-networking).
  - For TLS, use AcornSSL (RISC OS 5.28 and later) on the socket's RISC
    OS handle (`__get_ro_socket`), as riscos-gtk's Claws Mail does.
  - Decrypted data waits inside AcornSSL, so "is there input?" must ask
    AcornSSL too (a 1-byte `MSG_PEEK`).
- **iconv** is UnixLib's front end to the Iconv module, from PackMan.
- **Memory:** the program runs in its WimpSlot, which is its size plus
  4 MB. The C heap grows in dynamic areas and can go past 128 MB.

## Code that RISC OS's processors reject

- **Unaligned accesses abort.** RISC OS doesn't fix up an unaligned
  load or store. GCCSDK generates aligned code, but casts such as
  `*(uint32_t *) (buf + 1)` and packed structs still produce unaligned
  accesses. Run `$RCD_DIR/checks/check-unaligned.sh` (the template's
  `make check-unaligned`) and use `memcpy` for unaligned data.
- **Stack frames over 4 KB need probes.** ARMEABISupport grows the stack
  a page at a time. `-fstack-clash-protection` is required: it's in
  `RISCOS_CFLAGS`, so keep it if you set your own CFLAGS.
  `check-all.sh` finds functions without probes.
- **VFPv3 only.** `-mfpu=vfpv3` runs on every ARMv7 RISC OS machine.
  `check-all.sh` finds VFPv4 instructions.

## Files and names

- **GTK sees UnixLib names**: `/ADFS::HD4/$/Docs/Report.txt` for
  `ADFS::HD4.$.Docs.Report/txt`. UnixLib swaps `.` and `/` between the
  two. Show names to users in RISC OS form with
  `gdk_riscos_path_from_unix()`.
- **RISC OS names are case-insensitive.** Don't ship two files whose
  names differ only in case, as some icon themes and data sets do.
- **File types, not extensions,** are what RISC OS goes by. riscos-gtk's
  Save box sets the type from the extension. A program that saves one
  kind of file without an extension should set
  `RISCOS_GTK_DEFAULT_FILETYPE`.
- **Settings and data:** `riscos_gtk_app_init` points GLib's user
  directories at `<Choices$Write>.Name`, so `g_get_user_config_dir()`
  and GKeyFile settings files go there. GSettings are kept in memory
  only (no file monitor for GLib's keyfile backend), so keep anything
  that must last in a GKeyFile. A program with its own `~/.prog`
  directory should use `g_get_user_config_dir()` instead of `$HOME`.
- **GSettings schemas:**
  - **Your own:** pass your `.gschema.xml` files to
    `riscos-gtk-package` in `SCHEMAS`. Without its schema, GLib stops
    the program when it asks for it.
  - **GTK's own** (its file, colour and emoji choosers) are always
    there. If a program ships none, riscos-gtk writes its built-in copy
    to `<Wimp$ScrapDir>.riscos-gtk-schemas` at start-up and tells GLib.
    Before that, a program packaged without schemas stopped on every
    Open and Save.
- **Writing a file twice:** RISC OS allows one write-open of a file at a
  time. Don't `freopen()` one file for both stdout and stderr: open it
  once and `dup2()`, as `riscos_gtk_app_init` does.

## Pictures, fonts and the look

- **Images:** gdk-pixbuf reads PNG, JPEG, GIF, BMP, ICO, TGA, XPM and
  PNM, but **not SVG** (no librsvg) **or TIFF**. Icon themes must
  be PNG. riscos-gtk ships Adwaita's PNG icons for the names GTK uses.
  Give your program's own icons as PNGs, plus `@2` versions for 180 dpi
  modes.
- **Fonts:**
  - **With PackMan's FontConfig (UnixFC) installed,** programs get the
    system's fonts and shared font cache, plus their own `fonts`
    folder.
  - **Without it,** they get only their own `fonts` folder, which has
    DejaVu Sans by default.
  - **More faces:** add them with `EXTRA_FONTS` when packaging
    (`DejaVuSansMono.ttf` for a monospaced view), or put other TrueType
    or OpenType files in `fonts`.
  - **RISC OS's own outline fonts** can't be used (API.md, "Fonts").
- **The theme** is riscos-gtk's RISCOS theme. A program's own CSS works,
  but CSS that names Adwaita's colours, or hard-codes sizes for a
  different theme, may look out of place.
- **Header bars:** dialogues use ordinary title bars, because
  riscos-gtk turns off `gtk-dialogs-use-header`. A window that sets a
  `GtkHeaderBar` as its own title bar hasn't been tried with riscos-gtk.
  Prefer `gtk_window_set_title()`, with the header bar's buttons in a
  toolbar.
- **OpenGL:** `GtkGLArea` works through riscos-mesa (OpenGL 2.1, GLSL
  1.20, rendered on the processor) when the program is linked with
  `$RISCOS_GTK_GL_LIBS`. See "OpenGL" in API.md. Code written for GL
  3.2 core needs GLSL 1.20 shaders, and must check
  (`epoxy_has_gl_extension`) for anything newer than 2.1 it uses. A program that renders a lot is better served by riscos-mesa's
  SDL2 or EGL directly.
- **Translations:** messages are in English only. riscos-gtk's libintl
  is a stand-in that returns the original strings.

## Menus and keys

- **The menu bar** becomes a Wimp menu on Menu, and isn't drawn. Menu
  items with mnemonics and accelerators work, and accelerators are
  shown as RISC OS shows them.
- **Context menus** of the widget under the pointer come first in the
  window menu. A program that pops up its own menu on button 3 should
  do it on "button-press-event" with
  `gdk_event_triggers_context_menu()`. Menu is button 3 to GTK.
- **Menu always gives a menu.** A widget that uses button 3 for
  something else (a paint program's canvas) still gets the click, and
  then the window's menu opens. Such a program should leave button 3
  alone on RISC OS and use Adjust (GDK button 2) for what the right
  button does elsewhere: Select, Adjust, Menu are buttons 1, 2, 3.
- **Quit:** RISC OS programs quit from the icon bar menu.
  riscos-gtk's icon bar finds a menu item labelled Quit, or the
  application's "quit" action.

## Notes from ports

**Geany** (2.0, and 2.1: riscos-gtk's `!Geany` demo, `build/build-geany.sh`)
builds with autotools from the devkit:

```sh
./configure --host=arm-riscos-gnueabihf --disable-shared --enable-static \
  --disable-plugins --disable-nls --disable-vte --disable-socket \
  CPPFLAGS=-DHAVE_SCANDIR
make
```

It also passes riscos-crossdev's checks. Things that came up:

- **scandir:** ctags' fallback `scandir` copies `d_reclen`, which
  UnixLib's `struct dirent` hasn't got. UnixLib has a real `scandir`, so
  `-DHAVE_SCANDIR` uses that.
- **libtool and the toolchain's `.la` files:** they name the machine
  riscos-crossdev was built on, so libtool's link stops with
  "cannot find the library '/opt/b14/.../libm.la'". `env.sh` prints a
  `sed` command that corrects the paths. Or link without libtool:
  `build-geany.sh` lets make compile libgeany's objects, archives them
  with `ar` and links them with its own `main()`.
- **Static pkg-config:** without `--static`, the link misses expat,
  epoxy and the rest. The devkit's pkg-config adds it.
- **Its data:** Geany looks for its data (filetypes, colour schemes,
  templates) in the configured prefix. On RISC OS it belongs in the
  application: a one-hunk patch makes it `/<Geany$Dir>/share`
  (UnixLib reads the system variable in the name).
- **Its icons:** a program's own icons in `share/icons/hicolor` need the
  folders listed in hicolor's `index.theme`, or GTK won't look there.
- **The result:** saving, the Open box, the Save box and the directory
  browser all work on the fake desktop (riscos-gtk's `geany-fake`).

**mtPaint** (3.50.14: riscos-gtk's `!mtPaint`, `build/build-mtpaint.sh`)
has its own configure script:

- **pkg-config:** it asks for each library on its own, so give it a
  wrapper that adds `--static` (`PKG_CONFIG=/path/to/wrapper`), or the
  link misses libepoxy and the rest.
- **sysctl:** its memory-size code wants `<sys/sysctl.h>`, which UnixLib
  hasn't got; on RISC OS it reads OS_ReadMemMapInfo.
- **Its own file picker:** mtPaint doesn't use GTK's file chooser, so
  riscos-gtk's Save and Open boxes can't stand in for it. The port gives
  it a RISC OS one: a draggable file icon (gdk_riscos_save_drag_begin,
  gdk_riscos_save_drop, gdk_riscos_save_done), files dropped on it from
  the Filer (gdk_riscos_window_set_load_func), a full RISC OS name, and
  Browse... for `GtkFileChooserNative`. A program with a picker of its
  own needs the same.
- **Buttons:** it paints with the right button; on RISC OS that's Adjust,
  and it leaves Menu to riscos-gtk.

## Licences

The devkit's libraries are LGPL 2.1 and statically linked. When you
distribute a program:

- include `LicenceLGPL` and `Source` (`riscos-gtk-package` adds both);
- make your program's source, or its object files, available, so that
  users can relink it with changed libraries. Set `PROGRAM_SOURCE_URL`
  when packaging, and `Source` will say where.

A GPL program, like L3afpad or Claws Mail, must publish its complete
source as usual.
