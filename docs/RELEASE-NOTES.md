# riscos-gtk 3.24.41-riscos1.0 — public beta (release candidate 10)

**Status: public beta.** RC11 is the ninth public beta build. Windows
are drawn faster: GTK's backend now copies and plots only the parts of a
window that changed, not the whole window each time, so typing,
scrolling and highlights under the pointer keep up better, and
programs start sooner with themes that have translation tables
(Raspberry). Every program is rebuilt. RC10 added colour emoji (an
optional PackMan font package, TTF-Noto-Color-Emoji) and GTK's
pop-overs; RC9 scaled emoji to the text's size; RC8 fixed Gnumeric's
Insert > Chart and Serif and Monospace. RC6's Menu, title bar, browser
and mtPaint changes and Gnumeric have been run on a Raspberry Pi 4;
RC7's icons, Geany's Build and Execute and parts of RC5 haven't been on
real hardware yet: please report anything that goes wrong (see
"Reporting problems" below). The final 3.24.41-riscos1.0 will be this
build with whatever the beta turns up fixed. Items marked *[Pi]* still
need a check on real hardware ([RC-TEST-PLAN.md](RC-TEST-PLAN.md)).

## What it is

GTK 3.24.41 for RISC OS. GTK programs run as ordinary Wimp tasks:
- **Windows:** Wimp windows with the Wimp's own title bar, close, back and
  toggle icons; GTK draws the work area.
- **Menus:** Wimp menus, opened with Menu anywhere in a window.
- **Files:** the RISC OS Save and Open boxes, and drag and drop with the
  Filer and other programs.
- **Printing:** through !Printers.
- **Look:** built from the desktop's own Wimp colours and sprites.

The libraries are statically linked into each program; there is no
shared GTK module to install.

**Version:** `3.24.41-riscos1.0` (the programs number themselves the same way: their own version and `-riscosN`, e.g. Geany 2.1-riscos9): the GTK release it's built from
(3.24.41), the riscos-gtk revision (riscos1) and a point release (.0;
fixes to this release become riscos1.1, riscos1.2 ...). Release
candidates add `-rcN`.

## What's in RC11

| Package | Version | What |
|---|---|---|
| `L3afpad-0.8.18.1.11-riscos30.zip` | 0.8.18.1.11-riscos30 | A simple text editor. Each document has its own window; it prints, uses Save/Open boxes, and has a Choices window |
| `ClawsMail-4.4.0-riscos20.zip` | 4.4.0-riscos20 | An email client: POP3, IMAP and SMTP, with all TLS through AcornSSL, and local MH folders. Shows HTML mail laid out; links open in your browser (Iris or NetSurf); RISC OS icons. Starts on the icon bar; closing its window leaves it there |
| `Geany-2.1-riscos9.zip` | 2.1-riscos9 | **Demo:** the programmer's editor, built with its own build against riscos-gtk's devkit: syntax highlighting for dozens of languages, a symbol list, tabs. Build commands (GCC, or the DDE with throwback) run in a TaskWindow with their errors in the Compiler tab; Execute runs the program in a task window. No plugins or built-in terminal |
| `mtPaint-3.50.14-riscos9.zip` | 3.50.14-riscos9 | A painting program for pixel art and photos: palettes, layers, animation frames; PNG, JPEG, GIF and BMP. Its file picker is a RISC OS Save/Open box with Browse...; Adjust paints with the second colour |
| `Gnumeric-1.12.57-riscos14.zip` | 1.12.57-riscos14 | The spreadsheet: hundreds of functions, charts, and Excel (.xls, .xlsx), OpenDocument, Lotus 1-2-3, Quattro Pro, HTML, CSV and text files. Save and Open boxes, Menu for its menus, the icon bar, Choices... for its Preferences; CSV files open from the Filer |
| `GTKThemeTest-3.24.41-riscos1.0-rc11.zip` | 3.24.41-riscos1.0-rc11 | Every common GTK widget on four pages: a demonstration and a test of the theme and backend |
| `GTKGLTest-3.24.41-riscos1.0-rc11.zip` | 3.24.41-riscos1.0-rc11 | A spinning triangle in a GtkGLArea, drawn with riscos-mesa, and the frame rate |
| `TTF-Noto-Color-Emoji-2.051.0.1-1.zip` | 2.051.0.1-1 | **Optional** (unchanged since RC10): a PackMan package of Noto Color Emoji (SIL Open Font Licence 1.1) for `!UnixFont`. Needs FontConfig and UnixFont; every GTK program (and other FontConfig programs) then show emoji in colour at the text's size. Install it with PackMan, or copy its font into `!UnixFont.truetype` |
| `riscos-gtk-sdk-3.24.41-riscos1.0-rc11.tar.gz` | | The devkit, for building GTK programs for RISC OS with riscos-crossdev: the libraries, headers, pkg-config, a template program, docs |
| `riscos-gtk-3.24.41-riscos1.0-rc11-src.tar` | | The complete source: this repository at the release commit and every upstream tarball (see [SOURCE-AND-LICENCES.md](SOURCE-AND-LICENCES.md)) |
| `MANIFEST.txt` | | SHA-256 of each file, the commit, and the toolchain and UnixLib used |

