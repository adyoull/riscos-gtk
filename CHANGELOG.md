# Changelog

## Since RC7
- **Font choosers: Serif and Monospace looked the same as Sans.** In
  Gnumeric's Format Cells > Font, choosing Serif or Monospace changed the
  preview's size but not its face: every package had only DejaVu Sans, so
  fontconfig drew every family with it. Every package now has DejaVu
  Serif and DejaVu Sans Mono too (regular, bold and italic), and its
  fonts.conf makes them serif and monospace (and Serif, Monospace,
  system-ui, the names font choosers list), so they look different in
  every program. `DEJAVU_FACES` in `build/env.sh`,
  `tests/render/mkfontsconf.sh`, the devkit's `riscos-gtk-package`;
  L3afpad's and Geany's own `EXTRA_FONTS` for Sans Mono are no longer
  needed. Each package is about 1 MB bigger. Tests: `gnumeric-fake`
  picks Sans, Serif and Monospace in GOffice's font chooser and checks
  the preview's face; `fontconfig-fake` checks what each name gives.
  Gnumeric 1.12.57-riscos8 (test build).

## 3.24.41-riscos1.0-rc7 (2026-10-18)
- **Gnumeric is part of the release:** `Gnumeric-1.12.57-riscos7.zip`, a
  direct download beside the other programs (`make-release.sh` builds it;
  `GNUMERIC_VERSION`). riscos7 is riscos6 with its !Help no longer calling
  it a test build. GTK, GLib and the icons changed since RC6, so every
  program was rebuilt: L3afpad 0.8.18.1.11-riscos26, ClawsMail
  4.4.0-riscos16, Geany 2.1-riscos5, mtPaint 3.50.14-riscos5, and the
  test programs 3.24.41-riscos1.0-rc7.
- **git in containers:** `build/env.sh` adds the repository and
  riscos-crossdev to git's `safe.directory` for the build's own git
  commands (through `GIT_CONFIG_COUNT`), so the devkit and the packages
  name their commit when the checkout belongs to another user (GitHub's
  container jobs stopped at `make sdk` with "dubious ownership", and
  `build-app.sh` wrote "unknown" as the commit).
- **File > Properties: Group showed rubbish (Gnumeric).** RISC OS has no
  /etc/group, and UnixLib's `getgrgid_r` (and `getpwuid_r`) then return
  -1 without setting the result pointer, which GLib read anyway. GLib now
  ignores the result when the call fails, so the group shows as its
  number, as on Unix for an unknown group; GOffice copes with a file that
  has no owner or group name (`patches/glib-2.80.0-riscos.diff`,
  `patches/goffice-0.10.57-riscos.diff`). Gnumeric 1.12.57-riscos6. UnixLib
  handoff: `getgrgid_r`/`getgrnam_r`/`getpwuid_r` should set `*result` to
  NULL on failure.
- **Builds on GitHub, with a pull request when riscos-crossdev releases.**
  `.riscos-deps` (`riscos-crossdev v1.8.2`) and the two workflows from
  riscos-crossdev's `ci/`: `make deps tests hello check sdk` in the
  riscos-crossdev image on every push, and a daily check that opens a
  pull request, built and tested, when there's a newer riscos-crossdev.
  The build now uses riscos-crossdev 1.8.2 (the same toolchain; devkit
  20.3.5-16). `build/check-host.sh` names any missing host package; GTK's
  host tools are no longer needed (the symbolic icons are drawn: 88 of
  them, including the emoji chooser's and the gestures). See
  docs/BUILDING.md. GOffice and Gnumeric download from Ubuntu's universe
  (their main URLs were wrong; the first GitHub run found it).
  libffi comes from its own release tarball, which has its configure
  script: Ubuntu's needed autoconf 2.71, and the image has 2.69. A failed
  library build now shows the end of its log.
- **RISC OS-style icons for every program.** The RISCOS icon theme now
  has riscos-gtk's own icons, drawn in the style of ClawsMail's, for all
  130 full-colour names it ships (folders, file types and discs too,
  for a desktop without those sprites) and 72 of the one-colour
  (symbolic) icons GTK's widgets use (arrows, the field clear button,
  window controls, the file chooser's places...; emoji and gesture
  pictures stay Adwaita's). Programs' own icons get the same treatment:
  Gnumeric's 100 (toolbars, borders, alignment, sheet objects) and
  Geany's 11 (symbol list, Build, Save all, Close all), drawn by
  `tools/riscosicons_apps.py` and put in their RISCOS theme by
  `build-app.sh` (`ICON_PACK`). This covers the names programs use for
  toolbars, menus and dialogue boxes
  (open, save, print, cut, copy, paste, undo, find, go, zoom, text
  formatting, media, warning and error...), at 16, 22, 24, 32 and 48
  pixels, in place of Adwaita's (`tools/mkriscosicons.py`). The drawing
  is shared with ClawsMail's icons (`tools/riscosdraw.py`, unchanged
  output for ClawsMail) and comes with the devkit for ports' own icons.
  Test `themetest-fake`.
- **The directory browser's Discs: the icon bar's on a real desktop.**
  On the Pi the list showed every filing system, as if there were no icon
  bar. The Filers are module tasks, so their icons' text is in the RMA,
  and it was read with Wimp_TransferBlock, which failed: every icon was
  skipped. A module task's text is now read directly (checked with
  OS_Memory 24); an application's still with Wimp_TransferBlock. If
  Wimp_WhichIcon can't list the icon bar, each icon handle is tried in
  turn. The Log gets one line saying what was read
  (`riscos-gtk: icon bar; SDFS Filer "RISCOSpi"; ...`). Test
  `browser-fake`: the Filers are module tasks, and the sidebar is clicked
  with the mouse (`gdk/riscos/gdkfiles-riscos.c`).
- **Names beyond ASCII in the Save and Open boxes and the browser.** A
  file name with a hard space or an accented letter (Latin-1) was put
  into GTK as it was: invalid UTF-8 (Pango warnings, wrong text). The
  boxes now convert RISC OS names to UTF-8 to show them and back to
  Latin-1 to use them, and the program's suggested name (UTF-8) to
  Latin-1 (`gtk/riscos/gtkfilechooser-riscos.c`; `browser-fake`).
- Gnumeric 1.12.57-riscos3 (test build) has both; riscos4 adds the icons, riscos5 its own icons and the symbolic ones.
- **Gnumeric 1.12.57-riscos2 (test build): File > Open works, and
  scrolling past a sheet's controls no longer stops it.** From the first
  Pi run (riscos1):
  - **Open and Save boxes took no clicks.** GOffice shows its file
    dialogue and then adds a grab to it; the dialogue is shown as a RISC
    OS box and stays hidden, so every event went to it. GTK now gives no
    grab to a dialogue shown as a box (`gtk/gtkmain.c`); the box has its
    own when it's modal. Affects any program that does the same.
  - **It stopped when the sheet scrolled** past a check box, button or
    list on it: GOffice draws them through an offscreen box that asks for
    a native child window, which the RISC OS GDK can't make (an
    assertion). GDK now keeps such windows drawn in their top-level
    (`gdk_window_ensure_native`, `gdk/gdkwindow.c`).
  - **A spreadsheet dropped on its window wasn't opened:** every file's
    type was `application/octet-stream`, as no program carried the MIME
    database. Every application now has `share/mime` (shared-mime-info
    2.4's names and magic, `build/mkmime.sh`), so GIO knows types from
    names, for all programs. Gnumeric also takes .xlsx, CSV and files of
    unknown type dropped on it.
  - **File names with characters beyond ASCII** (a hard space) showed as
    invalid text: GLib now takes RISC OS file names as Latin-1, not
    UTF-8 (`g_get_filename_charsets`; `G_FILENAME_ENCODING` overrides),
    and GOffice converts a file's name for display.
  - `g_module_open (NULL)` (the program itself, as GTK asks) gives a
    module with no symbols instead of NULL, so no criticals at start.
  - Test `gnumeric-fake`: 12 checks (scrolling with a check box, file
    types, the grab).
- **Gnumeric 1.12.57-riscos1 (new), a test build.** The spreadsheet, with
  GOffice 0.10.57 for its charts and libgsf 1.14.51, libxml2 2.9.14 and
  libxslt 1.1.39 for its files (`build/build-gnumeric.sh`, `make gnumeric`).
  - Its plugins (functions, file formats, chart types: 34 of Gnumeric's,
    GOffice's 9) are linked in and found through GLib's new static
    module registry, below.
  - Gnumeric's and GOffice's files come from `/<Gnumeric$Dir>` and
    `/<GOffice$Dir>` (`patches/gnumeric-1.12.57-riscos.diff`,
    `patches/goffice-0.10.57-riscos.diff`); settings in
    `<Choices$Write>.Gnumeric`; the icon bar, Choices, CSV files (&DFE).
  - SVG pictures aren't shown: librsvg is written in Rust, so a stand-in
    (`riscos/rsvg-stub`) says it can't load them.
  - Test `gnumeric-fake` (9 checks): formulas from three function
    plugins, saving .ods and .xlsx, reopening the .xlsx, a bar chart plot,
    quitting.
- **GModule plugins can be linked into a program.** RISC OS has no
  `dlopen`, so `g_module_open()` failed and programs that load plugins
  lost them. GLib now has a registry of linked-in modules:
  `g_module_register_static (name, symbols)`; `g_module_open()` of a path
  ending in that name (with or without `.so`) opens it, and
  `g_module_symbol()` finds its symbols. `tools/mkstaticmodules.py` does
  the rest: it renames each plugin's symbols so that several can define
  `go_plugin_init`, archives them and writes the registration function
  (`patches/glib-2.80.0-riscos.diff`; test `staticmodule-test`, 10
  checks).
- **Icons a program adds from its resources are found.**
  `gtk_icon_theme_add_resource_path()` icons (Gnumeric's toolbar) are
  looked for as part of hicolor, in the folders hicolor's `index.theme`
  lists; the packaged theme listed only the program's own, so they were
  missing. `build/mkicons.py` now lists hicolor's standard folders too.
- **ClawsMail 4.4.0-riscos15: files dragged to a message window are
  attached or inserted.** Both failed ("Could not get size of the file",
  "File ///RAM::RamDisc0/$/... doesn't exist"): Claws Mail took the name
  from a `file:///` URI by removing only `file:`, and UnixLib can't open a
  name starting `///`. It now drops the URI's `//` and host
  (`patches/claws-mail-4.4.0-riscos.diff`, `src/common/utils.c`); test in
  `claws-fake`.
- **File names in UnixLib's own form.** `gdk_riscos_path_to_unix` and
  `gdk_riscos_path_from_unix` use UnixLib's `__unixify_std` and
  `__riscosify_std` on RISC OS, so a dropped or opened file is named as
  UnixLib names it (`/RAM::RamDisc0.$/Textfile.txt`, not
  `/RAM::RamDisc0/$/Textfile.txt`; `c.hello` is `hello.c`), and `/tmp` is
  the scrap directory. The tests' fake RISC OS keeps the simple swap.

## 3.24.41-riscos1.0-rc6 (2026-10-11)
Programs: L3afpad 0.8.18.1.11-riscos25, ClawsMail 4.4.0-riscos14, Geany
2.1-riscos4, mtPaint 3.50.14-riscos4 (new), GTKThemeTest and GTKGLTest
3.24.41-riscos1.0-rc6.

- **Menu gives the program's menu anywhere in its windows.** Before, a
  widget that used button 3 itself (a paint program's canvas, a toolbar,
  an insensitive button) swallowed the click, so Menu only worked over
  parts of a window (reported with mtPaint).
  - The widget under the pointer still has the click first. If it opens
    a menu of its own, that menu is the window menu's first entry, as
    before; if not, the window's menu opens all the same.
  - Tool windows with no menu of their own (not dialogue boxes) give the
    menu of the window they belong to, or of the program's only window
    with a menu.
  - `gtk/riscos/gtkmenu-riscos.c`; test `menuclick-fake` (7 checks).
- **Programs started with `g_spawn_async*()` run as RISC OS tasks,**
  alongside the program. Before, UnixLib ran them in place of the
  program until they ended, and their output couldn't be read as it came.
  - With the output read through pipes, the command runs in a TaskWindow
    with the program as its parent: its output reaches the pipe as it
    comes, the working directory is set, and `Sys$ReturnCode` is the exit
    status. Without, it is started with Wimp_StartTask. Child watches see
    it end.
  - Unix file names in the command are given as RISC OS names.
  - `gdk_riscos_spawn_kill()` ends one. `RISCOS_GTK_SPAWN=exec` turns it
    off; `RISCOS_GTK_TASK_WIMPSLOT` sets a TaskWindow's memory.
  - `gdk/riscos/gdkspawn-riscos.c`; GLib: spawn and child watch hooks
    (`patches/glib-2.80.0-riscos.diff`); test `spawn-fake` (13 checks).
- **The directory browser's Discs are the icon bar's.** It listed every
  filing system FileSwitch knows (devices without discs, filing systems
  with nothing mounted) and opened `<FS>:$`, which is no use for network
  filing systems with several mounts (LanMan98 couldn't be opened).
  - Now it lists the discs and mounts the Filers have on the icon bar,
    left to right, with their names and sprites (RISCOSpi, RAMDisc0,
    Apps, a LanMan98 mount...), and opens each one's own root
    (`LanMan98::Server.$`; a floppy drive `ADFS::0.$`).
  - `gdk_riscos_discs()` (`gdk/riscos/gdkfiles-riscos.c`). Without an icon
    bar to read, each filing system whose current disc can be found.
    `RISCOS_GTK_DISCS=all` uses that list always. Test `browser-fake`.
- **The DDE's throwback for spawned builds.** While a TaskWindow started
  by `g_spawn_async*()` runs, the program is registered with DDEUtils,
  so `cc`, `objasm`, `link` and `amu` given `-throwback` send it their
  errors. They are written to the task's output as GCC-style lines with
  Unix file names (`c.hello` as `hello.c`), so Geany's Compiler tab lists
  them and goes to the line. `RISCOS_GTK_THROWBACK=0` turns it off. Test
  `spawn-fake` (17 checks).
- **!Geany 2.1-riscos4 (riscos2 and riscos3 were test builds): Build and Execute work.** Compile, Build, Make and
  the other Build commands run in a TaskWindow, with their messages
  (errors linked to the lines) in Geany's Compiler tab; Stop ends them.
  Execute runs the program in a task window of its own, from an Obey file
  that sets the directory and shows its return code (`./prog` runs
  `prog`). The commands must be ones RISC OS has (GCC and make from
  PackMan for C). Settings from older versions that name xterm get the
  task window instead. Test `geany-fake` (11 checks).
- **Windows keep their title bar on the screen.** A program that puts its
  window at the top of the screen (`gtk_window_move (w, 0, 0)`: GTK means
  the frame's corner) had the title bar off the top, so the window
  couldn't be moved or closed (mtPaint's main window). The window is now
  moved down to show it, and GTK is told; maximised windows fill the
  screen below their title bar.
  - `gdk/riscos/gdkriscos-server.c` (Wimp_GetWindowOutline after opening),
    `gdk/riscos/gdkwindow-riscos.c`; test `menuclick-fake` (14 checks).
- **Pop-up windows with a title get a title bar and a close icon.**
  Programs open some windows as GTK pop-ups (no frame), which on RISC OS
  could only be closed with Escape or a click elsewhere (mtPaint's Pan
  Window and Pattern Chooser).
  - A pop-up with a title (`GTK_WINDOW_POPUP`, a normal window, not a
    menu, tooltip or drop-down) is now an ordinary Wimp window with a
    title bar and close icon. Pop-ups with no title stay plain.
  - Its close icon sends it Escape, as the program expects; if it is
    still open a moment later, it is sent delete-event.
  - `*Set RISCOS_GTK_PLAIN_POPUPS 1` keeps them plain.
  - `gdk/riscos/gdkwindow-riscos.c`, `gdk/riscos/gdkeventsource.c`;
    tests `menuclick-fake`, `mtpaint-fake` (12 checks).
- **!mtPaint 3.50.14-riscos4, a new port** (riscos1 to riscos3 were test builds): mtPaint, the painting
  program (`build/build-mtpaint.sh`, `apps/mtpaint`,
  `patches/mtpaint-3.50.14-riscos.diff`).
  - Its own file picker becomes a RISC OS one: a draggable file icon for
    saving, files dropped on it from the Filer, a full RISC OS name, and
    Browse... for riscos-gtk's Save and Open boxes and directory browser;
    mtPaint's format options stay under it.
  - Adjust does what the right button does elsewhere (colour B; a tool
    button's settings); Menu is the menu.
  - The icon bar (Info, Choices... for its Preferences, Quit; PNG, JPEG,
    GIF and BMP files), the Log, settings in `<Choices$Write>.mtPaint`.
  - Its toolbar icons (XPM) load: riscos-gtk's gdk-pixbuf has the XPM
    loader built in. A build against a gdk-pixbuf that loads loaders as
    modules can't, in a static RISC OS program.
  - Test `mtpaint-fake` (12 checks; needs MTPAINT_LA).

## 3.24.41-riscos1.0-rc5 (2026-10-11)
- **A directory browser for Save and Open, like a Filer window.**
  - `*Set RISCOS_GTK_CHOOSER filer` shows it instead of the Save and Open
    boxes, for one program (in its !Run) or every program. The Save and
    Open boxes have a Browse... button that switches to it (Menu over a
    box offers "Directory browser..." too), and its own menu offers the
    box back. The Save box is a little wider for the button, on the
    Selection line.
  - The window: the folder's objects with their Wimp sprites, as large
    icons, small icons or full info (type names, sizes, dates), sorted
    by name, type, size or date (Menu > Display, as the Filer's). Parent
    and the folder's full name along the top (type one and Return).
  - A sidebar like the Finder's: Discs (every filing system that holds
    directories, with its icon bar sprite), Places (the program's
    folder, Boot, Choices, Scrap) and Recent (the last ten folders used).
  - Double-click a directory to go in, a file to open it. A directory
    dragged in from the Filer is shown; a file dragged in is chosen. The
    program's file filters are offered (file types matched to MIME
    types). Save keeps the file icon, which can still be dragged to a
    Filer window. Choosing a folder and choosing several files work.
    New directory... in Save.
  - The view, sort and recent folders are kept in
    `<Choices$Write>.GTK.Browser`.
  - New GDK calls for it (`gdk/riscos/gdkfiles-riscos.c`):
    `gdk_riscos_read_dir` (OS_GBPB 12), `gdk_riscos_filing_systems`
    (OS_FSControl 33), `gdk_riscos_canonicalise` (37) and
    `gdk_riscos_filetype_name` (18).
  - Test: `browser-fake` (27 checks). The fake RISC OS answers OS_GBPB 12
    and OS_FSControl 33 and 18.
- **A program's file dialogue gets its folder.** GTK's hidden file
  chooser widget sets the folder a program gives it only once it has read
  it, so a dialogue shown at once (Geany's Open) started nowhere.
  `gtk_file_chooser_set_current_folder_file` now keeps the folder for
  the box and the browser (patches/gtk+3.0-3.24.41-riscos.diff).
- **Stock button labels:** a program's dialogue buttons made from stock
  items (Geany's) gave the box "gtk-open" and "gtk-cancel". The stock
  labels are used.
- **!Geany 2.1-riscos1, a demo:** Geany 2.1, the programmer's editor, built with
  its own autotools build against riscos-gtk (`build/build-geany.sh`).
  - Changed only where it finds its data (`/<Geany$Dir>/share`,
    patches/geany-2.1-riscos.diff); `apps/geany/riscos-geany.c` adds the
    icon bar (Info, Choices... for its Preferences, Quit; text, HTML,
    XML, CSS, JavaScript, JSON and CSV files), the Log and RISC OS names.
  - Settings in `<Choices$Write>.geany`. No plugins, terminal or
    Execute.
  - Test: `geany-fake` runs Geany 2.1 on the fake desktop, with the box
    and with the browser (6 checks each), when a Geany build for the
    test target is given (GEANY_LA).
- **Packaging:** a program's own icons in `share/icons/hicolor` are
  listed in hicolor's index.theme, so GTK finds them (Geany's toolbar
  and symbol icons). `build-app.sh` packages a program built by its own
  build (PREBUILT).
- **Versions: the program's own version and `-riscosN`,** as riscos-gtk's
  other ports number theirs, with N carrying on from the old 0.x count:
  !L3afpad 0.8.18.1.11-riscos24 and !ClawsMail 4.4.0-riscos13 (their Save
  and Open dialogues get the folder fix and the directory browser),
  !Geany 2.1-riscos1 (new). The test programs, with no upstream version,
  take the release's: GTKThemeTest and !GTKGLTest 3.24.41-riscos1.0-rc5.
  The Info box shows the version as it is. The release now has GTKGLTest
  and Geany.

## Since RC4 (2026-10-11)
- **GTK's GSettings schemas for programs that ship none.**
  - **The report:** Geany, built by GCCSDK's autobuilder against
    riscos-gtk, crashed on every Open and Save
    (Crossdev-public/handoffs/2026-10-11-gtk-geany-load-save-crash.md).
  - **The cause:** the package had no GSettings schemas. GTK's file
    chooser asks for org.gtk.Settings.FileChooser when it's made, and
    GLib stops the program when no schema is installed. riscos-gtk's own
    packages ship them, so they never hit it.
  - **The fix:** riscos-gtk carries GTK's compiled schemas
    (`gdk/riscos/gdkschemas-riscos.h`, made by
    `tools/mkgtkschemas.sh`). `gdkschemas-riscos.c` checks at start-up
    whether GLib will find GTK's schema. If not, it writes the copy to
    `<Wimp$ScrapDir>.riscos-gtk-schemas` and puts that folder first in
    GSETTINGS_SCHEMA_DIR. The Log says so.
  - **Test:** new `schemas-fake` covers both cases.
  - **Geany 2.0 on the fake desktop:** built for ARM Linux against the
    fake RISC OS, by hand, not in `make check`. Starting with a file,
    saving it, Open... with a file dropped on the Open box, and Save
    as... with the Save box's icon dragged to a Filer all work. Without
    schemas it stopped at Open before the fix, and it works after.
- **Devkit:**
  - pkg-config is a wrapper that adds `--static`, so autotools programs
    link GTK's dependencies.
  - `env.sh` prints a `sed` fix when the toolchain's `.la` files name a
    missing build path, which libtool's C++ links trip on.
  - PORTING.md has notes from Geany: `-DHAVE_SCANDIR` for its ctags.
- !L3afpad 0.23, !ClawsMail 0.12, GTKThemeTest 0.47 and !GTKGLTest 0.3, with the system fontconfig set-up.
- **Fonts: the system's fontconfig set-up when it's installed.**
  - With PackMan's FontConfig (UnixFC) installed, GTK programs use its
    fonts.conf: the system's fonts in UnixFont:, its mapping and its
    shared font cache. Their own fonts folder is added as application
    fonts.
  - Otherwise, or if it doesn't load, they use their own fonts.conf and
    a cache in scrap, as before.
  - `gdk/riscos/gdkfontconfig-riscos.c` does this when the display
    opens, after riscos-warzone2100's `riscos_fontconfig.c`.
  - `RISCOS_GTK_FONTS=own` opts out. The Log says which set-up was
    used.
  - New test `fontconfig-fake` covers both cases.
- **Devkit `app.h`:** it no longer sets `GSETTINGS_BACKEND=keyfile`.
  GLib's keyfile backend needs a file monitor RISC OS hasn't got (seen
  on the Pi in October). That is why the backend uses memory.
- !L3afpad 0.22, !ClawsMail 0.11, GTKThemeTest 0.46 and !GTKGLTest
  0.2. These are built with riscos-crossdev 1.5, and their !Run files
  now make the font cache folder.
- The RISC OS build maps SRC and SYSROOT out of `__FILE__` as well as
  the repository. A build with them outside the repository leaked
  `/tmp/...` through GLib's `gobjectnotifyqueue.c`, and build-app.sh's
  check caught it.
- **The font cache folder:** each program's fonts.conf keeps
  fontconfig's cache in `<Wimp$ScrapDir>.<Program>.fontcache`, but only
  `<Wimp$ScrapDir>.<Program>` was made. That left fontconfig to create
  the cache folder itself, and a report says L3afpad has trouble there.
  - Every !Run (L3afpad, ClawsMail, GTKThemeTest, GTKGLTest, and the
    devkit's template and example) now makes the folder with `CDir`.
  - The devkit's `riscos_gtk_app_init` makes it as well.
- **riscos-crossdev 1.5** (toolchain 1.4: UnixLib 5.0.3.3 built from
  its release's diff; riscos-mesa devkit 20.3.5-15).
  - riscos-gtk now builds with it, and `build/unixlib-overlay.sh`, the
    stop-gap that put 5.0.3.3 into crossdev 1.3, is gone.
  - `env.sh` recognises 5.0.3.3 by `__pthread_held_wait`, since 1.4's
    library isn't byte for byte the release's. A RISC OS build with an
    older UnixLib now stops.
  - The devkit's `env.sh` checks the same way and asks for crossdev
    1.5.
  - The GL test and !GTKGLTest use crossdev's own devkit
    (`make devkit`) by default; 20.3.5-15 is enough.
  - Docs: BUILDING, README, the devkit README, RELEASE-NOTES, DECISIONS
    and PORTING-NOTES.
  - All libraries rebuilt from clean with it; tests below.
- **OpenGL: GtkGLArea through riscos-mesa.**
  - **What it uses:** riscos-mesa's EGL and OSMesa (devkit 20.3.5-16):
    OpenGL 2.1 with GLSL 1.20, or ES 2.0 with `GDK_GL=gles`.
  - **`gdk/riscos/gdkglcontext-riscos.c`:** each GdkGLContext is an
    EGL context on a shared 1x1 pbuffer, and contexts share with the
    window's paint context. GTK's request for 3.2 core becomes a 2.1
    "legacy" context, as GTK does elsewhere without core profiles. EGL
    is declared here and referenced weakly, so GTK builds without
    riscos-mesa's headers.
  - **GTK patch (`gdkwindow.c`):** RISC OS windows are never painted
    with GL. `gdk_cairo_draw_from_gl()` takes GTK's software path,
    reading the area's pixels back into the backing sprite: one copy of
    the area a frame.
  - **libepoxy patch:** GL functions come from a weak
    `eglGetProcAddress` instead of dlopen.
  - **Linking:** a program that doesn't link
    `-Wl,-u,eglGetProcAddress -lEGL -lOSMesa` has no Mesa in it, and
    GtkGLArea reports "OpenGL is not available: this program wasn't
    linked with riscos-mesa's EGL and OSMesa".
  - **New program:** !GTKGLTest 0.1 (`tests/gtk/gltest.c`,
    `build/build-gltest.sh`, `make gltest`). `build-app.sh` takes
    EXTRA_CFLAGS and EXTRA_LIBS.
  - **Devkit:**
    - `env.sh` sets `RISCOS_GTK_GL_LIBS` and writes pkg-config's
      `riscos-gtk-gl` when `RISCOS_MESA_DEVKIT` (default crossdev's
      RISCOS_DEVKIT) has riscos-mesa's devkit.
    - `riscos-gtk-package` adds `LicenceMesa` to programs with Mesa
      linked in.
    - New `examples/glarea/`: a GLSL triangle with a vertex buffer.
    - API and PORTING have an OpenGL section; `mktemplatesprites.py`
      takes a name and draws a GL icon.
  - **Tests:**
    - `gltest-fake` links riscos-mesa's real RISC OS libEGL.a and
      libOSMesa.a into the ARM Linux test, through
      `tests/fake/mesa-shim.c` (adapted from riscos-mesa's ARM checks)
      and objcopy-renamed errno and mutex symbols. The fake answers
      VFPSupport_Features.
    - It checks the context, the renderer, the background, the
      triangle, the way up (a white square top left) and a resize, at
      EIG 1 and EIG 0.
    - `gltest-nogl` checks a program without Mesa reports it and runs
      on.
    - Both run in `make check`; the Mesa one only with MESA_DEVKIT.
- **M5: the riscos-gtk devkit** (`sdk/`, `build/make-sdk.sh`,
  `make sdk`; `make release` now includes it). It lets other developers
  build GTK 3 programs for RISC OS with riscos-crossdev 1.3 (plus
  UnixLib 5.0.3.3) without building riscos-gtk:
  - **Libraries:** `sysroot/riscos-gtk`, with headers, static libraries
    (without debug information) and pkg-config files. It leaves out the
    libraries only ClawsMail uses.
  - **`env.sh`:** sets the compiler, pkg-config, `RISCOS_GTK_CFLAGS` and
    `RISCOS_GTK_LIBS`.
  - **Other build systems:** compiler wrappers that drop `-pthread`, a
    meson cross file generator and a CMake toolchain file.
  - **`riscos-gtk-package`:** makes the !App and its zip. It adds the
    theme, icons, schemas (with the program's own), fonts,
    PThreadTicker, the LGPL and a Source file, and runs crossdev's
    checks.
  - **`<riscos-gtk/app.h>`:**
    - start-up: fonts, theme and settings directories, the Log, and
      files from the Filer;
    - `riscos_gtk_open_uri()`: a web address through URI_Dispatch,
      starting Iris or NetSurf first.
  - **`template/`:** a small GtkApplication program, with a menu bar,
    Save and Open boxes, icon bar, unsaved-work question and a web link.
    It has 90 and 180 dpi sprites (`tools/mktemplatesprites.py`) and a
    Makefile.
  - **`docs/`:** README (setup, building, what a program must ship),
    API (every RISC OS function and environment variable), PORTING
    (UnixLib's limits, alignment, files, fonts, menus).
- **Tests:**
  - `template-fake` runs the template on the fake desktop at EIG 1 and
    EIG 0. It checks the file is loaded, the title, the icon bar sprite,
    the Say hello button, the Menu, and that the "quit" action ends the
    program. This is the first test of `GtkApplication` on riscos-gtk.
  - Built from the devkit alone in a clean directory, the template
    passes crossdev's checks with make, meson and CMake.

## 3.24.41-riscos1.0-rc4 — public beta (2026-10-10)
Support for EX0 EY0 (180 dpi) screen modes, with high-resolution icons;
this release is the public beta RC4.
- Release notes, README and RC test plan updated for RC4; test plan
  lines 1.5 to 1.7 for 180 dpi modes.
- **EX0 EY0 (180 dpi) screen modes.** GTK laid windows out in the
  screen's pixels, so in a 180 dpi mode (one OS unit a pixel) everything
  came out half its usual size. Now:
  - In an EX0 EY0 mode GDK's scale factor is 2. GTK lays out in 90 dpi
    pixels (two OS units each, as on the usual EX1 EY1 desktop) and draws
    at the screen's resolution, so text and widgets are their usual size
    and sharp. `RISCOS_GTK_SCALE=1` or `2` overrides; the Log says
    "GDK scale 2".
  - The server keeps the screen's own eigen factors for the backing
    sprites (`dev_xeig`, `dev_yeig`) apart from GDK's (`xeig`, `yeig`).
    Backing sprites are twice the pixels each way with cairo's device
    scale set. The scale is decided when the program starts; a later
    mode change keeps it, and windows stay the right size.
  - The desktop's sprites are made at GDK pixel size (the "22" tool
    sprites in a 180 dpi mode). The theme's CSS gives each a second
    picture at the screen's resolution through `-gtk-scaled()`: the
    180 dpi sprite where the desktop has one, otherwise the 90 dpi one
    doubled pixel for pixel, as the Wimp shows it. New
    `gdk_riscos_scale_factor`, `gdk_riscos_wimp_sprite_scaled` and
    `gdk_riscos_wimp_tool_sprite_scaled`.
  - The scroll bar strip's image is drawn at the window's scale; the
    monitor reports its scale and a 90 dpi physical size.
  - Not changed: the pointer shapes riscos-gtk draws itself (up to
    32 pixels) are half size in a 180 dpi mode.
- Tests: the fake screen at `FAKE_EIG=0` has twice the pixels each way,
  and the test coordinates and `fake_screen_pixel` are in GDK pixels. The
  whole suite passes at EIG 1 and at EIG 0. `make check` now also runs
  hellotest and themetest-fake at EIG 0. The fake's `OS_ReadModeVariable`
  returns a numbered mode's own eigen factors.
- !L3afpad 0.20, !ClawsMail 0.9, GTKThemeTest 0.44.
- **High-resolution icons for 180 dpi modes** (0.20 and 0.9 ran on the
  Pi):
  - **GTK:** a GdkPixbuf tagged `riscos-gtk-scale` 2 is shown by GtkImage
    and GtkCellRendererPixbuf at half its pixel size, so a picture with
    twice the pixels fills the screen's pixels at GDK scale 2. This is a
    small change to `gtkiconhelper.c` and `gtkcellrendererpixbuf.c`, under
    GDK_WINDOWING_RISCOS.
  - **ClawsMail:**
    - `tools/mkclawsicons.py` also draws every RISC OS icon at twice the
      size (`NAME@2.png`), and the logo at 272 pixels.
    - At scale 2, `riscos_claws_stock_hires` gives Claws Mail those
      pictures, tagged: the toolbar and the message list's marks. The
      folder icons are made from the desktop's 180 dpi or doubled
      sprites with `@2` emblems.
    - Claws Mail's own lists and icon overlays read a tagged pixbuf's
      size and paint it at scale (`riscos-pixbuf-scale.h`, included in
      gtkcmclist.c, gtkcmctree.c and stock_pixmap.c).
    - The About box's logo is sharp too.
  - **Application icons:** `!Sprites11` files (180 dpi: 68 x 68 and
    34 x 34, PRMs "Icon Sets") for !ClawsMail (drawn by mkclawsicons.py)
    and !L3afpad (from L3afpad's own SVG drawing, by
    `tools/mkl3afpadsprites.py`). `tools/png2sprite.py --dpi 180` makes
    180 dpi sprites.
  - claws-fake's icon checks expect the `@2` pictures, tagged, at GDK
    scale 2.
- !L3afpad 0.21, !ClawsMail 0.10, GTKThemeTest 0.45.

## 3.24.41-riscos1.0-rc3 — public beta (2026-10-09)
ClawsMail 0.8's functionality confirmed on the Pi, links opening in Iris
included; this release is the public beta RC3.
- !ClawsMail 0.8 (the two sections below). L3afpad 0.19 and GTKThemeTest
  0.43 are unchanged since RC2, rebuilt to name this release.
- Release notes: public beta status, a "Reporting problems" section, what
  has been confirmed on the Pi (stated as functionality confirmed, not
  quoted), and ClawsMail's HTML mail, links, icons and
  About box. The assumed review decisions are now stated as decided:
  - the overlay toolchain is documented;
  - GTKHello and GTKTest are left out;
  - the source is published on GitHub releases;
  - UnixLib's source is referred to, not included.
- RC test plan: lines 4.15 to 4.20 for ClawsMail 0.8; the lines
  confirmed on the Pi are marked "yes".

## ClawsMail 0.8 (2026-10-09)
Fixes from testing 0.7 on the Pi, and follow-ups:
- **The HTML view was on a grey background:** litehtml leaves an
  uncoloured page transparent, so the theme's grey showed through. Now
  the canvas is painted white first (`lh_widget::draw`), as in a browser.
- **The HTML view didn't show the headers the text view shows:**
  - The litehtml viewer replaces the text view, which is where Claws
    Mail prints the headers.
  - It now has a Claws Mail TextView above the HTML showing the
    message's headers exactly as the text view does: same fonts and
    colours, clickable addresses.
  - New `textview_show_message_headers`. It's hidden when Claws Mail's
    header pane is on.
- **Menu clicks didn't work in the HTML view:**
  - The viewer swallowed every Menu click. Away from a link it now lets
    the click through, so the window's menu opens.
  - On a link, its own menu (Open Link, Copy Link) is "Link >" in the
    window's menu.
  - Copy Link copies to the global clipboard. RISC OS has no PRIMARY
    selection.
- **Links open in !Iris or NetSurf (not through MimeMap):**
  - MimeMap maps MIME types to file types. Links go through the Acorn
    URI handler instead.
  - Claws Mail's `open_uri` calls `riscos_claws_open_uri`:
    - `mailto:` starts a message.
    - Anything else is sent with URI_Dispatch (&4E381), which a running
      browser claims.
    - If neither Iris nor NetSurf is running, Iris (Iris$Dir), else
      NetSurf (NetSurf$Dir), is started with Wimp_StartTask. The link is
      dispatched once it shows in the task list (polled every 0.5 s, for
      up to 30 s).
    - If nothing takes the link, an error box says so.
- **The About box said GnuTLS and showed the old logo:**
  - The About box's feature list says AcornSSL.
  - The logo, in the About box and the set-up wizard, is the new
    application icon drawn large (`riscos_claws_priv_pixbuf`;
    `claws_mail_logo*.png` and `claws_mail_icon*.png` in the RISC OS
    theme, drawn by mkclawsicons.py).
- Tests: claws-fake 36 checks (7 new):
  - headers above the HTML;
  - a white canvas;
  - Menu away from a link and on a link;
  - a link click starting Iris and dispatching the link (a faked
    URI_Dispatch, Wimp_StartTask and TaskManager);
  - the About box's AcornSSL line and logo.

## ClawsMail 0.7 (2026-10-09)
- **HTML mail** (chosen: litehtml with remote images):
  - Claws Mail's litehtml viewer plug-in is built and linked into the
    program; `riscos-claws.c` starts it (`riscos_claws_plugins`, called
    from main.c just before Claws Mail loads its plug-ins). UnixLib has
    no dlopen, so plug-ins are linked in.
  - gumbo 0.12.0 (the HTML parser litehtml uses) is built by
    `build-claws.sh` (Ubuntu's orig tarball; in SHA256SUMS).
  - Remote images: `apps/claws/litehtml/http.cpp` replaces the
    plug-in's libcurl loader. It uses plain sockets and, for https,
    Claws Mail's `ssl_init_socket`, so AcornSSL checks the certificate.
    It runs in the viewer's GTask thread, with no signals. Redirects,
    chunked bodies, a 16 MB limit and a 10 s connect timeout.
  - Remote content is still off by default (the plug-in's own
    privacy setting, Choices... > Plugins > LiteHTML).
  - New default on RISC OS: the HTML part of multipart/alternative
    messages is shown.
- **RISC OS icons** for the application itself and its toolbar (the GTK 3
  ones looked out of place):
  - Original icons drawn by `tools/mkclawsicons.py` in the style of the
    RISC OS 5 desktop: 57 toolbar icons, 22 message-list marks, the
    folder icons, and a new !ClawsMail application icon (34 x 34 and
    17 x 17).
  - They are a Claws Mail icon theme in the application,
    `share/claws-mail/themes/RISCOS`, listed in Choices... > Display >
    Themes. It is the default on RISC OS.
  - The folder list's icons are the desktop's own small_dir / small_diro
    sprites, so they match the Filer in any desktop theme, with an
    emblem for Inbox, Sent, Queue and Drafts. The list check boxes are
    the Wimp's option sprites. This is in `riscos_claws_stock_pixbuf`,
    called from a patched `stock_pixmap.c`.
  - The two new defaults (the icons and HTML parts) are set once, the
    first time 0.7 runs (`<Choices$Write>.ClawsMail.claws-mail.RISCOSDefaults`).
    A later choice in Preferences is kept.
- Tests: claws-fake 29 checks (8 new):
  - the theme is chosen, Get Mail's icon is the theme's PNG, and the
    inbox's icon is the desktop sprite with the emblem;
  - HTML: the HTML part is shown by the litehtml viewer, laid out (a CSS
    block measured on screen), with a remote image over http (through a
    redirect, sent chunked) and one over https (through the fake
    AcornSSL).

  mailserver.py gains an HTTP server. `claws-fake-claws.c` has the calls
  that need Claws Mail's headers.
- Packages carry LicenceLiteHTML and LicenceGumbo.

## 3.24.41-riscos1.0-rc2 (2026-10-09)
- **One menu per window.** Menu in ClawsMail gave a different menu
  depending on where it was clicked. Now the window's main menu opens,
  with the context menu as a submenu:
  - `gtk/riscos/gtkmenu-riscos.c`: a widget's context menu opened by a
    Menu click, in a window with a menu, opens the window's menu with the
    context menu as its first entry.
  - The entry's name comes from "riscos-gtk-context-name" on the widget
    or an ancestor, defaulting to "Selection".
  - `riscos-claws.c` names the message list "Message" and the folder list
    "Folder".
  - Text views still give the window's menu directly.
  - This is GTK-wide, so every program gets it.
- Tests:
  - claws-fake 21 checks (3 new): Menu over the message list, over the
    folder list and over the status bar. The first two give the window's
    menu with "Message >" or "Folder >" first, then File; the status bar
    gives File first. The poll limit is raised for the longer run.
  - themetest-fake's context menu checks now expect "Selection >" in the
    window menu.
- !ClawsMail 0.6, !L3afpad 0.19, GTKThemeTest 0.43.

## 3.24.41-riscos1.0-rc1 (release candidate, signed off on the Pi, 2026-10-09)
The first release candidate: !L3afpad 0.18, !ClawsMail 0.5, GTKThemeTest
0.42, and the complete source. Everything above "Unreleased" below.
- ClawsMail 0.4 on the Pi: two size icons appeared at first start
  until something redrew the window; the main window opened at start
  rather than starting on the icon bar; closing it quit (it should stay on
  the icon bar). Fixed:
  - `gtkriscostheme.c`: when the grip moves between the window's corner
    and the outside scroll bar, GTK redraws the corner, so the grip drawn
    there in the first frame doesn't stay;
  - Claws patch, main.c: on RISC OS the main window isn't shown at start
    (only the first time, after the set-up wizard); Select on the icon
    shows it. mainwindow.c: its close icon hides it (Quit: icon bar menu,
    File > Exit). !Help says so.
  - claws-fake 18 checks (3 new: starts on the icon bar, Select shows the
    window, closing hides it; plus a guard that the corner has no second
    grip, though the fake didn't reproduce the Pi's case).
- A clean build from a fresh clone (`make deps` then `make release`)
  succeeds: 34 static libraries, three packages, the source tar and the
  manifest.
- Signed off on the Pi: size icon right in all three programs; ClawsMail
  IMAP receive and SMTP send with TLS 1.2 through AcornSSL; starts on the
  icon bar, closing the last window leaves it there; all working.
- Version named `3.24.41-riscos1.0-rc1` (a .x point number added).
- Release documents: `docs/RELEASE-NOTES.md`, `docs/RC-TEST-PLAN.md` (the
  Pi sign-off), `docs/BUILDING.md`, `docs/SOURCE-AND-LICENCES.md`; the
  README rewritten for the release.
- `build/make-release.sh` / `make release RELEASE=...`: from a committed
  tree, the three programs, `riscos-gtk-<release>-src.tar` (the
  repository at the commit plus every upstream tarball) and
  `MANIFEST.txt` (SHA-256s, commit, toolchain, UnixLib). Makefile targets
  `l3afpad`, `themetest`, `claws`, `release`.
- Every package now carries `LicenceLGPL` (one copy in
  `riscos/licences/`) and a `Source` file (commit, release, what it links,
  where the source archive is, a three-year offer); GTKThemeTest gains its
  MIT `Licence`. !Help licence paragraphs list them. ClawsMail's default
  version in build-claws.sh is 0.4.
- LICENCES.md: the LGPL obligation is met by the source archive (all
  three programs are open source), not an object-file relink kit; UnixLib
  5.0.3.3.

## Unreleased (only a bar in the corner takes the grip: !ClawsMail 0.4, !L3afpad 0.17, GTKThemeTest 0.41, 2026-10-07)
- A scroll bar must run to the bottom right edge to claim the grip. So
  0.3's grip at the foot of a bar that ends above the
  corner is gone: only a vertical bar running down into the bottom right
  corner takes the grip (at the window's foot, under the bar); a window
  without one has the empty scroll bar outside it, as in 0.2. Claws Mail's
  main window (message list's bar ends above the message view) has the
  outside bar again; L3afpad's text and the compose window keep the grip
  under their bar. A scrolled area in the corner whose bar comes and goes
  shows it always (so the window doesn't change width); one elsewhere is
  left alone.
- claws-fake check 11 now: the main window has the scroll bar of its own
  (its Wimp window a bar wider than GTK's).

## Unreleased (the app's own scroll bar takes the grip: !ClawsMail 0.3, !L3afpad 0.16, GTKThemeTest 0.40, 2026-10-07)
- A program's own scroll bar on the window's right edge makes the extra
  Wimp scroll bar unneeded. Now:
  - any vertical GTK scroll bar at the window's right edge takes the
    grip, not only one running into the corner. The lowest such bar is
    used. One with something under it (Claws Mail's message list, above
    the message view and status bar) stops a grip's height short and the
    grip sits at its foot, covering nothing;
  - a scrolled area at the right edge whose vertical bar comes and goes
    (GTK_POLICY_AUTOMATIC) shows it always, as RISC OS windows' scroll
    bars are always there, so the window doesn't change width as the bar
    comes and goes;
  - only a window with no scroll bar at its right edge at all gets the
    empty scroll bar outside it.
  - GDK: `gdk_riscos_window_set_size_grip_place (window, x, y, width)`
    (y: the grip's top; -1 for the window's foot).
- Tests: claws-fake 15 checks: the main window has no extra scroll bar
  (its Wimp window is as wide as GTK's).

## Unreleased (IMAP; the size grip's scroll bar: !ClawsMail 0.2, !L3afpad 0.15, GTKThemeTest 0.39, 2026-10-07)
- **!ClawsMail 0.2: IMAP** (stage 2), through libetpan 1.9.4, its TLS
  through AcornSSL like the rest (AcornSSL is used for all SSL and
  TLS). libetpan is built for GnuTLS against Claws Mail's GnuTLS-subset
  headers, so its sessions are AcornSSL sessions in `ssl_acornssl.c`;
  the shim gained what libetpan uses (GnuTLS 2 type names,
  gnutls_retr2_st, set_retrieve_function, set_default_priority,
  dh_set_prime_bits, handshake_set_timeout, perror, bye,
  GNUTLS_VERSION_NUMBER). IMAP runs in Claws Mail's own thread (UnixLib
  pthreads): the handshake only runs Claws Mail's idle from the main
  thread. `etpan_certificate_check` trusts AcornSSL's own check (it
  doesn't hand certificates out). libetpan is linked as C (automake
  picked the C++ linker for a Windows-only file, and libtool then read
  the toolchain's .la files, which name another machine's paths). No
  OpenSSL, SASL, Berkeley DB, curl, expat or zlib. libetpan's and
  Claws Mail's etpan code: no cast-align warnings.
- Tests: claws-fake 14 checks (4 new): a second account (IMAP over imaps,
  `tests/claws/mailserver.py` now serves IMAP4rev1 too) has its INBOX
  found and scanned, logs in, selects, fetches the message whole into
  the cache, through a third AcornSSL session.
- **The size grip no longer covers a window's contents** (it
  overhung part of the contents and needs a scroll bar to sit on). A
  document window whose bottom right corner has no GTK scroll bar running
  into it (Claws Mail's main window, with its
  status bar; GTKThemeTest) gets an empty vertical scroll bar down its
  right edge, outside GTK's window, as the Wimp's furniture is outside
  the work area, with the grip at its foot: GTK draws the bar (with the
  theme's scroll bar style) into an image (`gtkriscostheme.c`); GDK's new
  `gdk_riscos_window_set_size_strip` widens the Wimp window by it and
  plots it, and clicks on it (other than the grip) do nothing. GTK's
  window keeps its size and place, so nothing is laid out again or
  moves. A vertical bar that ends above the corner (something under it)
  no longer gets the grip in its column. `RISCOS_GTK_DEBUG_GRIP=1` logs
  the decisions. hellotest's toggle-size check allows for the strip.

## Unreleased (Claws Mail: !ClawsMail 0.1, 2026-10-07)
- **!ClawsMail 0.1: Claws Mail 4.4.0, the second GTK program** (chosen
  as an email client that can use AcornSSL). Stage 1:
  POP3 and SMTP, local MH folders, no plug-ins, no IMAP (libetpan: stage
  2). `build/build-claws.sh` (Nettle 3.10.2 built into the stage for its
  stored-password DES), `patches/claws-mail-4.4.0-riscos.diff`,
  `apps/claws/`.
  - **TLS through AcornSSL**: Claws Mail's GnuTLS code is kept (pop3s,
    smtps, STARTTLS, its settings) against a small GnuTLS-subset shim
    (`src/common/gnutls-acornssl/gnutls/*.h`, `src/common/ssl_acornssl.c`,
    included by ssl.c): a session is AcornSSL_CreateSession on the
    socket's RISC OS handle, with the server name (SNI and AcornSSL's
    certificate check); the handshake steps run inside Recv (1-byte
    MSG_PEEK) with the desktop kept going (claws_do_idle) for up to 180 s
    - AcornSSL may be asking about a certificate; record_recv/send are
    AcornSSL_Recv/Send, EWOULDBLOCK as GNUTLS_E_AGAIN; check_pending a
    peek. Client certificates, certificate files, PKCS#12 and GnuTLS's
    ciphers aren't available (stored passwords: `--with-password-
    encryption=old`).
  - RISC OS: no fork() for name lookups (UnixLib's is a vfork): the lookup
    is done in place and handed over from an idle (socket.c); no Unix
    domain socket for a single copy (main.c: the icon bar does it);
    AI_ADDRCONFIG 0; a SOCKS5 port stored byte by byte (unaligned);
    gtk_unit_get_type renamed (clashed with GTK's when linked statically);
    UnixLib has no fputs_unlocked, so the plain stdio calls.
  - `apps/claws/riscos-claws.c`: fonts and GTK's data from the
    application; HOME = <Choices$Write>.ClawsMail (settings, and the
    mailbox unless another place is chosen at first start); the Log in
    <Wimp$ScrapDir>.ClawsMail; the icon bar (Select: the main window;
    Menu: Info, Choices... = Preferences, Quit through Claws Mail's own
    quit, which keeps open messages in Drafts); a file dropped on the
    icon or the application starts a message with it attached, its MIME
    type (and an extension, if the name has none) from its file type.
  - !Run needs AcornSSL (System:Modules.Network.URL.AcornSSL, RISC OS
    5.28+).
- Tests: **claws-fake** (11 checks) runs Claws Mail on the fake Wimp
  against `tests/claws/mailserver.py` (POP3; SMTP that refuses MAIL
  before STARTTLS) through a fake AcornSSL in the fake RISC OS (SWIs
  &50F80-&50F90, from riscos-ffmpeg's tests): it starts with its main
  window, asks for the POP3 password, fetches over pop3s into the MH
  inbox, opens the compose window from a mailto: URL, sends with
  STARTTLS, files the message in Sent; a session and handshake for each,
  SNI "localhost", sessions closed before their sockets. The linuxarm
  build defines __riscos, so it runs the RISC OS code paths.
  `check-unaligned.sh` over Claws Mail's sources: 20 casts checked by
  hand (`apps/claws/unaligned-safe.txt`), one fixed (SOCKS5).

## Unreleased (multiple windows: !L3afpad 0.14, GTKThemeTest 0.38, 2026-10-07)
- **!L3afpad: a window per document in one task** (each
  file its own window, as Edit). Select on the icon bar and File > New
  open a new empty window (a hidden empty one first); a file dropped on
  the icon or double-clicked opens in a window of its own, 48 OS units
  down and right of the last; a file that's open already has its window
  brought forward. Closing a window asks to save, then closes it (the
  last one is emptied and hidden: L3afpad stays on the icon bar). Quit /
  PreQuit ask if any window has unsaved text; File > Quit asks in each.
  Choices apply to every window.
  - How: L3afpad keeps a window's state in `pub` and file-level variables.
    The patch lists them per file (`src/riscos-windows.h`, tables in
    callback.c, hlight.c, indent.c, linenum.c, menu.c, undo.c, view.c;
    callback.c's two function statics moved to file level) and factors
    the window set-up out of main() (`l3afpad_setup_window`);
    `apps/l3afpad/riscos-l3afpad.c` keeps a Doc per window and swaps the
    variables (and `pub`) when an input event (click, key, focus in, drop,
    close) goes to another window or one of its dialogue boxes or menus -
    not while a modal dialogue box holds the input. A new window starts
    from the variables' values at program start. Search history and the
    clipboard stay shared.
- **GDK:** `gdk_riscos_set_event_watch` - a program sees each event before
  GTK without replacing GTK's handler (riscos-gtk's own Menu-over-text
  handler is already installed with gdk_event_handler_set).
- Tests: l3afpad-fake 44 checks (7 new): the icon drop opens a second
  window (the first keeps its file), offset 24,24 pixels; typing in the
  second leaves the first unmodified with nothing to undo; Edit > Undo in
  the first (after a click in it) leaves both texts alone (fails without
  the variables being swapped); Select with windows open gives a third,
  empty window; the open file again gives no new window (acknowledged);
  closing one of several destroys it.

## Unreleased (vector printing: !L3afpad 0.13, GTKThemeTest 0.37, 2026-10-07)
- **Printed text was poor** (the job seen in the printer's
  viewer: thin, broken letters). Pages were bitmaps; a PDF/PostScript
  driver or a viewer scaling them shows that. Now text, lines and solid
  shapes are sent as **Draw paths** (Draw_Fill / Draw_Stroke, colours by
  ColourTrans_SetGCOL), as sharp as the printer prints; what can't be a
  vector (images, gradients, transparency, clip paths, colour glyphs) is
  printed as before, as bitmaps over them, only where it is.
  - cairo: new `cairo_riscos_page_vectors` (`cairo/cairo-riscos-print.c`,
    `cairo/cairo-riscos.h`; `patches/cairo-1.18.0-riscos.diff` made by
    `build/mkcairopatch.sh`; `build/dev-cairo.sh` rebuilds an existing
    cairo): cairo's analysis surface (as for PDF/PostScript) against a
    surface of ours that accepts opaque solid fills, strokes (uniform
    scale; dashes, caps, joins, mitre limit) and outline text (unhinted
    fonts), unclipped or in one rectangle; the NATIVE region is replayed to
    callbacks as paths, the rest returned as a region.
  - GDK: the paths kept as Draw paths (1/256 OS unit, y up from the
    paper's bottom), drawn in every rectangle PDriver asks for; the image
    region in strips at up to 600 dpi on top (sprites can now be a part
    of a row: x position). `RISCOS_GTK_PRINT_BITMAP=1`: all bitmap, as
    0.12.
- Tests: the fake printer draws Draw_Fill/Draw_Stroke into the page with
  cairo; l3afpad-fake checks the text page is all vectors (3 Draw calls a
  band, no sprites) with the header and text where they were. New
  printvec-fake (7 checks): a solid even-odd rectangle and a scaled dashed
  curve come out as vectors; a gradient, a half-transparent fill and a
  circle-clipped paint as the image region; a solid square over the
  gradient stays in the image.

## Unreleased (start-up crash, real cause: !L3afpad 0.12, GTKThemeTest 0.36, 2026-10-07)
- **0.11 still stopped at start-up with "EMT trap"** under the Raspberry
  theme. The backtrace, read against 0.11's own build, is in block_find at
  the name comparison, after `readable()` had passed: the Wimp's block
  holds words that are addresses of memory that is mapped (so
  OS_ValidateAddress passes) but not readable in user mode (privileged),
  and reading it aborts. 0.10's crash was the same line; 0.11's "wrapping
  range" diagnosis was wrong (its guard stays, harmless).
  `readable()` now asks OS_Memory 24 (Check memory access, RISC OS 5.21+)
  for bit 0, "completely readable in user mode", falling back to
  OS_ValidateAddress where OS_Memory 24 isn't there.
- Fake: OS_Memory 24 from the process's map (/proc/self/maps);
  toolsprites-fake puts a mapped PROT_NONE page among the tool block's
  pointers: 0.11's code segfaults on it, 0.12's passes.

## Unreleased (start-up crash: !L3afpad 0.11, GTKThemeTest 0.35, 2026-10-07)
- **0.10 stopped at start-up with "EMT trap" under the Raspberry theme**
  (a Pi's Log: backtrace block_find < try_block < tool_area < tool_uri <
  make_css < gtk_init). Looking for the tool sprites, the Wimp's block is
  tried as a table of pointers; with the Raspberry theme loaded one of its
  words is near the top of memory, and OS_ValidateAddress on start .. start
  + 48 wrapped past &FFFFFFFF, which faults in the kernel (the check is
  older than 0.10; which words the block holds depends on the theme).
  `readable()` now refuses ranges that wrap, are over 64 MB or start in the
  bottom page; span_find's window is clamped likewise.
- The fake OS_ValidateAddress now aborts on a wrapping range, as the Pi
  does; toolsprites-fake puts &FFFFFFF0 and &10 in the tool block (without
  the fix it aborts).

## Unreleased (Raspberry theme: !L3afpad 0.10, GTKThemeTest 0.34, 2026-10-07)
- **The Raspberry theme's scroll bars came out in garbled colours**
  (seen on the Pi). That theme's tool sprites are 8bpp without a palette;
  the RISC OS 5 Wimp colours them through translation tables kept in the
  same Tools file as sprites "table_<Wimp colour>" (the Wimp's
  Doc/ToolTables: a 256-word image, &00BBGGRR each; table_2 when there's
  none for the colour). We read them with the standard 256-colour palette.
  Now such a sprite uses the table for the colour its part is drawn in (a
  document window's: slider = scroll bar inner 1, well and arrows = outer
  3, gadgets = title background 2), else table_2. The Pi's Wimp gives
  pointers only to the sprites it knows by name, so the tables are found
  by name in the memory around them.
- New test `toolsprites-fake` (3 checks): a Pi-style tool block with 8bpp
  unpaletted sprites and table_1 / table_2 beside them.

## Unreleased (print layout and resolution, size icon: !L3afpad 0.9, GTKThemeTest 0.33, 2026-10-07)
- **Printed pages were laid out in pixels, not points** (seen on the Pi:
  L3afpad's header printed over the first line). GTK's default unit is the
  surface's own, which programs take to be points (GTK's PDF and
  PostScript surfaces on Unix are 72 dpi); our page bitmap was in printer
  pixels, so offsets such as L3afpad's "10 mm above the margin" came out
  a quarter as far at 300 dpi. GTK now gets 72 dpi contexts: each printed
  page is drawn into a cairo recording surface in points, and the preview's
  bitmap has a device scale of 90/72.
- **Higher resolution:** pages print at the printer's own resolution up
  to 600 dpi (was 300; Draft still 150). The recording is drawn at that
  resolution a strip at a time (at most 4 MB of sprite) for each rectangle
  PDriver_GetRectangle asks for, so memory doesn't grow with resolution
  and banding printer drivers get only their band. Strips are plotted at
  180/dpi OS units a pixel (OS_SpriteOp 52 scale 90/dpi) from rows a whole
  number of OS units down, so they meet exactly.
- **Size icon in the scroll bar's column** (seen on the Pi: a few pixels
  right of the scroll bar and narrower). GTK's vertical scroll bar stops
  short of the window's right edge (the text's frame), and the sicon
  sprite is not quite the bar's width; the grip is now drawn under the bar,
  as wide as it (new `gdk_riscos_window_set_size_grip_column`, set from the
  theme's scroll bar fitting), and the click area follows it.
- Tests: the fake printer asks for the page in 3 bands and puts the page
  together from the strips (FAKE_PRINT_DUMP=file.ppm writes it);
  l3afpad-fake 37 checks: strips per band, header apart from the text in
  the top margin (2 lines of ink, first at row 186 of 3508), the grip under
  the scroll bar.

## Unreleased (print residue: !L3afpad 0.8, GTKThemeTest 0.32, 2026-10-07)
- **Printing left the Print box's area unredrawn** over the window and the
  pinboard (seen on the Pi). The print job stayed selected while GTK went
  back to Wimp_Poll between pages, so the redraws the Wimp asked for went
  to the printer driver instead of the screen. The job is now suspended
  (PDriver_SelectJob 0) except while a page prints and at EndJob/AbortJob.
- Fake printer: SelectJob tracks the selected job; Wimp_Poll counts polls
  made while a job is selected. l3afpad-fake has 35 checks, adding "no
  Wimp_Poll while the job is selected" (a mutation without the suspend
  gives 7).

## Unreleased (printing: !L3afpad 0.7, GTKThemeTest 0.31, 2026-10-06)
- **Printing** through RISC OS's printer drivers (chosen: the
  Toolbox Print box, bitmap pages, a preview window of our own).
  - GTK patch: gtkprintoperation-unix.c hands every platform function to
    the new `gtk/gtkprint-riscos.c` on RISC OS (GTK's print back-end
    modules aren't used).
  - **Print box:** the Toolbox PrintDbox, laid out from the Toolbox
    manual's print-2.png (320 x 196 pixels at 2 OS units): All / From [ ]
    to [ ]; Copies; Scale %; Upright / Sideways; Draft; Save, Cancel,
    Print (default). Its title is the printer's name. Save keeps the
    settings for the program's next Print without printing.
  - **Pages:** GTK draws each page into a white 32bpp sprite on the
    printer's paper (PDriver_PageSize) at its resolution, at most 300 dpi
    (Draft 150); sideways pages are turned by GTK; scale by GTK; copies by
    the printer driver.
  - **GDK (`gdk/riscos/gdkprint-riscos.c`):** the printer protocol with
    !Printers (Message_PrintSave broadcast; PrintFile then PrintTypeOdd
    answered with PrintTypeKnown; PrintError shown; a DataSaveAck (busy,
    wants to queue) refused; no !Printers: straight to the driver), then
    "printer:" + PDriver_SelectJob, and per page PDriver_GiveRectangle
    (the whole paper in OS units at its bottom left), PDriver_DrawPage
    with the copies, OS_SpriteOp 52 scaled to the paper for each
    rectangle, PDriver_GetRectangle; PDriver_EndJob (AbortJob on an
    error), the previous job back, hourglass on meanwhile. PrintFile,
    PrintError, PrintTypeOdd and SetPrinter added to Wimp_Initialise's
    list. The PDriver SWIs aren't in the 2026 PRM: numbers and registers
    from the RISC OS 3 PRMs as in DeskLib's PDriver veneers and Print
    library.
  - **Print Preview:** the pages drawn at 90 dpi (actual size on the
    desktop) and shown in a window: Page n of m, Back, Next, and Print
    (the program's own Print menu item). Page Up/Down, Escape.
  - New API (gdkriscoswimp.h): `gdk_riscos_printer_info`,
    `gdk_riscos_print_begin/_page_surface/_page/_end`.
- **!L3afpad 0.7:** printing on (File > Print..., Print Preview); !Help.
- Tests: l3afpad-fake 34 (Print box gadgets and layout, the !Printers
  messages, the printed job: one page, 2 copies, the A4 paper 1488 x 2105
  OS units at 300 dpi = 2480 x 3508 pixels, the text inside L3afpad's
  margins; the preview). Fake Wimp: a fake printer (PDriver SWIs,
  "printer:", hourglass) recording what it's given.

## UnixLib 5.0.3.3: !L3afpad 0.6, GTKThemeTest 0.30, GTKHello 0.5, GTKTest 0.6 (2026-10-06)
- **Relinked against UnixLib 5.0.3.3** (riscos-unixlib
  handoff 2026-10-06-all-ports-unixlib-5.0.3.3: fork with threads, waits
  in read/write/stdio (eventfd, used by GLib, can be cancelled), getserv*,
  heap gaps, swprintf, exit codes). No riscos-crossdev release has it yet,
  so `build/unixlib-overlay.sh` makes a copy of crossdev
  1.3 with the v5.0.3.3 release's libunixlib.a (sha256 e24f16c3…, checked);
  the public headers are the same as 5.0.3.2's, so only the link changes.
  `build/env.sh` sets UNIXLIB_VERSION from the library's checksum and
  notes when it isn't 5.0.3.3. Every program checked to contain 5.0.3.3's
  `__pthread_held_wait`. !Help files say "riscos-crossdev 1.3 and UnixLib
  5.0.3.3". PThreadTicker stays 0.03.

## !L3afpad 0.5, GTKThemeTest 0.29 (2026-10-06)
- **Windows open in the middle of the screen** (the application and
  Choices windows among them). A toplevel GTK hasn't placed (no position
  asked for: GTK_WIN_POS_NONE, no gtk_window_move, so no GDK_HINT_POS /
  USER_POS) is centred on the screen the first time it's shown, then
  reopens where it was (gdkwindow-riscos.c, `placed`). Windows GTK places
  keep their place: Save/Open boxes at the pointer, message dialogues on
  their parent. `RISCOS_GTK_CENTRE=0` turns it off.
- Tests: l3afpad-fake 27 (L3afpad's window and the Choices window open
  centred).

## !L3afpad 0.4, GTKThemeTest 0.28 (2026-10-06)
- **The icon bar done as our other RISC OS programs do it** (0.3's
  didn't follow RISC OS practice; modelled on Reel, !FFmpeg's front end,
  YTDL, NetStats, RDPClient):
  - the icon: non-indirected sprite icon, flags &301A, 0,0,68,68 (as
    Reel/!FFmpeg);
  - the menu: the program's name as title, **Info, Choices... (only if the
    program has settings), Quit**: no Open.../Close (files go by dragging
    to the icon or double-clicking; the close icon closes); plain 12-byte
    items &07000021, width 16 a letter + 16; at x-64, top 96 + 44 a line
    (exactly: the OS-units-to-pixels round trip no longer adds 2); Adjust
    keeps it open (reopened), as !FFmpeg's;
  - Info: the same About this program window as proginfo.h (Name:,
    Purpose:, Author:, Version: right-aligned on grey; grey R2 display
    fields, centred, &1700013D; 152 + value width + 12, rows 52);
  - Quit with unsaved work, and Message_PreQuit: a RISC OS question box
    (Wimp_ReportError, question, Cancel / Discard), as !FFmpeg's Convert
    asks; Cancel on PreQuit objects (acknowledged), Discard lets
    Message_Quit come. No GTK dialogue, no Ctrl-Shift-F12 dance;
  - one copy: a second copy (TaskManager_EnumerateTasks, same task name)
    hands its start-up file to the first (Message_DataLoad, window -2,
    icon -1) and closes down, as Reel does; DataLoad to icon -1 accepted;
    a directory dropped on the icon gets "That's a directory" (as Reel).
  - API: handlers are now show, open_file, choices, quit, unsaved; new
    gdk_riscos_iconbar_set_start_file, gdk_riscos_ask; the resume and
    PreQuit-event API is gone.
- !L3afpad 0.4: Quit from the icon bar asks via the question box then
  quits; passes its start-up file to a running copy.
- Tests: l3afpad-fake 25 (menu lines and position, icon flags, PreQuit and
  Quit question boxes). Fake Wimp: Wimp_ReportError recorded and answered
  (fake_report_answer).

## !L3afpad 0.3, GTKThemeTest 0.27 (2026-10-06)
- **The icon bar** (GTK programs had no icon bar icon).
  Decided: every program gets one automatically, with hooks; a
  program stays on the icon bar when its last window closes; L3afpad
  gets a new Choices window.
  - GDK (`gdk/riscos/gdkiconbar-riscos.c`): Wimp_CreateIcon on the icon
    bar's right side, the program's sprite (`!` + program name, else
    `application`); Mouse_Click with window -2; Message_DataOpen for the
    claimed types and Message_DataLoad to the icon, both acknowledged with
    DataLoadAck; Message_DataSave to the icon answered with a DataSaveAck
    to <Wimp$Scrap>; Message_PreQuit objected to (acknowledged) when the
    program has unsaved work, and after a desktop shutdown it carries on
    with Ctrl-Shift-F12 (Wimp_ProcessKey &1FC) when the program exits;
    the Info box is a Wimp window (Name, Purpose, Author, Version), the
    Info item's submenu. DataOpen (5) added to Wimp_Initialise's list.
  - GTK (`gtk/gtkiconbar-riscos.c`, from gtk_init): Select/Adjust show
    the program's window; Menu opens Info >, Open..., Close, Choices...,
    Quit at the Style Guide's place (64 OS units left of the click, top at
    96 + its height); items it can't do are shaded. Each comes from the
    program's handlers, else its GtkApplication (activate, open, the
    "preferences" and "quit" actions; the application is held so it stays
    on the icon bar), else its own menu items found by label.
  - API (gdkriscoswimp.h): gdk_riscos_iconbar_set_handlers (show,
    open_file, open, close, choices, quit, unsaved), _set_info,
    _set_sprite, _add_filetype, _disable; RISCOS_GTK_ICONBAR=0 for none.
  - Opening another menu now closes the one that was open (its owner is
    told), so the icon bar menu and GTK's Wimp menus don't trip over each
    other.
- **!L3afpad 0.3**: starts on the icon bar (a window only when given a
  file); Select opens the window; the close icon empties the window
  (asking to save) and hides it; text files double-clicked in the Filer
  (while it's loaded), dropped on the icon or saved to it from another
  program are opened (the last as untitled); Choices... opens "L3afpad
  choices" (font, tab width, word wrap, line numbers, auto indent; Cancel
  / Set), kept in <Choices$Write>.L3afpad.Choices; Info gives the
  version; PreQuit with unsaved text asks first.
- Tests: l3afpad-fake 23 (icon, Select, the icon bar menu and Info box,
  Choices, close-to-icon-bar, DataOpen, a drop on the icon, PreQuit +
  Cancel, Quit from the icon bar); themetest-fake 38 (a program with no
  sprite gets "application"). Fake Wimp: Wimp_CreateIcon (icon bar),
  Wimp_ProcessKey's key, Mouse_Click icon handles.

## !L3afpad 0.2, GTKThemeTest 0.26 (2026-10-06)
- **Options > Font crashed !L3afpad** (the Log: "Unknown type
  PangoFontFamily specified in treemodel model", then an EMT trap).
  GtkBuilder finds types it hasn't seen by looking up `*_get_type` in the
  program with GModule, which a statically linked RISC OS program can't
  do; GTK's own types are covered by gtk_test_register_all_types, but the
  font chooser's list store names Pango types. GTK patch (gtkbuilder.c):
  on RISC OS, a table of the Pango, GdkPixbuf and GIO types builder files
  name (PangoFontFamily/Face, GAppInfo, GIcon, GFile, GMenu...).
- **Size grip** (the draggable corner at the bottom right). The
  Wimp only puts its Adjust Size icon where a window's scroll bars meet,
  and GTK windows have no Wimp scroll bars. Resizable document windows
  (not dialogue boxes) now show the desktop's own size icon sprite
  (`sicon`, from the window border sprites; a plain ridged corner if
  there isn't one) in the work area's bottom right corner; Select or
  Adjust on it starts the Wimp's own size drag (Wimp_DragBox 2), Select
  bringing the window to the front. A GTK scroll bar running into that
  corner is given a margin so it stops above (or beside) the grip, as a
  Wimp scroll bar stops at the Adjust Size icon (gtkriscostheme.c).
  New `gdk_riscos_window_get_size_grip()`. `RISCOS_GTK_SIZE_GRIP=0` turns
  it off.
- Tests: l3afpad-fake 15 (font dialogue opens and lists fonts; grip drawn,
  starts DragBox 2 without a GTK click; scroll bar ends above it; no grip
  on a dialogue). Fake Wimp has `sicon`/`psicon` tool sprites.

## !L3afpad 0.1, GTKThemeTest 0.25 (2026-10-06)
- 0.24 confirmed on the Pi (the Log: no warnings left,
  icons from sprites 25 of 25).
- **!L3afpad 0.1**: L3afpad 0.8.18.1.11 (GPL-2+; Ubuntu universe source),
  the first real GTK program ported. build/build-l3afpad.sh: no configure
  (apps/l3afpad/config.h; printing off), patches/l3afpad-*-riscos.diff:
  New empties the window (asking to save) instead of starting a second
  copy; extra dropped files ignored (no second copy); no stdin read.
  apps/l3afpad/riscos-l3afpad.c: fonts and data from the application,
  messages to <Wimp$ScrapDir>.L3afpad.Log (opened once), a RISC OS file
  name from !Run made a UnixLib one, RISCOS_GTK_DEFAULT_FILETYPE=fff.
  App: !Run (%*0: a file dropped on the application opens), !Boot,
  !Sprites (from L3afpad's icon, tools/png2sprite.py), !Help, Licence
  (GPL-2) and LicenceLGPL, DejaVu Sans Mono too.
- Found by L3afpad, fixed in the backend:
  - **Typing lost e, o and space** in a window with a menu: the keymap
    (gdkkeys-riscos.c) left translate_keyboard_state's consumed modifiers
    unset, so GTK's key hash matched menu mnemonics (Alt+E _Edit, Alt+O
    _Options...) on plain keys. Now 0.
  - **Text dropped on a GtkTextView went in twice** (GTK asks twice: the
    view's drag-drop and GTK_DEST_DEFAULT_DROP). Drop data is now given
    one request at a time from the main loop, so the second comes after
    gtk_drag_finish and gets nothing, as on X11.
  - **Menu over a multi-line text view in a window with a menu** opens the
    program's menu (as in Edit or StrongED), not GTK's Cut/Copy/Paste menu
    for the view; text fields keep theirs (gtkmenu-riscos.c, an event
    handler ahead of GTK's).
  - Save boxes: a name without an extension is the program's type if it
    sets RISCOS_GTK_DEFAULT_FILETYPE (hex), else Data.
  - RISCOS_GTK_KEY_DEBUG logs each key (code, keyval) to the Log.
- build-app.sh: copies any other files the application has (!Boot,
  !Sprites, Licence...), fills @VERSION@ in !Help, EXTRA_FONTS.
- Tests: l3afpad-fake (10: start, type, Menu gives L3afpad's menu, File >
  Save As is a Save box, drag-save as Text, saved, title, a Latin-1 text
  file dropped in once, no warnings, quit) added to build-linuxarm.sh.

## Unreleased (GTKThemeTest 0.24, 2026-10-06)
- 0.23's Pi Log: icons from sprites 25 of 25 (every Wimp sprite looked
  for was there). Two warnings remained, both fixed:
  - "GLib-GIO-WARNING: Failed to create file monitor for .../!UnixHome/
    home/.config/glib-2.0/settings/keyfile": GLib chose its keyfile
    GSettings backend (writes under UnixHome, needs a file monitor RISC
    OS hasn't got). The backend now sets GSETTINGS_BACKEND=memory when
    the program hasn't chosen one (gdkdisplay-riscos.c).
  - "Gtk-WARNING: Unknown value of _NL_TIME_WEEK_1STDAY" (GtkCalendar):
    UnixLib's nl_langinfo isn't glibc's. GTK patch (gtkcalendar.c): on
    RISC OS the week starts on the territory's first working day
    (Territory_ReadCalendarInformation; Monday in the UK, else Monday).
- themetest-fake 37 (GSettings in memory).

## Unreleased (GTKThemeTest 0.23, 2026-10-06)
- Icons (chosen: the desktop's sprites first, a shipped set for
  the rest):
  - At start-up the theme makes an icon theme "RISCOS-Wimp" of the
    desktop's own sprites for the names RISC OS has pictures for:
    directories (directory, small_dir), applications, file types
    (file_xxx / small_xxx for text, HTML, CSV, images, PDF, archives,
    fonts, data...), discs, and the error box symbols where the Wimp has
    them. The Log line says how many ("icons from sprites n of 25").
  - It inherits "RISCOS", shipped in each app's share/icons: Adwaita 3.36's
    icons (GNOME Project, LGPL-3 / CC-BY-SA 3.0) for the 199 names GTK and
    programs use (stock items, toolbar actions, dialogue symbols, the file
    chooser's), PNGs at 16-48 pixels, symbolic icons as GTK's
    .symbolic.png (no SVG loader on RISC OS). 3.2 MB, made at build time
    by build/mkicons.py from theme/icons-list.txt.
  - An empty hicolor theme is shipped too, so GTK no longer warns about a
    missing icon theme (the file chooser's "user-home-symbolic-ltr").
  - The backend's default gtk-icon-theme-name is now RISCOS.
- themetest-fake 36 (folder from the desktop's sprite, toolbar icons and
  image-missing from RISCOS, symbolic icons load). The fake Wimp pool has
  a "directory" sprite.

## Unreleased (GTKThemeTest 0.22, 2026-10-06)
- GtkFileChooserDialog (a program's own file dialogue, what most GTK
  programs use) is shown as the RISC OS Save box or Open box too. The
  dialogue stays hidden (GTK patch: its show() opens the box instead); when
  the box closes the dialogue gets its response (its own Save/Open
  button's id, or Cancel / delete), and gtk_file_chooser_get_file() and
  friends return the chosen file (GTK patch to gtk_file_chooser_get_files,
  "riscos-gtk-files"). The box takes the dialogue's title, button labels,
  current name and folder; a name that already exists there starts as its
  full name, so Save saves it again. gtk_dialog_run() and the response
  signal both work. RISCOS_GTK_NATIVE_CHOOSER=0 still gives GTK's own.
- GTKThemeTest: "Save (dialogue)..." and "Open (dialogue)..." buttons.
  edittest-fake 44 (dialogue Save box, save, Open box, cancel).

## Unreleased (GTKThemeTest 0.21, 2026-10-06)
- Theme layout checked against the Toolbox manual (riscos.com, chapter 16
  Window class gadget pictures, chapter 5 Discard/Cancel/Save), measured
  pixel by pixel. Decision: keep the theme's colours (only layout and sizes
  to follow the manual); widgets still read from the desktop's sprites.
  - number range: the field has its own border; the adjuster arrows sit
    outside it on the grey, bare (no slab), 4px to its right.
  - slider: a well with a 4px margin inside its border, the bar from the
    start to the value, no knob (bar colour unchanged, dark blue).
  - a window with a title bar but no title (GTK message dialogues) shows
    the program's name, as the Discard/Cancel/Save box does.
- Already matched: action buttons and their sizes, display fields,
  writables (52 OS units), labelled boxes, option/radio icons and scroll
  bars (the desktop's sprites), Wimp menus, the Save box (0.20).
- Noted, not changed (colours, as decided): the manual's default
  button channel is cream, labelled boxes are light grey/white, slider
  bars grey. GTK 3 paints box-shadows in list order (last on top), so the
  default button shows only its outer grey ring; left as it is.

## Unreleased (GTKThemeTest 0.20, 2026-10-06)
- 0.19's Save box shape and size were wrong (wide and short). Now
  laid out from measurements of the Toolbox manual's picture of the
  SaveAs box (1 pixel = 2 OS units): work area 155 x 148, icon 34 x 34
  centred 6 from the top, writable 26 high inset 18/17, the Selection
  option (shaded: GTK has no selection to save) 6 in, Cancel 64 x 28 and
  Save 56 x 28 (default channel round it) along the bottom, 10 below.
  Widths grow only if the desktop font needs it.

## Unreleased (GTKThemeTest 0.19, 2026-10-06)
- 0.18 works, but the Save box looked nothing like RISC OS's.
  Redone after the Toolbox SaveAs dialogue (riscos.com Toolbox manual,
  ch. 14, and its picture): file icon, the name centred under it with the
  caret at its end, Cancel and Save (default) the same width across the
  bottom; a small dialogue box with a title bar only.
- Transient like the Toolbox's: a Save box opened by a click or from a
  menu is opened as a menu (Wimp_CreateMenu with its window handle), so a
  click elsewhere closes it (Message_MenusDeleted -> cancelled). Opened
  from the keyboard it's static. The Open box is always static.
- Save with only a leaf name: the Save As module's error box ("To save,
  drag the icon to a directory display", Wimp_ReportError).
- Window furniture follows GTK: title bar unless undecorated; close icon
  unless gtk_window_set_deletable (FALSE); toggle/adjust size only if
  resizable; back icon on ordinary windows, not dialogue boxes. A Wimp
  window is made again before it's shown when that changes.
- New GDK API: gdk_riscos_window_set_transient_dbox, gdk_riscos_report_error.
- edittest-fake 40 (window flags, transient box, MenusDeleted cancel).

## Unreleased (GTKThemeTest 0.18, 2026-10-05)
- M4 starts: RISC OS Save boxes and Open boxes for GtkFileChooserNative
  (GTK patch: MODE_RISCOS in gtkfilechoosernative.c; gtk/riscos/
  gtkfilechooser-riscos.c). Save: drag the file icon (Wimp sprite
  file_xxx for the name's type, DragASprite) to a Filer window or another
  program; DataSave / DataSaveAck give the name; after the program saves,
  the file gets its type (OS_File 18) and the receiver Message_DataLoad.
  OK takes a full name, or the program's current folder plus the leaf.
  Open / Select folder: drag a file (directory) from the Filer to the box,
  or type its full name. RISCOS_GTK_NATIVE_CHOOSER=0 keeps GTK's own.
- Files dropped from the Filer on a GTK window (Message_DataLoad) reach the
  widget as a file:// URI (text/uri-list); text files also as their text.
  gdk_riscos_window_set_load_func() lets a window take dropped files
  itself.
- New GDK API (gdkriscoswimp.h): gdk_riscos_path_to_unix/_from_unix,
  gdk_riscos_save_drag_begin/_drop/_done, gdk_riscos_window_set_load_func.
- Apps ship GTK's compiled GSettings schemas (share/glib-2.0/schemas):
  GtkFileChooserNative builds GTK's own chooser too, which needs
  org.gtk.Settings.FileChooser, and GLib stops the program without it.
- GTKThemeTest: Save as..., Open... and a file drop area on the Dialogues
  page. edittest-fake: 9 new checks (35).

## Unreleased (GTKThemeTest 0.17, 2026-10-05)
- 0.16's scroll bars are correct on the Pi (tool sprites through
  the pointer table). "All sheets (mixed)" didn't change when clicked:
  GTK keeps a check button inconsistent until the program clears it, and
  themetest didn't. It now clears it on the first toggle (mixed -> on ->
  off), as a tri-state option should behave.

## Unreleased (GTKThemeTest 0.16, 2026-10-05)
- 0.15's Pi Log: Wimp_ReadSysInfo 9's block: +16 and +20 both
  &201A3058, which points to a table of addresses &2047xxxx (the sprites,
  elsewhere); +24 &234, +28 &20, +32 an address inside the block. New
  fourth way: the table of sprite pointers at +16's address (count from
  +4). Debug also prints the first table entries as sprites.
- The fake Wimp's tool block is now laid out as the Pi's (header, +16/+20
  -> table -> sprites outside the block); the suite takes that way.

## Unreleased (GTKThemeTest 0.15, 2026-10-05)
- 0.14's Pi Log: Wimp sprites 7 of 7 (pool via Wimp_BaseOfSprites;
  4bpp mode 20 without palettes), true colours right. Tool sprites still
  0: Wimp_ReadSysInfo 9's block at &20045AE0 starts 00008D90 0000005A
  0042F3A4 00438124 201A3058: size and count (90) of a sprite area, but
  first/free aren't offsets into it. Tool sprites are now found in such a
  block three ways: a plain area; by name anywhere in the block (header
  checked); or through a table of sprite pointers after the header.
  RISCOS_GTK_DEBUG names the way used, or dumps more of the block.
- The fake Wimp's tool block has the Pi's odd first/free words, so the
  suite takes the by-name path; TT_ICONS takes the plain-area path.

## Unreleased (GTKThemeTest 0.14, 2026-10-05)
- 0.13's Pi Log: "Wimp sprites 0 of 7; window border sprites 0 of 27";
  Wimp_ReadSysInfo 9 = &20045AE0 not taken as a sprite area; the Wimp
  colours came as #f0f0f0 for white.
- Wimp_ReadPalette with R2 = "TRUE" (true colours; without it the bottom
  4 bits of each component are 0, per the PRMs).
- Wimp sprites: searched in the pool's two areas (Wimp_BaseOfSprites: RAM
  part first, then ROM), Wimp_SpriteOp 24 as a fallback.
- RISCOS_GTK_DEBUG: why each Wimp sprite wasn't found or decoded, each
  sprite's format, and the first words at Wimp_ReadSysInfo 9's R0/R1.
- Fake Wimp: the sprite pool is a real sprite area (header), returned by
  Wimp_BaseOfSprites.

## Unreleased (GTKThemeTest 0.13, 2026-10-05)
- 0.12 still didn't use the RISC OS widgets on the Pi. The theme now
  always writes one line to the program's log: Wimp colours read or not,
  Wimp sprites found (of 7), window border sprites found (of 27), and
  whether the PNGs could be written (and where), to see on the Pi which
  step fails.

## Unreleased (GTKThemeTest 0.12, 2026-10-05)
- 0.11 on the Pi: GTK's scroll bars didn't follow the desktop's
  theme (RISC OS 5 is themeable). Scroll bars are now drawn with the
  Wimp's window border (tool) sprites, names from the PRMs' "Naming
  Convention": uicon/dicon/licon/ricon (and pressed p...), the wells
  (v/hwell caps and section) and sliders (v/hbar ends and middle, pressed
  while dragged), blicon in the corner. GDK: gdk_riscos_wimp_tool_sprite
  reads the tool sprite area from Wimp_ReadSysInfo 9 (not in the 2026 PRM
  text, so the area is checked: readable, sane header, an uicon in it),
  preferring the screen's resolution suffix (uicon22 at EIG 1). Decoding:
  8bpp sprites with a 16-entry (old-style) palette.
- Test data: a few RISC OS 5 (Sovereign) tool and Wimp sprites as PNGs from
  marutan/riscos-free-icons (Apache-2.0), tests/data/riscos-free-icons;
  TT_ICONS=that/Sovereign themetest-fake previews widgets in them. The
  fake Wimp has a tool sprite area (Wimp_ReadSysInfo 9); themetest-fake
  checks a scrolled window draws with it (33 checks).

## Unreleased (GTKThemeTest 0.11, 2026-10-05)
- Theming redone with the Wimp's sprites read. At start-up
  (gtk_init_check, GTK patch) gtk/riscos/gtkriscostheme.c adds a style
  provider just above the theme (GTK_STYLE_PROVIDER_PRIORITY_SETTINGS + 1:
  GtkSettings adds the theme at SETTINGS, not THEME) that redefines
  @wimp0-15 as the desktop's Wimp colours (Wimp_ReadPalette) and uses the
  Wimp sprite pool's optoff/opton, radiooff/radioon, gright (pop-up
  button) and left/right (adjusters), written as PNGs to a temporary
  directory (removed at exit). Missing sprites keep the theme's pictures;
  the mixed state keeps the theme's (RISC OS has none).
  RISCOS_GTK_WIMP_THEME=0 turns it off; RISCOS_GTK_DEBUG logs the CSS.
- GDK: gdk_riscos_wimp_palette, gdk_riscos_wimp_sprite (new public header
  gdkriscoswimp.h, gdk/riscos/gdkwimptheme-riscos.c): reads a Wimp sprite
  (Wimp_SpriteOp 24) and decodes it: 1/2/4/8 bpp with or without palette
  (Wimp colours / standard 256-colour palette), 16/32 bpp, old-style,
  1bpp and RISC OS 5 alpha masks, old numbered modes and both mode word
  forms; scaled to screen pixels by eig.
- Fake Wimp: Wimp_ReadPalette and a sprite pool in those formats.
  themetest-fake checks each format decodes, and that the theme draws in
  a changed desktop palette and with the Wimp's option/radio sprites
  (32 checks).

## Unreleased (GTKThemeTest 0.10, 2026-10-05)
- 0.9's pointers are right on the Pi; the scroll wheel did nothing. Windows
  now ask for extended scroll requests (extra window flags bit 1, window
  block +39); the wheel's Scroll_Request (+32/+36 in OS units, multiples
  of 4, y positive = up) becomes GDK scroll events at the pointer, one
  step per 64 OS units (at least one, at most 8), up/down/left/right
  (the protocol's scroll message gained left and right). Which window the
  wheel goes to is the Wimp's choice (*WimpScroll -Type).
  RISCOS_GTK_DEBUG logs each request's amounts, to tune the step on the Pi.
- Fake Wimp: Scroll_Request (script reason 10). edittest-fake scrolls the
  long list down three nudges and up one (26 checks).

## Unreleased (GTKThemeTest 0.9, 2026-10-05)
- Pointers in the RISC OS pointer colours, and the text pointer
  and caret as RISC OS has them.
- Our pointer sprites now use the desktop's pointer colours: filled with
  the lighter of pointer colours 1 and 2, outlined with the darker (read
  with OS_ReadPalette, R1 = 25), and OS_SpriteOp 36 keeps the pointer
  palette (R3 bit 5).
- The text pointer is the Wimp's own ptr_write (Wimp_SpriteOp 36, active
  point in its middle from Wimp_SpriteOp 40), ours if the Wimp hasn't one.
- GTK patch: the text caret is the RISC OS caret, an I: red stem with a bar
  2 px each side across its top and bottom (gtkstylecontext.c); GtkTextView
  redraws that much (gtktextview.c).
- Fake Wimp: OS_ReadPalette 25, Wimp_SpriteOp 40 and ptr_write.
  edittest-fake checks ptr_write, the colours, and the caret's bars (24).

## Unreleased (M3: pointer shapes; GTKThemeTest 0.8, 2026-10-05)
- Pointer shapes (new gdk/riscos/gdkpointer-riscos.c): GTK's cursor names
  map to RISC OS shapes (gdkcursor-riscos.c): I-beam (text), hand (links),
  horizontal/vertical/diagonal resize, cross-hair, move. Each is a 2bpp
  sprite with a palette made at start-up (white shape, black outline, 90
  dpi, no mask so the given active point is used), set as pointer shape 2
  with OS_SpriteOp 36. The server shows the shape of the window under the
  pointer and the normal pointer (Wimp_SpriteOp 36 "ptr_default", shape 1,
  which also restores the palette) everywhere else, after a mode change,
  and at exit.
- Fake Wimp: OS_SpriteOp 36 (user area by name; 2bpp with palette
  checked) and Wimp_SpriteOp 36. edittest-fake checks the I-beam over a
  field, the hand over a link, the normal pointer elsewhere and over
  another task's window, and pictures the pointers (22 checks).

## Unreleased (GTKThemeTest 0.7, 2026-10-05)
- Pi report on 0.6: dragging text showed a black rectangle and didn't
  always copy. The rectangle was GTK's drag icon window: GTK gives a
  popup's type hint in gdk_window_new's attributes, which the backend
  ignored, so the DND window was opened on the Wimp (unpainted) under the
  pointer; Wimp_GetPointerInfo then reported it, and the drop went to the
  icon instead of the field or other task's window. The hint is now taken
  from the attributes, and a DND window is never opened.
- Fake Wimp: Wimp_GetPointerInfo reports the frontmost window (the last
  opened), not the scripted target first, which had hidden this.
  edittest-fake checks no window of ours is under the pointer while
  dragging (18 checks).

## Unreleased (M3: text fields, the clipboard, dragging text; GTKThemeTest 0.6, 2026-10-05)
- Pi report: text fields had no Cut/Copy/Paste menu, and dragging text
  did nothing. GtkEntry/GtkTextView open their menu only after asking the
  clipboard for its TARGETS; the backend's convert_selection was a stub,
  so the answer never came (GTK gives up after 30 s). Selections now work:
  window properties are kept in memory (gdkproperty-riscos.c), a
  conversion goes to an owner in this program as a selection request, and
  the answer comes back as a selection notify.
- The RISC OS clipboard (gdktransfer-riscos.c): Copy/Cut broadcasts
  Message_ClaimEntity; another task's claim clears GTK's owner. Paste
  from another task: Message_DataRequest, DataSave, RAMFetch/RAMTransmit
  (or DataSaveAck/<Wimp$Scrap>/DataLoad). Another task pasting ours: its
  DataRequest gets DataSave, then RAM transfer or a file. Latin-1 <-> UTF-8.
- Drag and drop: between our widgets in-process (as GDK's Win32 "local"
  protocol); onto another task's window, icon bar or Filer window, the
  text is saved there with Message_DataSave; text saved into our window by
  another task (DataSave) or a text file from the Filer (DataLoad &FFF) is
  dropped into the widget under the pointer. Wimp_DragBox (dashed box)
  while dragging; GTK's drag icon window is not shown.
- GTK patch: gtk_drag_begin_internal works on a copy of the trigger event
  (without a GDK-managed drag, its own grab cancelled GtkEntry's gesture,
  which freed the event, and it was read afterwards).
- Backend: each pointer motion is handed to GLib at once (it was queued
  until a non-null Wimp event, so GTK only saw where a drag ended up).
- gtk-enable-primary-paste off: Adjust no longer pastes X's PRIMARY.
- edittest-fake (new, 17 checks): the field's menu, Copy to and Paste from
  another task, dragging text out, between fields, and in. The fake Wimp
  has Wimp_SendMessage (with a hook), queued messages from "other tasks",
  Wimp_TransferBlock and Wimp_DragBox. hellotest gives GTK's frame clock
  real time after the toggle-size request (it could fail under load).
- Groundwork for pointer shapes: GDK cursor names map to RISC OS shapes
  (not applied yet).

## Unreleased (M3: the window menu; GTKThemeTest 0.5, 2026-10-05)
- GTK programs now use menus as RISC OS does, Menu anywhere.
  A GtkMenuBar now takes no room and draws nothing (gtkmenubar.c patch);
  a Menu click in its window that no widget used opens one Wimp menu at
  the pointer (-64 OS units), titled with the program's name, with the
  menu bar's titles leading to their menus as submenus. Shortcuts,
  ticks, Adjust all as before; widgets' own context menus come first.
  RISCOS_GTK_MENUBAR (or RISCOS_GTK_MENUS) keeps GTK's menu bar.
- The hidden bar lays its items out at their natural size (clipped by its
  1px window) so GTK doesn't warn about negative content heights.
- themetest-fake: File > Save and View > Show grid (Adjust) through the
  window menu, release off the window, menu bar takes no room (25/25).

## Unreleased (GTKThemeTest 0.4, 2026-10-05)
- Pi report: the bottom of the tab area moved when Apply (the default
  button) was clicked and took focus. The theme's 4px margin on
  button.default came and went as GTK restyled the default button on
  focus, so the button row changed height by 8px. The R6 channel is now
  only a box-shadow, no margin (theme rev 3.2). themetest-fake gives the
  window the caret (Gain_Caret) so clicks focus buttons as on the Pi, and
  checks the tab area's height when Apply takes focus (23/23; 556 -> 564
  before the fix).

## Unreleased (GTKThemeTest 0.3, 2026-10-05)
- Pi report: menu bar menus showed garbled text until reopened. GTK pops
  a menu bar menu up twice (press, then release); the second time the
  Wimp menu was closed, its memory freed and a new one opened, and the
  Wimp drew from the freed block. Now the open menu is kept, and menu
  memory is freed late (the last 8 menus are kept).
- Backend: implicit grab. A button release with the pointer off our
  windows (over a Wimp menu just opened from the menu bar) was dropped,
  so GDK thought the button still held and sent the next click anywhere in
  the window to the menu bar title (which reopened its menu). Events of a
  held button now go to the window it went down in.
- Debugging on the Pi: RISCOS_GTK_MENU_DEBUG traces menus, RISCOS_GTK_DEBUG
  logs mouse buttons, both to the program's log.
- Fake Wimp: Open_Window_Request can move a window; FAKE_SLEEP;
  fake_target_window (scripted events to a chosen window, not the newest);
  fake_wimp_script_pos(). themetest-fake 22/22: Edit opened, pointer off
  the window onto the menu and released there (menu stays, text intact),
  the next click reaches Apply, the window stays where the Wimp moved it.

## Unreleased (M3: Wimp menus; GTKThemeTest 0.2, 2026-10-04)
- GtkMenus are shown as Wimp menus (`gtk/riscos/gtkmenu-riscos.c`, hooked
  into gtk_menu_popup_internal / gtk_menu_popdown and
  gtk_menu_shell_activate by the GTK patch): titles, keyboard short-cuts
  in a column (^O, F3, Shift-F3), ticks for check and radio items, shaded
  items, dotted lines for separators, submenus. Menu bar menus open below
  their title, combo boxes to the right of the field (as Toolbox
  pop-ups), context menus 64 OS units left of the pointer. The Wimp
  closes them (click elsewhere, Escape: Message_MenusDeleted); a choice
  activates the item from an idle callback; Adjust reopens the menu with
  ticks updated before the next Wimp_Poll (the backend holds its poll
  until GTK is done). RISCOS_GTK_MENUS set: GTK's own menus.
- Backend: Wimp menu API (`gdk/riscos/gdkriscosmenu.h`), Menu_Selection
  and Message_MenusDeleted, Wimp_TextOp widths, UTF-8 to Latin-1.
- Fake Wimp: menu position, closing, Wimp_TextOp 1, MenusDeleted with the
  menu pointer, FAKE_BUTTONS (Adjust), the tree closes on a choice.
- themetest-fake rewritten around real use: pop-up, File (Save), View
  (Adjust on Show grid, then closed), tabs, context menu (Info): checks
  menu contents, ticks, separators, shading, positions and the actions
  (19/19).

## Unreleased (GTKThemeTest 0.1, 2026-10-04)
- `tests/gtk/themetest.c` / !GTKThemeTest: every common GTK 3 widget in
  the RISCOS theme on four pages, a menu bar with ticks and a submenu, a
  context menu, popover, message dialogue, tooltips, info bars, calendar;
  every interaction logged. `build/build-app.sh` builds any test app
  (build-hello.sh and build-themetest.sh use it).
- `tests/gtk/themetest-fake.c` (QEMU, fake Wimp): clicks every tab, opens
  the File menu, closes it with Escape, pictures each page; checks window
  size, menu item names drawn, no GTK warnings (8/8).
- Backend: a client-side resize is confirmed with a configure event, as
  Broadway's server does. GTK freezes a mapped window's drawing until it
  arrives, so any window that changed size after opening stopped drawing.
- Backend: the screen resolution is set to 90 dpi
  (gdk_screen_set_resolution); gtk-xft-dpi alone wasn't applied, so text
  was laid out at 96 dpi. hellotest checks it (15/15).
- Theme: hover rules in menus use child selectors. GTK 3 makes a submenu
  a CSS descendant of the item it hangs from, so the highlighted menu bar
  title turned every label in its menu white on white.
- tests/gtk/riscos-freopen.c: the one-write-open wrapper, shared by the
  QEMU tests.

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
- Theme toward the RISC OS 5 default desktop (chosen: fonts and
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
- GTKTest 0.5 on a Raspberry Pi (UnixLib 5.0.3.2-rc2): glibtest 29/29,
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
  passes riscos-crossdev's checks; on the Pi: 29/29.
- `tests/fake/`: riscos-mesa's fake Wimp/screen and event harnesses, for
  the GDK backend's host tests.