GTKHello and GTKTest are development tests and are not part of the
release.

## Requirements

- **RISC OS 5**, in a 16 or 32 bpp screen mode. Tested on a Raspberry Pi
  4 with the RISC OS 5 default and Raspberry desktop themes.
- **ClawsMail only:** RISC OS 5.28 or later, because it needs AcornSSL,
  plus a network set up in !Configure.
- **Modules from PackMan:** SharedUnixLibrary 1.16 or later,
  ARMEABISupport and Iconv.
- **PThreadTicker:** each package carries its own copy, so nothing extra
  is needed.
- **Gnumeric only:** an ARMv7 or later machine (Raspberry Pi 2 or later,
  ARMX6, Titanium...), as for the other programs, and about 31 MB free.
- **Memory:** each program starts in a WimpSlot of 21-31 MB (the program
  plus 4 MB) and grows it as its heap needs more. None makes a dynamic
  area for its heap, so they reserve no logical address space.

## Features of the RISC OS backend

**Windows and drawing**
- **Windows:** each GTK window is a Wimp window. GTK draws into a 32bpp
  sprite and the backend plots it: only the parts GTK repainted since
  the last update, so typing or a highlight doesn't replot the whole
  window. *[Pi]*
- **Colours:** right on both TRGB and TBGR screens.
- **High resolution (EX0 EY0, 180 dpi) modes:** windows, text and
  widgets are their usual size and drawn at the screen's resolution
  (GDK scale 2). Desktop sprites are shown at 180 dpi where the desktop
  has them, otherwise doubled. ClawsMail's icons and both programs'
  application icons are drawn at 180 dpi too.
- **Placement:** new windows open centred on the screen; a program's
  next window opens 48 OS units down and to the right of the last.
- **Toggle size:** works, and the window is laid out again at the new
  size. A maximised window fills the screen below its title bar.
- **Title bars stay on the screen:** a window a program puts at the top
  of the screen is moved down so its title bar shows. Functionality
  confirmed on the Pi.
- **Pop-up windows with a title** (tool windows some programs open
  without a frame, such as mtPaint's Pan Window) get a title bar and a
  close icon; the close icon closes them as Escape does. Functionality
  confirmed on the Pi.
- **The size grip:** a window has the desktop's own Adjust Size sprite,
  following RISC OS:
  - if a GTK scroll bar runs into the window's bottom right corner, the
    grip sits under it;
  - otherwise the window gets an empty scroll bar outside its right edge,
    with the grip at its foot, so the grip never covers the contents.

**Input**
- **Mouse:** Select, Adjust and Menu, and double clicks.
- **Scroll wheel:** works.
- **Keys:** short-cuts as RISC OS writes them (^O, F3, Shift-F3).
- **The caret:** follows the input focus.
- **Pointer:** shapes (I-beam, hand, the resize arrows and others) in the
  desktop's pointer colours.

**Menus**
- **Wimp menus:** GTK menus are real Wimp menus, with ticks, shading,
  dotted separators and a short-cut column.
- **Menu bar:** a program's menu bar becomes a Menu-click menu, as RISC
  OS programs have, and is not drawn in the window.
- **One menu per window:** Menu anywhere in a window gives the same menu,
  even over things that use the mouse buttons themselves (a paint
  program's canvas, toolbars); tool windows without a menu give their
  main window's. Functionality confirmed on the Pi.
  Over something with its own context menu, such as a list, that menu is
  the first entry. In ClawsMail it is "Message >" over the message list
  and "Folder >" over the folder list; elsewhere it is "Selection >".
  Over a text view, Menu gives the window's menu, as in Edit.
- **Pop-ups:** combo boxes are Wimp menus opening to the right of the
  field, as Toolbox pop-ups do; context menus open 64 OS units left of
  the pointer.

**Files and the clipboard**
- **Save:** a Toolbox-style Save box. Drag its icon to a Filer window or
  another program, or type a full path.
- **Open:** a RISC OS Open box that takes a file dragged to it.
- **The directory browser:** the Browse... button on a Save or Open box
  (or Menu over it), or `*Set RISCOS_GTK_CHOOSER filer` to use it every
  time. A Filer-like window: large icons, small icons or full info
  with the desktop's file sprites, sorted as the Filer sorts, Parent and
  the folder's name along the top, and a sidebar of discs (those on the
  icon bar, with their names and sprites: discs, RAM disc, Apps;
  functionality confirmed on the Pi; LanMan98 mounts don't work yet),
  places (the program's folder, Boot, Choices, Scrap) and
  recent folders. Double-click to go into a directory or open a file;
  directories dragged in from the Filer are shown. Save keeps its
  draggable file icon. *[Pi]*
- **Drag and drop:** files dropped on windows and fields are loaded.
  Text dragged out of a field becomes a Text file, or goes to another
  program by RAM transfer.
- **Clipboard:** the global clipboard is shared with other programs, in
  Latin-1 or UTF-8.
- **File types:** set from the name's extension; names without one are
  saved as Text in L3afpad. Programs know a file's type from its name and
  contents (shared-mime-info's database comes in every package), so a
  spreadsheet or a picture dropped on a window is recognised.
- **Names beyond ASCII:** RISC OS file names are Latin-1; GTK shows them
  as UTF-8 and gives them back as Latin-1, so names with accented letters
  or hard spaces show and open correctly.

**The icon bar**
- **Every program has an icon,** as our other ports do. Select shows the
  window; Menu gives Info, Choices... (if the program has settings) and
  Quit.
- **Loading files:** files double-clicked (DataOpen), dropped on the icon
  or saved to it are opened.
- **Unsaved work:** PreQuit asks about it.
- **One copy:** starting a second copy hands its file to the first.

**Running other programs**
- **As RISC OS tasks:** commands a program starts (GLib's
  `g_spawn_async`) run alongside it. When the program reads their output,
  they run in a TaskWindow and the output arrives as it's written, with
  the working directory set and `Sys$ReturnCode` as the exit status;
  otherwise they're started as tasks of their own. *[Pi]*
- **Throwback:** while such a TaskWindow runs, the DDE's tools given
  `-throwback` send their errors to the program, as compiler messages
  that name the file and line (Geany goes to the line). *[Pi]*

**Printing**
- **The Print box:** a Toolbox-style box (pages, copies, scale,
  orientation, draft), printing to !Printers' printer.
- **Print Preview:** in riscos-gtk's own window.
- **Quality:** text, lines and solid shapes go to the printer driver as
  Draw paths. Images, gradients and transparency go as bitmaps at up to
  600 dpi.

**Theme**
- **The RISCOS GTK theme** follows the RISC OS 5 Style Guide sizes.
- **Colours and sprites** come from the running desktop: Wimp colours,
  the option, radio and icon sprites, and the window border (tool)
  sprites for scroll bars. This includes the Raspberry theme's 8bpp tool
  sprites with their colour tables.
- **File and folder icons** come from the desktop's sprites.
- **Icons:** riscos-gtk's own RISC OS-style icons, drawn in the style of
  ClawsMail's, for every name GTK's icon theme has (open, save, print,
  cut, copy, paste, undo, find, zoom, text formatting, warnings...) at
  16 to 48 pixels, and for the small one-colour icons widgets use
  (arrows, a field's clear button, the file chooser's places). Gnumeric's
  and Geany's own toolbar icons are drawn the same way. GNOME's Adwaita
  icons remain for the few names not drawn (emoji pictures). *[Pi]*
- **Fonts:** with PackMan's FontConfig (UnixFC) installed, the
  system's fonts.conf, fonts and shared cache, with the program's own
  DejaVu fonts added; otherwise DejaVu Sans, shipped in each package,
  with a cache in scrap. Seen on the Pi with UnixFC. `*Set
  RISCOS_GTK_FONTS own` keeps to the program's own.
- **Serif, Monospace and Sans** (the names font choosers list first)
  are the system's preferences when its config has them; otherwise
  riscos-gtk's own: DejaVu Serif, DejaVu Sans Mono and DejaVu Sans, then
  Liberation and FreeFont, whichever are installed. Without UnixFC,
  Serif and Monospace look like Sans unless the program carries those
  fonts (L3afpad and Geany carry DejaVu Sans Mono). *[Pi]*
- **The Log says which fonts:** one line with the set-up, how many fonts
  and families were found, the font folders, the config files read, and
  what Sans, Serif and Monospace became (or why UnixFC wasn't used, e.g.
  `UnixFC$Dir` not set because FontConfig hasn't been booted), and which
  font emoji get.
- **Emoji:** from a colour emoji font in UnixFont (the release's
  TTF-Noto-Color-Emoji package, or Twemoji...) when one is installed,
  scaled to the text's size (the system's fontconfig rules when it has
  them, riscos-gtk's copy otherwise); without one, DejaVu Sans's plain
  emoji or boxes with the code in, and the Log's fonts line says to
  install the package. *[Pi]*
- **Typing emoji:** GTK's emoji chooser (Ctrl-. in a text field, or the
  field's menu > Insert Emoji) and ClawsMail's Message > Insert emoji...
  in the compose window. *[Pi]*
- **Pop-overs** (GtkPopover: the emoji chooser, menu buttons' pop-overs)
  are shown; before RC10 they never appeared. *[Pi]*

**ClawsMail**
- **HTML mail:** shown laid out, with its fonts, colours, tables and
  pictures, by Claws Mail's LiteHTML viewer, built into the program. The
  message's headers are shown above it. When a message has both a plain
  text and an HTML version, the HTML one is shown.
- **Pictures on the web** in HTML mail are fetched only if you turn on
  remote content (Choices... > Plugins > LiteHTML), over http or https
  (AcornSSL).
- **Links:** clicking one (in HTML or plain text mail) sends it to your
  browser through RISC OS's URI handler. If no browser is running, Iris
  is started (or NetSurf, if the Filer hasn't seen Iris). mailto: links
  start a new message.
- **RISC OS icons:** the toolbar, the message list's marks and the
  application icon are drawn in the RISC OS style; the folder list uses
  the Filer's own directory icons from the desktop theme. Claws Mail's
  own icons can be chosen in Choices... > Display > Themes.
- **The About box** names AcornSSL and shows the RISC OS port's logo.

**Gnumeric**
- **Files:** Excel (.xls, .xlsx), OpenDocument (.ods), Gnumeric's own,
  Lotus 1-2-3, Quattro Pro, HTML, CSV and text. File > Save As chooses
  the format from the name you give in the Save box. Spreadsheets
  dropped on its window or icon bar icon are opened, and CSV files
  double-clicked in the Filer while it's loaded.
- **Functions and charts:** Gnumeric's function and file format plugins
  and GOffice's chart types are built into the program.
- **Its settings** are kept in `<Choices$Write>.Gnumeric`; Choices... on
  the icon bar menu is Gnumeric's Preferences.
- **File > Properties:** without POSIX owners on RISC OS, the owner and
  group show as numbers.

**OpenGL**
- **GtkGLArea** draws with riscos-mesa: Mesa's software renderer, OpenGL
  2.1 with GLSL 1.20, or OpenGL ES 2.0 with `*Set GDK_GL gles`.
- **Only programs that ask** have it linked in, since Mesa adds about
  9 MB. A program without it shows GTK's "OpenGL is not available".
- **How it works:** GDK reads each frame back into the window, and only
  the GL area. At 180 dpi the area renders at the screen's resolution.
- **To try it,** !GTKGLTest (`make gltest`) spins a triangle and shows
  the frame rate *[Pi]*.

**Network** (ClawsMail)
- **TLS:** all of it goes through AcornSSL: pop3s, imaps, smtps and
  STARTTLS. AcornSSL checks server certificates and asks the user about
  any it can't verify.
- **IMAP** runs in a thread of its own, so the desktop keeps going while
  it talks to the server.

## Known issues and limits in RC11

- **New in RC11, not yet signed off on the Pi** *[Pi]*: only the changed
  parts of a window are drawn (a test build of L3afpad felt quicker);
  anything left undrawn after typing, scrolling or a window coming back
  into view would be a fault in this.

- **New in RC10, not yet on the Pi** *[Pi]*: the emoji package with
  PackMan; ClawsMail's Insert emoji...; pop-overs in every program.
- **Recently used emoji** aren't kept between runs (GTK's settings are
  kept in memory).

- **New in RC9, not yet on the Pi** *[Pi]*: colour emoji at the text's
  size.

- **New in RC8, not yet on the Pi** *[Pi]*: Gnumeric's Insert > Chart
  types and trend lines (fixed by packaging GOffice's type files);
  Serif and Monospace with UnixFC (riscos-gtk's fallback rules).
- **Tested on the Pi (RC7 Gnumeric, 2026-10-18/19):** UnixFC's fonts
  are used once FontConfig has been booted (24 fonts, DejaVu Sans, Serif
  and Sans Mono); its config didn't map Serif and Monospace, which RC8
  fixes.

- **New in RC7, not yet on the Pi** *[Pi]*: the RISC OS-style icons;
  Gnumeric 1.12.57-riscos7 (riscos6's File > Properties fix, its own
  icons). Gnumeric's earlier test builds ran on the Pi: their File >
  Open, Save and directory browser problems (boxes that took no clicks,
  every filing system in Discs, invalid text for names with hard
  spaces) are fixed in GTK, so every program has the fixes.

- **New in RC6, not yet on the Pi** *[Pi]*: programs started as tasks
  (Geany's Build and Execute with GCC or the DDE, throwback), tested on
  the fake desktop.
- **Tested on the Pi (RC6, 2026-10-11):** functionality confirmed: Menu
  over a canvas or toolbar, title bars on titled pop-ups and kept on the
  screen, the browser's discs from the icon bar, mtPaint.
- **LanMan98:** its mounts don't work in the directory browser yet.
- **New in RC5, not yet on the Pi** *[Pi]*: the directory browser; the
  UnixFC font set-up; GtkGLArea (GTKGLTest); Geany. GTK's settings
  schemas built in (for programs that ship none: Geany's crash on Open
  and Save with GCCSDK's autobuilder) are tested on the fake desktop.
- **Tested on the Pi so far (2026-10-10):**
  - RC1 (L3afpad 0.18, GTKThemeTest 0.42, ClawsMail 0.5) signed off: the
    size icon in all three; ClawsMail fetching over IMAP and sending
    over SMTP, both with TLS 1.2 through AcornSSL; ClawsMail starting on
    the icon bar and staying there when its window closes.
  - ClawsMail 0.8: functionality confirmed: one menu per window, HTML
    mail, the RISC OS icons, the About box, and links opening in Iris.
  - ClawsMail 0.9 and L3afpad 0.20: functionality confirmed in a
    180 dpi mode. The high-resolution icons added in ClawsMail 0.10 and
    L3afpad 0.21 haven't been seen on the Pi yet *[Pi]*.
  - Not yet reported on the Pi: L3afpad 0.19 and GTKThemeTest 0.43
    (RC2's one-menu-per-window change: test plan 2.2a) *[Pi]*.
  - POP3 hasn't been tried against a real server.
- **What changed since RC1:**
  - RC2: one menu per window (Menu in ClawsMail gave a different menu
    depending on where you clicked); any context menu is the window
    menu's first entry. In GTK, so all three programs were rebuilt.
  - RC3: ClawsMail 0.8, with HTML mail, links to the browser, RISC OS
    icons and the About box above. GTK itself is unchanged since RC2,
    so L3afpad 0.19 and GTKThemeTest 0.43 are the same versions as in
    RC2 (rebuilt to name this release).
  - RC4: high-resolution (EX0 EY0, 180 dpi) screen modes. GTK lays
    windows out at their usual size and draws them at the screen's
    resolution; ClawsMail's icons and both programs' application icons
    have 180 dpi versions. In GTK, so all three programs were rebuilt:
    ClawsMail 0.10, L3afpad 0.21 and GTKThemeTest 0.45.
  - RC5: the directory browser and the folder a program's file
    dialogue starts in; UnixFC fonts; built with riscos-crossdev 1.5
    (UnixLib 5.0.3.3 as released); GtkGLArea (GTKGLTest); the devkit;
    Geany. Versions are now the program's own and `-riscosN` (L3afpad
    0.8.18.1.11-riscos24, ClawsMail 4.4.0-riscos13, Geany 2.1-riscos1);
    the test programs take the release's.
  - RC6: programs started as RISC OS tasks (Geany's Build, Execute and
    the DDE's throwback); Menu anywhere in a window; title bars for
    titled pop-ups and kept on the screen; the browser's discs from the
    icon bar; mtPaint. In GTK, so every program was rebuilt: L3afpad
    0.8.18.1.11-riscos25, ClawsMail 4.4.0-riscos14, Geany 2.1-riscos4,
    mtPaint 3.50.14-riscos4.
  - RC7: Gnumeric; RISC OS-style icons; file types from names and
    contents; Latin-1 file names; the browser's Discs on a real desktop;
    built with riscos-crossdev 1.8.2. In GTK and GLib, so every program
    was rebuilt: L3afpad 0.8.18.1.11-riscos26, ClawsMail
    4.4.0-riscos16, Geany 2.1-riscos5, mtPaint 3.50.14-riscos5.
  - RC8: Gnumeric's chart types; fonts: Serif and Monospace fallbacks
    with UnixFC, the program's own fonts added file by file, the Log's
    fonts line. In GTK, so every program was rebuilt: L3afpad
    0.8.18.1.11-riscos27, ClawsMail 4.4.0-riscos17, Geany 2.1-riscos6,
    mtPaint 3.50.14-riscos6, Gnumeric 1.12.57-riscos11.
  - RC9: colour emoji scaled to the text's size. In GTK, so every
    program was rebuilt: L3afpad 0.8.18.1.11-riscos28, ClawsMail
    4.4.0-riscos18, Geany 2.1-riscos7, mtPaint 3.50.14-riscos7, Gnumeric
    1.12.57-riscos12.
  - RC10: the optional TTF-Noto-Color-Emoji package; pop-overs shown;
    ClawsMail's Insert emoji.... In GTK, so every program was rebuilt:
    L3afpad 0.8.18.1.11-riscos29, ClawsMail 4.4.0-riscos19, Geany
    2.1-riscos8, mtPaint 3.50.14-riscos8, Gnumeric 1.12.57-riscos13.
  - RC11: only the changed parts of a window copied and plotted; a
    faster search for a theme's translation tables. In GTK, so every
    program was rebuilt: L3afpad 0.8.18.1.11-riscos30, ClawsMail
    4.4.0-riscos20, Geany 2.1-riscos9, mtPaint 3.50.14-riscos9, Gnumeric
    1.12.57-riscos14.
- **Fixed after the first look at ClawsMail 0.4 on the Pi:**
  - two size icons at first start (the old one stayed in the window's
    corner until something redrew it): GTK now redraws the corner when
    the grip moves to the outside scroll bar;
  - it opened its main window at start: it now starts on the icon bar,
    and Select opens the window (the first time only, after the set-up
    wizard, the window opens by itself);
  - closing the main window quit it: it now hides the window and stays
    on the icon bar; Quit is on the icon bar menu and in File.
- **Changing between a 180 dpi mode and an ordinary one** while a
  program runs keeps its windows the right size, but it keeps the
  sharpness it started with until it's restarted. The pointer shapes
  riscos-gtk draws itself (resize arrows and the like) are half size in
  a 180 dpi mode.
- **RISC OS theme changes** take effect at the next start of a program
  (RISC OS themes only change on a reboot anyway).
- **GTK's Page Setup dialogue** isn't replaced: the paper size comes from
  the printer driver.
- **No translations:** messages are in English only.
- **Each program carries its own copy of the libraries,** about 20 MB.
- **Geany:** no plugins and no built-in terminal (VTE). Build commands
  must be ones RISC OS has (GCC and make from PackMan, or the DDE's,
  set in Build > Set Build Commands); a build can't be typed into.
  Its settings are in `<Choices$Write>.geany`.
- **Programs started as tasks:** standard input isn't passed on, and the
  working directory (`*Dir`) is the whole desktop's. A program that waits
  for one to finish (`g_spawn_sync`) still stops the desktop meanwhile.
- **Gnumeric:** no Python or Perl functions and plugins, databases
  (GDA), external linear solvers (glpk, lp_solve) or help; SVG pictures
  aren't shown (librsvg is written in Rust, which GCCSDK can't build).
- **mtPaint:** its toolbar icons are XPM pictures, which riscos-gtk
  loads; builds of mtPaint against a gdk-pixbuf that loads its loaders
  as modules can't, in a static RISC OS program. Settings are in
  `<Choices$Write>.mtPaint`.
- **ClawsMail:**
  - no spell checking, other plug-ins or Claws Mail certificate manager;
  - news (NNTP) is built but untested;
  - IMAP with STARTTLS takes the same code path as imaps but hasn't been
    tested against a server;
  - attachments can't yet be dragged out to the Filer or opened with RISC
    OS programs;
  - links in HTML mail open in a browser, but forms and scripts in HTML
    mail do nothing (as in Claws Mail elsewhere).
- **ClawsMail's settings,** and its mailbox by default, live in
  `<Choices$Write>.ClawsMail`.
- **Warnings in Claws Mail's Log:** a few harmless GTK warnings appear
  during start-up (a title set before its window exists; button sizes).

## Reporting problems

- **What to send:** what you did, what happened, your machine, RISC OS
  version, screen mode and desktop theme, and the program's Log:
  `<Wimp$ScrapDir>.<Program>.Log` (for example
  `<Wimp$ScrapDir>.ClawsMail.Log`). A photo of the screen helps.
- **Where:** the riscos-gtk GitHub issues
  (https://github.com/adyoull/riscos-gtk/issues), or the ROOL forum
  thread for the beta.
- **Before reporting a crash:** check that SharedUnixLibrary,
  ARMEABISupport and Iconv are up to date from PackMan.

## Building

[BUILDING.md](BUILDING.md). In short:
- **Toolchain:** riscos-crossdev v1.8.2 (`.riscos-deps`): UnixLib
  5.0.3.3, and its devkit is riscos-mesa 20.3.5-16. RC5 and RC6 used
  crossdev 1.5; before crossdev 1.4 there was no release with 5.0.3.3,
  so RC1–RC4 used crossdev 1.3 with the 5.0.3.3 library swapped in.
- **On GitHub:** every push builds and tests in riscos-crossdev's image
  (`make deps tests hello check sdk`).
- **Packages:** `make deps` builds the libraries, then
  `make release RELEASE=3.24.41-riscos1.0-rc11` builds the packages, the
  devkit, the source archive and the manifest.

## Tests

Under QEMU (`make check`), the libraries and programs built as ARM Linux
code against a fake RISC OS and Wimp:

| Suite | Checks |
|---|---|
| glibtest | 29 |
| rendertest | 22 |
| hellotest (at EIG 1 and EIG 0) | 15 + 15 |
| themetest-fake (at EIG 1 and EIG 0; the RISC OS-style icons) | 39 + 39 |
| edittest-fake | 44 |
| toolsprites-fake | 3 |
| printvec-fake | 7 |
| l3afpad-fake | 44 |
| claws-fake (and Insert emoji: the chooser opens, the emoji goes in) | 41 |
| template-fake (the devkit's template; at EIG 1 and EIG 0) | 9 + 9 |
| gltest-fake (GtkGLArea on riscos-mesa's RISC OS libraries; at EIG 1 and EIG 0) | 9 + 9 |
| gltest-nogl (GtkGLArea without riscos-mesa linked in) | 2 |
| schemas-fake (GTK's schemas for a program that ships none) | 3 + 3 |
| fontconfig-fake (UnixFC, own, forced, UnixFC without serif/monospace rules; the emoji font's size) | 9 + 7 + 7 + 9 |
| browser-fake (the directory browser; the icon bar's discs; Latin-1 names) | 29 |
| geany-fake (Geany 2.1: with the box and with the browser; Build and Execute) | 11 + 11 |
| menuclick-fake (Menu anywhere; titled pop-ups; title bars on the screen) | 14 |
| spawn-fake (programs started as tasks; throwback) | 17 |
| mtpaint-fake (mtPaint: Menu, its Save and Open, pop-ups) | 12 |
| gnumeric-fake (Gnumeric: functions, .ods and .xlsx, a chart, the chart types and trend lines, scrolling past a check box, file types, the grab, its RISC OS icons, the font chooser's Serif and Monospace with UnixFC) | 16 |
| staticmodule-test (GModule plugins linked into a program) | 11 |

All 495 pass at the RC commit.

On the Pi: the sign-off list is [RC-TEST-PLAN.md](RC-TEST-PLAN.md).

## Thanks and licences

- **GTK, GLib, pango, cairo and their libraries:** LGPL and MIT-style.
- **L3afpad:** GPL 2, by Tarot Osuji and Steven Honeyman.
- **Claws Mail:** GPL 3, by the Claws Mail team.
- **Geany:** GPL 2, by the Geany contributors.
- **mtPaint:** GPL 3, by Mark Tyler and Dmitry Groshev.
- **Gnumeric and GOffice:** GPL 2 or 3, by the Gnumeric developers.
- **Noto Color Emoji** (the optional package): SIL Open Font Licence
  1.1, Google.
- **libgsf:** LGPL 2.1; **libxml2, libxslt:** MIT (in Gnumeric).
- **shared-mime-info's database:** GPL 2 or later.
- **Mesa** (GTKGLTest, through riscos-mesa): MIT.
- **libetpan:** BSD-style, by DINH Viet Hoa.
- **litehtml:** BSD-style, by Yuri Kobets (in Claws Mail's LiteHTML
  viewer, GPL 3).
- **gumbo:** Apache License 2.0, Google and contributors.
- **Nettle:** LGPL 3 / GPL 2.
- **Adwaita icons:** LGPL 3 / CC-BY-SA 3.0.
- **DejaVu fonts:** their own free licence.
- **riscos-gtk's own files:** MIT, including the RISC OS-style icons.

Details: [../LICENCES.md](../LICENCES.md) and
[SOURCE-AND-LICENCES.md](SOURCE-AND-LICENCES.md).
