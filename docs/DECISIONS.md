# Decisions

From the feasibility study (2026-09-24) and the choices made since.

## GTK 3.24 with a new native GDK backend
- GTK 3, not GTK 4: GTK 3 draws through cairo image surfaces, which suits
  the Wimp's redraw model; GTK 4 wants GL/Vulkan and newer dependencies.
- A GDK backend (`gdk/riscos/`, modelled on broadway's client side, ~6–9k
  lines), **not** a wrapper mapping GTK widgets onto Toolbox gadgets. The
  gadget set is far too small (no tree view, text view, drawing area or
  size-negotiating layout) and real programs depend on GTK's widget
  internals. GTK draws its own widgets; the theme makes them look like
  RISC OS.
- Native where GTK already has a hook:
  - window furniture is the Wimp's (an unknown backend gets no client-side
    decorations); the theme styles header bars as plain toolbars;
  - popups and menus open as Wimp menus (Wimp_CreateMenu with a window
    handle); a combo's menu opens to the right of its button, as a RISC OS
    pop-up does (backend positioning, not CSS);
  - `GtkFileChooserNative` gets a RISC OS version (a save box with a file
    icon to drag to the Filer; drag from the Filer to open), shaped like
    GTK's Win32 and Quartz versions;
  - later: GMenuModel → Wimp menus, GtkStatusIcon → icon bar icon.
- Payoff programs: AbiWord (fonts, clipboard, file chooser, printing),
  then Gnumeric. LibreOffice and Calligra are out of scope; GIMP stays
  blocked on its plug-in processes.

## Toolchain and libraries
- riscos-crossdev tagged releases (now 1.3: GCC 10.2, UnixLib 5.0.3.2, static only),
  never main. Build scripts adapted from riscos-warzone2100, which already
  builds FreeType, expat, fontconfig and libpng for this toolchain.
- Static linking only (UnixLib has no dlopen): gdk-pixbuf loaders and IM
  modules built in, no print backends at first, no GIO modules.
- Library versions follow Ubuntu 24.04 (GLib 2.80.0, GTK 3.24.41, which
  the theme was made with), using Ubuntu's orig tarballs, which the cloud
  build machine can download.
- Libraries are configured with prefix `/riscos-gtk` and installed into
  `sysroot/riscos-gtk`, so no build-machine path is compiled in. !Run
  files point GLib and GTK at their data with environment variables.

## Choices (2026-10-03)
- Folder `RiscOS/GTK/`, repo `riscos-gtk`, rules as the other ports
  (changes delivered as git bundles; handoff notes kept outside git).
- Own files MIT; copied files keep their licences.
- DejaVu Sans stands in for Homerton (as Warzone 2100 ships it).
- M1 passes on QEMU first; the Pi run of !GLibTest comes later.

## Desktop settings come from the backend (2026-10-04)
RISC OS has no settings daemon or settings.ini convention, so
`gdk_riscos_screen_get_setting` answers GtkSettings with the RISC OS
defaults: gtk-theme-name RISCOS, gtk-font-name "DejaVu Sans 12",
gtk-xft-dpi 90 (RISC OS's nominal resolution, so 12pt = 15 pixels as on
the desktop), animations, overlay scrolling, dialog header bars, cursor
blink and menu/button images off. Ported programs need no code to look
right and can still override with g_object_set. Later these could follow
the user's configured desktop font size (Wimp_ReadSysInfo / Font_ReadDefn
on the desktop font).

## Theme follows the RISC OS 5 default desktop (2026-10-04)
Chosen: the stock RISC OS 5/Pi look (3D grey, Homerton, standard
Wimp palette) rather than RISC OS 3 or a NetSurf-like flat look. Sizes are
the Style Guide's in OS units at EIG 1 (theme/README.md has the table).

## Menus are Wimp menus, built from the GtkMenu (2026-10-04)
GTK's own pop-up windows can't close on a click on another task's window
(RISC OS has no global pointer grab), and RISC OS users expect Wimp menus.
So gtk_menu_popup_internal hands the menu to gtkmenu-riscos.c, which
turns its items into a Wimp menu block (text, short-cuts, ticks, shading,
separators, submenus) and opens it with Wimp_CreateMenu; choices activate
the GtkMenuItems. GTK never shows the GtkMenu. Positions follow RISC OS
conventions (pop-ups to the right of the field; context menus at the
pointer -64 OS units). The old way stays available with RISCOS_GTK_MENUS
set (for comparison; GTK-drawn menus can't close on a desktop click).
Not covered: custom widgets inside menus (only labels/short-cuts are
shown), menu bar keyboard navigation, tear-off menus.

## No menu bar: Menu anywhere opens the program's menu (2026-10-05)
Chosen: the context menu as RISC OS does it, Menu anywhere.
GtkMenuBar is kept (accelerators, mnemonics, app code that updates it)
but takes no space and isn't drawn; its titles become the entries of a
Wimp menu titled with the program's name, opened by Menu where no widget
uses the click. Widgets with their own context menus (text fields, apps'
canvases) keep them. Programs without a menu bar get nothing on Menu
unless they provide a context menu. RISCOS_GTK_MENUBAR restores the bar.
Not handled yet: apps that fill a submenu only when its title is
activated/selected (e.g. "recent files" built on demand).

## Clipboard and drag and drop on the Wimp's data transfer protocol (2026-10-05)
GTK's X-style selections stay as they are inside the program (GTK
shortcuts a selection it owns itself). Between tasks the backend speaks
the PRMs' protocol: CLIPBOARD = Message_ClaimEntity flags 4 and
Message_DataRequest; data moves by RAM transfer, else <Wimp$Scrap>. A
drag from GTK dropped on another task's window is a save to it
(Message_DataSave), as from a RISC OS Save box; the Wimp draws a dashed
drag box (Wimp_DragBox type 5) instead of GTK's drag icon window. Text on
the Wimp side is Latin-1 (type &FFF). Paste is offered while another task
may hold the clipboard (we can't know at start-up): if nobody answers the
DataRequest, nothing is pasted. Not done yet: Message_Dragging /
DragClaim (other tasks' drag previews), ReleaseEntity on quit, files.

## Our own pointer sprites (2026-10-05)
The Wimp sprite pool has ptr_write and ptr_hand, but the PRMs don't give
their active points (they're taken from the mask when there is one), and
there's no resize or move pointer. So riscos-gtk draws every shape itself
(2bpp, palette, no mask, exact active points) and uses only ptr_default
from the Wimp, to put the normal pointer back. Easy to switch a shape to a
Wimp sprite later if the system look is preferred.
Update (0.9): the shapes use the desktop's pointer colours (palette
kept), and the text pointer is the Wimp's ptr_write when it exists.

## The theme takes the desktop's Wimp colours and sprites (2026-10-05)
Chosen over real Wimp icons as widgets (which would fight GTK's
own widgets): the shipped CSS theme stays the base (sizes, layout, the
3D borders in @wimpN colours), and at start-up a provider above it swaps
in the desktop's Wimp palette and the Wimp sprite pool's option, radio,
pop-up and adjuster sprites. Sprites are decoded in GDK (not plotted), so
it needs no redirected VDU output; the PNGs go to a temporary directory
because GTK 3's CSS has no data: URLs.

## Save boxes and Open boxes for GtkFileChooserNative (2026-10-05)
RISC OS has no file selector: programs save by dragging a file icon from
a Save box to a Filer window, and open what the Filer drops on them. So
GtkFileChooserNative (the API GTK programs use for "native" dialogs, as on
Windows and macOS) becomes a Save box or an Open box (MODE_RISCOS, tried
before the portal). GtkFileChooserDialog used directly is still GTK's own
dialog for now. The box answers the program only once the receiver has
named the file (DataSaveAck), so the program saves as it always does,
through UnixLib names; the backend then sends DataLoad (and sets the file
type from the name's extension), as the data transfer protocol asks. The
field holds RISC OS names (Report/txt), so leaf names look as on RISC OS.
Not done yet: saving straight into another program by RAM transfer (it
goes through <Wimp$Scrap>), several files at once, DataOpen.

## The Save box follows the Toolbox SaveAs dialogue (2026-10-06)
0.18's box looked nothing like a RISC OS save dialogue; reference:
the Toolbox manual (riscos.com, ResEd and chapter 14,
SaveAs). So the box copies the SaveAs object's default window: draggable
file_xxx sprite, writable filename (centred, caret at the end), Cancel and
Save (default) action buttons; no Selection option (GTK has no such
notion); title bar only. Transient (Wimp_CreateMenu) when opened by a
click or from a menu, static from the keyboard, as the Toolbox does; Save
with a leaf name reports the module's own error. GDK window furniture now
follows GTK's decorations/functions/resizable/type hint, so other GTK
dialogue boxes lose close/back/size icons the same way.

## Theme checked against the Toolbox manual (2026-10-06)
The theme was checked against riscos.com's Toolbox manual; its
gadget pictures (chapter 16) were measured pixel by pixel. Decision:
keep the theme's original colours; what matters is how dialogue boxes
are laid out and sized; widgets keep reading the desktop's sprites at
start-up. So layout changed (number range arrows outside the field,
knobless Toolbox slider, untitled windows named after the program) and
colours didn't (the manual's cream default channel, light labelled box
and grey slider bar were tried and reverted).
GTK 3 note: multiple box-shadows are painted in list order, each over the
last, the reverse of the CSS spec.

## GtkFileChooserDialog shown as the RISC OS boxes too (2026-10-06)
Most GTK programs create GtkFileChooserDialog themselves rather than use
GtkFileChooserNative. Its show() is patched to open the Save/Open box and
keep the dialogue hidden, so gtk_dialog_run() and response handlers work
unchanged; the chosen file is stored on the dialogue and returned by
gtk_file_chooser_get_files() (which every getter goes through). Extra
widgets, filters and previews programs add to the dialogue are not shown
(RISC OS boxes have no place for them).

## Icons: the desktop's sprites, then a shipped set (2026-10-06)
Chosen (of: desktop sprites + fallback / Adwaita only / a new
RISC OS-style set). Names RISC OS has a picture for come from the Wimp
sprite pool at start-up, like the rest of the theme; everything else
from a trimmed Adwaita 3.36 (PNG). The list of names is
theme/icons-list.txt.

Update (2026-10-17): riscos-gtk now draws its own icons, in the style of
ClawsMail's RISC OS ones, for 116 of the names (toolbar, menu, dialogue
box, media, places): `tools/mkriscosicons.py` with the drawing shared
with `tools/mkclawsicons.py` (`tools/riscosdraw.py`). Each is drawn once
on a 24-unit grid and made at 16, 22, 24, 32 and 48 pixels (outlines kept
at 1 pixel below 24); `build/mkicons.py` puts them over Adwaita's, so
every program and the devkit get them. Adwaita stays for the other names
and for symbolic icons. Original drawings, MIT.

## L3afpad, the first real program; Menu over a text view (2026-10-06)
L3afpad (tiny, GTK 3 only, GtkFileChooserDialog, text view, menus) was
ported as !L3afpad. RISC OS runs one copy of a program, so "New" empties
the window instead of starting another copy. Its window is all text view,
so the program's menu would be unreachable with Menu (the view's own
context menu would take the click): in a window with a menu, Menu over a
multi-line text view now gives the program's menu, as RISC OS editors do;
single-line fields keep their Cut/Copy/Paste menu. Printing is off until
GTK has a RISC OS print route.

## A drawn size grip, not Wimp scroll bars (2026-10-06)
The Wimp draws its Adjust Size icon only in the scroll bar column, and GTK
windows have no Wimp scroll bars. Chosen (of: drawn grip / a real,
empty Wimp scroll bar column beside GTK's own): resizable document windows
get the desktop's `sicon` sprite drawn in the corner of the work area, and
a press there starts the Wimp's own size drag. GTK scroll bars that run
into the corner stop short of it. Dialogue boxes get none (as on RISC OS);
they keep the toggle size icon.

## The icon bar (2026-10-06)
Choices:
- every GTK program gets an icon bar icon automatically, with Info,
  Open..., Close, Choices... and Quit, done by the program's handlers or
  else its GtkApplication or its own menu items (rather than opt-in);
- a program stays on the icon bar when its last window is closed, as RISC
  OS programs do (a per-program change for programs that quit on their
  window's destroy; GtkApplications are held);
- L3afpad's Choices... is a new Choices window (Cancel / Set), kept in
  <Choices$Write>.L3afpad.Choices, rather than a submenu.
L3afpad also starts on the icon bar without a window (as Edit does)
unless it's given a file. It claims Text (&FFF) for Message_DataOpen
only while loaded; its !Boot doesn't take over double-clicked text files.

## The icon bar as our other programs do it (2026-10-06)
0.3's icon bar didn't follow RISC OS practice; it now mimics the other
ports exactly. Reel, !FFmpeg's front end, YTDL and NetStats all: a &301A
sprite icon; Menu = Info (proginfo window) + the program's own items +
Quit, at x-64 / 96+44n; Select opens the main window; Adjust keeps a
menu open; questions as Wimp_ReportError boxes with a custom button; one
copy, the second handing its file over. So the GTK icon bar menu is
Info, Choices... (only when there are settings), Quit; Open... and Close
are gone (RISC OS opens by dragging to the icon or double-clicking, and
closes with the close icon).

## UnixLib 5.0.3.3 before a crossdev release has it (2026-10-06)
Relinked against UnixLib's latest release (5.0.3.3). riscos-crossdev's
latest is 1.3 (5.0.3.2). Chosen (of: overlay now / wait for
crossdev 1.4): crossdev 1.3 with the 5.0.3.3 release library swapped in
(build/unixlib-overlay.sh, checksum-checked; headers unchanged), until a
crossdev release carries 5.0.3.3; a handoff asks Crossdev for 1.4.

## Printing (2026-10-06)
Choices: the Toolbox PrintDbox as the Print box (not GTK's
GtkPrintUnixDialog); bitmap pages (each page a sprite at the printer's
resolution, at most 300 dpi, sent through PDriver: works with every RISC
OS printer driver and matches the screen) rather than a Draw/Font Manager
cairo back end; Print Preview as riscos-gtk's own window (GTK's runs a
Linux previewer). !Printers' queue (a busy printer asking for a file) is
refused rather than saving a printable file.

**Changed 2026-10-07 (printed text was of very poor quality):**
text, lines and solid shapes now go to the printer driver as Draw paths
(Draw_Fill / Draw_Stroke in ColourTrans colours), so they're as sharp as
the printer prints; only what can't be a vector (images, gradients,
transparency, clip paths, colour glyphs) stays a bitmap (up to 600 dpi,
in strips). The split is cairo's own analysis, as for its PDF and
PostScript output, through a small addition to cairo
(`cairo/cairo-riscos-print.c`, `patches/cairo-1.18.0-riscos.diff`):
`cairo_riscos_page_vectors`. Text is glyph outlines (unhinted), not
Font Manager text, so it prints exactly as laid out with any font GTK
uses. `RISCOS_GTK_PRINT_BITMAP=1` prints the old way.

## Multiple windows in L3afpad (2026-10-07)
Chosen: each document its own window in one task (as Edit), not
several copies of the program (each copy would add an icon bar icon and
its own 20+ MB). L3afpad wasn't written for that (one window's state in
globals), so rather than rewrite it the RISC OS patch lists each file's
per-window variables and riscos-l3afpad.c swaps them when another window
is used (input events: GDK's new event watch). The cost is that any new
per-window variable L3afpad gains must be listed; the test's Undo check
fails if the swap is missing.

## Claws Mail, the second program (2026-10-07)
Chosen: an email client that uses AcornSSL, Claws Mail. Staged:
(1) POP3/SMTP with local MH folders, no plug-ins; (2) IMAP through
libetpan with AcornSSL; (3) polish (attachments to and from the Filer by
file type, threads, Choices). TLS: rather than port GnuTLS or OpenSSL,
Claws Mail's GnuTLS code is compiled against a GnuTLS-subset shim over
AcornSSL (`ssl_acornssl.c`), so the system's TLS and its certificate
store and prompts are used, and Claws Mail's own certificate manager
isn't (AcornSSL doesn't hand certificates out). Settings and, by default,
the mailbox live in <Choices$Write>.ClawsMail (HOME). Quit goes through
Claws Mail's own quit (drafts kept), so the icon bar never asks.

## IMAP through libetpan's GnuTLS code (2026-10-07)
Claws Mail's IMAP is libetpan's; libetpan's TLS is OpenSSL or GnuTLS. It
is built for GnuTLS against the same GnuTLS-subset shim as Claws Mail's
own TLS, so one piece of code (`ssl_acornssl.c`) puts every connection
through AcornSSL; no TLS library is ported. Certificates are AcornSSL's
business (it checks, and asks the user): Claws Mail's checks of the
certificate chain are skipped for IMAP as for POP3/SMTP.

## The size grip's scroll bar (2026-10-07)
A RISC OS Adjust Size icon sits at the foot of the vertical scroll bar,
outside the work area. A GTK window without a scroll bar in that corner
now gets an empty one (the slider filling it) outside GTK's window, with
the grip at its foot, rather than the grip drawn over its contents. It
is outside GTK's window so that GTK's layout and position are untouched
(an earlier try, a margin inside the window, grew windows after they
were shown and moved some).
Changed the same day (a program's own scroll bar on the right edge
makes the extra Wimp scroll bar unneeded): any GTK vertical bar
at the right edge takes the grip (at its foot if something is under it),
and such bars are shown always; the extra bar is only for windows with
no scroll bar at their right edge.
Changed again (a scroll bar must run to the bottom right edge to
claim the grip): only a vertical bar running into the bottom
right corner takes the grip; otherwise the window has the outside bar.

## The devkit (M5, 2026-10-10)
The devkit is for cross-compiling only: riscos-crossdev with UnixLib
5.0.3.3 (then 1.3 with the overlay, now 1.5), the toolchain the releases
use. A native GCC
on RISC OS has its own versions and memory limits, and would need its
own build and tests.

- **Contents:** the first devkit has the SDK, the API reference and
  porting guide, and a template program.
- **Packaging:** the template needs packaging, so `riscos-gtk-package`
  is a small script and not a separate tool. It does what
  `build/build-app.sh` does for riscos-gtk's own programs.
- **Layout:** the libraries keep their built prefix, `/riscos-gtk`,
  under `sysroot/`, and pkg-config adds the devkit's path back
  (`PKG_CONFIG_SYSROOT_DIR`). So the .pc files are used unchanged and no
  path has to be rewritten when the devkit is unpacked.
- **Size:** the libraries are stripped of debug information, which
  takes the devkit from 83 MB to 18 MB compressed.
- **`app.h`:** the start-up helper is header-only (static inline), not
  part of libgtk. A program can read and change it, and it needs
  nothing new in GTK.
- **GSettings:** `app.h` first set `GSETTINGS_BACKEND=keyfile`, so that
  GSettings would be kept. That was withdrawn on 2026-10-11: the
  keyfile backend needs a file monitor RISC OS hasn't got, which was
  seen on the Pi in October and is why the backend sets `memory`.
  `app.h` points the XDG user directories at `<Choices$Write>.Name` for
  GKeyFile settings.
- **Licences:** the libraries are LGPL and statically linked. A devkit
  is published only with a release, beside that release's source
  archive. Packaged programs carry `LicenceLGPL` and a `Source` file
  naming it.

## OpenGL for GtkGLArea (2026-10-11)
GtkGLArea draws with riscos-mesa's EGL and OSMesa: the software OpenGL
that SDL2, freeglut and the games already use.

- **Contexts on a pbuffer, not window surfaces.**
  - Each GdkGLContext is an EGL context made current on one shared 1x1
    pbuffer. GtkGLArea renders into its own framebuffer object.
  - GDK's GL painting of whole windows is switched off (the gdkwindow.c
    patch). GTK's software path reads back just the area's pixels and
    paints them into the backing sprite.
  - Painting windows with GL would upload the whole cairo surface as a
    texture and read the whole window back, every frame, on the
    processor. It would also need EGL window surfaces to cooperate with
    riscos-gtk's own window plotting.
- **Weak references, linked on request.**
  - Mesa is about 9 MB, so linking it into every GTK program isn't
    acceptable.
  - GDK and libepoxy refer to EGL weakly. A program asks for GL with
    `-Wl,-u,eglGetProcAddress` and riscos-mesa's libraries; the `-u`
    makes the linker take libEGL, which then resolves the weak
    references.
  - Without it, GtkGLArea gets GTK's normal "not available" error.
- **EGL declared locally:** GTK builds without riscos-mesa's headers.
  Only programs that use GL need its devkit.
- **Versions:**
  - OpenGL 2.1 compatibility, reported to GTK as legacy, which is what
    GTK 3 does wherever core profiles are missing.
  - ES 2.0 with GDK_GL=gles, as in GTK's other backends. A GtkGLArea's
    own use-es can't change it, because its context shares with the
    window's paint context.
- **Testing:**
  - The ARM Linux test links riscos-mesa's actual RISC OS libraries, as
    riscos-mesa's own ARM checks do.
  - Its UnixLib symbols go through a shim: errno, mutexes, the stdio and
    ctype names, and the JIT memory.
  - So the test exercises the same EGL and Mesa code as the Pi.

## riscos-crossdev 1.5 (2026-10-11)
- **The toolchain:** crossdev 1.4 has UnixLib 5.0.3.3, built from the
  release's diff, as riscos-gtk asked (handoffs 2026-10-06 and
  2026-10-08). 1.5 adds riscos-mesa devkit 20.3.5-15 on the same
  toolchain.
- **The overlay goes:** riscos-gtk builds with crossdev v1.5, and
  `build/unixlib-overlay.sh` is gone.
- **UnixLib check:** the 1.4 library is built from source, so its
  sha256 isn't the release library's. `env.sh` now recognises 5.0.3.3
  by `__pthread_held_wait`, which is new in 5.0.3.3. It refuses a RISC
  OS build without it.
- **`-pthread`:** crossdev's driver still rejects it, so riscos-gtk
  keeps its compiler wrapper.

## Fonts: the system's fontconfig when it's there (2026-10-11)
- **The system's set-up first:** with PackMan's FontConfig (UnixFC)
  installed, every GTK program uses the system's fontconfig set-up. That
  means its fonts, its mapping and its shared cache. The program's own
  fonts are added with `FcConfigAppFontAddDir`.
- **Fallback:** the program's own fonts.conf, as before.
- **Where:** `gdk/riscos/gdkfontconfig-riscos.c`, called when the display
  opens, before pango. It follows riscos-warzone2100's
  `riscos_fontconfig.c`; the version is the same, fontconfig 2.14.1.
- **Why:** a shared cache is built once for the machine, not per
  program in scrap. Programs see the user's installed fonts. A missing
  or unwritable scrap cache folder no longer matters when UnixFC is
  installed.
- **Opting out:** `RISCOS_GTK_FONTS=own` keeps a program to its own
  fonts.

## GTK's schemas built in (2026-10-11)
- **Why:** a GTK program packaged without GSettings schemas stops at its
  first file chooser. Geany built by the autobuilder did. riscos-gtk
  can't make every packager ship them.
- **What:** it carries GTK's own compiled schemas (2.3 KB) and writes
  them to scrap only when GLib wouldn't find them. They go in
  `<Wimp$ScrapDir>.riscos-gtk-schemas`, at the front of
  GSETTINGS_SCHEMA_DIR.
- **What's left alone:** a program's own schemas in its share folder
  are still found, and a program that ships GTK's schemas is untouched.
- **Alternatives rejected:**
  - Patching GTK's choosers not to use GSettings would mean three
    patches.
  - Compiling a GSettingsSchemaSource from memory isn't possible: GLib
    only reads schemas from files.

## The directory browser (2026-10-11)
- **Why:** the Save and Open boxes are RISC OS's own way, but some cases
  are awkward with a drag: a program in full screen, choosing several
  files, a folder the Filer hasn't open, or users used to other desktops.
  A Filer-like window in the chooser covers them without replacing the
  boxes.
- **Choosing it:** per program or per user (`RISCOS_GTK_CHOOSER=filer`,
  usually in `!Run`), and at any time with the box's Browse... button (or
  Menu over the box), which keeps the box's name and folder. The boxes
  stay the default. The Save box's Browse... sits on the Selection line,
  which the Toolbox's box leaves half empty; the box grows a little wider
  for it.
- **Look:** a Filer window's content (the desktop's sprites, large and
  small icons, full info, the Filer's sort orders and its Menu > Display)
  plus a sidebar like the Finder's: Discs, Places, Recent. The Save box's
  draggable icon stays in Save, so drag-to-save still works from it.
- **How:** the same `RiscosBox` the boxes use (gtk/riscos/
  gtkfilechooser-riscos.c), so the program sees no difference; GDK reads
  directories with OS_GBPB 12 (file types without a stat per file) and
  lists filing systems with OS_FSControl 33. Filters get MIME types from
  RISC OS file types, as a RISC OS program rarely ships GLib's MIME
  database.
- **The dialogue's folder:** GTK's hidden file chooser widget sets a
  folder only after reading it, asynchronously; the box shows straight
  away. `gtk_file_chooser_set_current_folder_file` keeps the folder for
  it (a GTK patch hunk).

## Geany as the demo (2026-10-11)
- **Why Geany:** a large, well-known GTK 3 program with its own autotools
  build, so it shows the devkit and porting notes working on something
  not written for riscos-gtk; it also exercises the file choosers
  (Geany's crash report came from them).
- **Changed as little as possible:** one patch hunk for where its data
  is (`/<Geany$Dir>/share`); the rest is a main() of our own around
  libgeany (icon bar, Log, RISC OS names), as L3afpad's start-up.
- **Linked from libgeany's objects,** not by libtool: crossdev's `.la`
  files can name the machine crossdev was built on and libtool stops on
  them.
- **Left out:** plugins (no shared objects on RISC OS), VTE, the socket
  (riscos-gtk's icon bar hands files to the running copy instead).

## Program versions (2026-10-11)
- **The program's own version and `-riscosN`** (L3afpad
  0.8.18.1.11-riscos24, Geany 2.1-riscos1), as riscos-gtk's other ports
  number theirs (TORCS 1.3.7-riscos3): the name says which upstream
  release it is. N carries on from the old 0.x count.
- **Only when it changes for that program:** a release doesn't raise a
  program's N unless something in it, or in the GTK it uses, changes
  what it does.
- **Test programs** (GTKThemeTest, GTKGLTest) have no upstream version
  and take the release's.

## Menu always gives a menu (2026-10-11)
- **Why:** on RISC OS, Menu is never anything but the menu button. GTK
  programs use button 3 for other things (painting with the second
  colour, a toolbar's options), and a widget that took the click left
  the window without its menu: in mtPaint Menu worked only over the
  menu-bar row.
- **How:** riscos-gtk's event handler gives the widget the click, then
  opens the window's menu unless a menu was opened meanwhile. A widget's
  own context menu stays the window menu's first entry. Taking the click
  away from widgets first was tried: it lost context menus GTK can't see
  coming (Claws Mail's attachment buttons and HTML links), so the widget
  keeps it.
- **Programs that paint with button 3** should leave it alone on RISC OS
  and use Adjust (button 2) for it, as the mtPaint port does.
- **Tool windows:** a window with no menu of its own, which isn't a
  dialogue box or modal, gives its parent's (transient-for) menu, or the
  program's only menu.

## Pop-ups with a title get a title bar (2026-10-11)
- **Why:** GTK programs open some tool windows as pop-ups
  (`GTK_WINDOW_POPUP`), which have no frame; on RISC OS a user has no
  sign that Escape or a click elsewhere closes them (mtPaint's Pan Window
  and Pattern Chooser).
- **How:** GDK gives a pop-up with a title and the normal type hint a
  title bar and close icon and makes it an ordinary window rather than a
  transient one. Menus, tooltips, drop-downs and untitled pop-ups are
  unchanged. A title is the sign: GTK programs only title pop-ups meant
  to be seen as windows.
- **Closing:** the close icon sends the window Escape first, since
  programs that use pop-ups close them on Escape and keep them for reuse
  (mtPaint's Pattern Chooser couldn't be reopened after GTK's default
  delete destroyed it). If the window is still open 250 ms later, it is
  sent delete-event.
- `RISCOS_GTK_PLAIN_POPUPS` turns it off.

## Title bars stay on the screen (2026-10-11)
- **Why:** GTK positions a toplevel by its frame (X window managers put
  the frame's corner where `gtk_window_move` says); riscos-gtk's GDK
  positions the work area, and frame extents are the work area. A window
  moved to (0, 0) had its title bar above the screen.
- **How:** after Wimp_OpenWindow, a window with a title bar reads its
  outline (Wimp_GetWindowOutline); if the outline's top is above the
  screen, the window is opened again lower down and GDK gets a configure
  event. Positions a program saves and restores (`gdk_window_get_root_origin`,
  `gtk_window_move`) stay in work-area terms, so they don't drift.
  Maximise uses the screen below the title bar.

## g_spawn_async runs RISC OS tasks (2026-10-11)
- **Why:** UnixLib's `fork()` is a vfork and `exec()` replaces the
  program until the child ends, so a spawned helper stopped the desktop
  and its output couldn't be read as it came. Geany's Build and Execute
  were left out of the first build because of it.
- **How:** GLib gets hooks (`GRiscosSpawnHooks` in gspawn.c; a child
  watch source in gmain.c for pids the hooks own). GDK sets them when the
  Wimp task starts, so programs without GDK keep UnixLib's behaviour. A
  spawn whose output is read becomes `*TaskWindow "Obey <scrap file>"
  -task &<us> -txt &<pid> -quit`; TaskWindow_Output is written to a
  UnixLib pipe (file-backed, so it works in one process), Morio closes it.
  Other spawns are Wimp_StartTask, ended by Message_TaskCloseDown.
- **Exit status:** TaskWindow doesn't report one, so the Obey file prints
  a marker and `Sys$ReturnCode` last; GDK strips it from the output. No
  marker (the Obey file stopped at an error) is status 1.
- **Not done:** standard input isn't forwarded (TaskWindow_Input could),
  `envp` is ignored, `g_spawn_sync()` still runs in place. `*Dir` is
  global on RISC OS, so a working directory changes the desktop's.
- **Programs:** Geany's patch only changes what is RISC OS specific
  (Execute's Obey file and task window, `kill()`, the terminal setting);
  the spawning itself is GTK's.

## Throwback into the build's output (2026-10-11)
- **Why:** the DDE's tools report errors as `"c.hello", line 3: Error:`,
  which GCC-style parsers (Geany's) don't read, and the RISC OS file name
  isn't the name the program has the file under. Throwback gives the file,
  line and severity exactly.
- **How:** GDK registers with DDEUtils (ThrowbackRegister) only while a
  spawned TaskWindow runs, and unregisters after, so StrongED or Zap get
  throwback the rest of the time. Each ErrorDetails/InfoDetails becomes
  `FILE:LINE: error: TEXT` in the newest running task's output. FILE is
  the UnixLib name: a parent directory in UnixLib$sfix (c, h, s, o...) is
  turned back into an extension, as UnixLib does when a program opens
  `hello.c`. Programs need nothing new: anything that parses compiler
  output gets it.
- **Limits:** with several builds running, throwback goes to the newest.
  The tool's own text messages still appear too.

## The browser's Discs come from the icon bar (2026-10-11)
- **Why:** OS_FSControl 33 names every filing system, including ones with
  nothing to show (no drive, nothing mounted, devices), and `<FS>:$` only
  works for a filing system with a current disc. RISC OS has no call that
  lists drives or network mounts; the Filers know them, and show them as
  icon bar icons.
- **How:** Wimp_WhichIcon on the icon bar (-2) and Wimp_GetIconState for
  each icon; the icon's creator from Wimp_SendMessage (reason 19, R2 = -2,
  R3 = icon: nothing is delivered); its task name (TaskManager) gives the
  filing system ("SDFS Filer", "SCSIFiler", "RAMFS Filer", "LanMan98":
  the longest filing system name it starts with, followed by nothing, a
  space, "FS" or "Filer"); the indirected text and validation string are
  read from that task with Wimp_TransferBlock. Root: `<FS>::<text>.$`,
  checked with OS_FSControl 37 (so a ShareFS "Discs" icon, which isn't a
  disc, is left out); a drive number (`:0`) isn't checked, so a floppy
  drive isn't read until it's opened. ResourceFS's Apps is
  `Resources:$.Apps`.
- **Fallback:** no icon bar icons to read: each filing system whose
  `<FS>:$` canonicalises.

## Linked-in GModule plugins: a registry in GLib (2026-10-17)
- **Problem:** Gnumeric and GOffice find their plugins (function
  libraries, file formats, chart types) by reading `plugin.xml` files and
  then `g_module_open()` the module the file names. RISC OS has no
  `dlopen`, so none loaded: no functions beyond the core, no .xlsx.
- **Choice:** keep the programs' plugin code unchanged and give GLib a
  table of modules linked into the program:
  `g_module_register_static (name, symbols)`. `g_module_open()` checks it
  before the platform: a path matches if it ends in the name at a `/`,
  after removing `.so`, `.la`, `.dll`, `.dylib` or `.sl`, so
  `/<Gnumeric$Dir>/lib/plugins/fn-math/plugin.so` finds `fn-math/plugin`.
  `g_module_symbol()` looks the symbol up in the module's table;
  `g_module_supported()` is true when anything is registered.
- **Name clashes:** every plugin defines `go_plugin_init`,
  `go_plugin_header` and often the same helpers.
  `tools/mkstaticmodules.py` renames each module's global symbols with
  `objcopy --redefine-syms` (prefix `smod_<name>__`), which also renames
  the references between that module's own objects, and writes the
  tables with the original names. Nothing in the plugins' source changes.
- **Rejected:** editing each plugin to register itself (dozens of
  patches, redone for every program); a fake `dlopen` in UnixLib (the
  symbol clash remains, and it would need the same tables).
- **Plugins left out of Gnumeric:** those needing Python, Perl, GDA,
  psiconv, Paradox, GLPK or lp_solve, the sample and hello plugins, and
  `excelplugins` (it loads Windows XLL files); GOffice's `lasem` (no
  lasem library). 34 of Gnumeric's plugins and 9 of GOffice's are in.

## SVG: a stand-in for librsvg (2026-10-17)
- GOffice uses librsvg for SVG pictures in charts and images. librsvg has
  been Rust since 2.42, and there's no Rust for RISC OS. `riscos/rsvg-stub`
  gives the few calls GOffice makes: loading always fails with "SVG
  pictures can't be shown on RISC OS yet", the rest do nothing.

## hicolor lists its standard folders (2026-10-17)
- GTK puts icons added with `gtk_icon_theme_add_resource_path()` into the
  hicolor theme, and looks for them only in the folders hicolor's
  `index.theme` lists. The `index.theme` that `build/mkicons.py` writes
  listed only the program's own folders, so Gnumeric's toolbar icons
  (and GOffice's combo pictures, which then gave criticals) weren't found.
  It now lists freedesktop's standard ones (16 to 512, each context) as
  hicolor-icon-theme does; every program gets it at packaging.

## Reading the icon bar: Filers' text is in the RMA (2026-10-17)
- The browser's Discs come from the icon bar (Wimp_WhichIcon on -2, then
  each icon's indirected text). The Filers (SDFS Filer, RAMFS Filer,
  LanMan98...) are module tasks, so that text is in the RMA, not in an
  application slot; Wimp_TransferBlock from them failed on the Pi, every
  icon was skipped and the fallback (every filing system) was shown.
  Module tasks (TaskManager_EnumerateTasks flag bit 0) are now read
  directly, after OS_Memory 24 says the range is readable in user mode.
- What was read goes to the Log in one line, so a desktop where it still
  fails can be diagnosed from the Log.

## File names are Latin-1 (2026-10-17)
- RISC OS file names are in the desktop's alphabet (Latin-1: a hard space
  is &A0), and UnixLib passes their bytes through. GLib assumed UTF-8, so
  such names were invalid text wherever a program showed them (GTK
  labels warned "Invalid input string"). GLib's patch makes
  `g_get_filename_charsets` give ISO-8859-1, then UTF-8, on RISC OS:
  `g_filename_to_utf8`, `g_filename_display_name` and GTK's file
  choosers convert, through UnixLib's iconv (the Iconv module).
  `G_FILENAME_ENCODING` still overrides it. A name a program wrote in
  UTF-8 shows as the Filer shows it.

## No grab for a dialogue shown as a RISC OS box (2026-10-17)
- A GtkFileChooserDialog is shown as a RISC OS Save or Open box and
  stays hidden. GOffice (and other code) calls `gtk_grab_add` on the
  dialogue after showing it, which sent every event to the hidden
  dialogue: the box's buttons did nothing. `gtk_grab_add` ignores a
  widget that has a box open (`riscos-gtk-box`); the box window has GTK's
  modal grab itself when the dialogue is modal.

## Native child windows stay client-side (2026-10-17)
- The RISC OS GDK's native windows are Wimp windows, top-level only (as
  Broadway's). `gdk_window_ensure_native` on a child window (GOffice's
  sheet widgets, through `gdk_window_set_composited`) reached the
  backend's assertion. It now returns FALSE on RISC OS, as for offscreen
  windows, and the child stays drawn in its top-level.

## Every application carries the MIME database (2026-10-17)
- GIO's content types come from shared-mime-info's database in
  `XDG_DATA_DIRS`; without it every file was `application/octet-stream`
  (Gnumeric ignored dropped spreadsheets; file chooser filters by type
  matched nothing). `build/mkmime.sh` builds it from shared-mime-info 2.4
  with the build machine's `update-mime-database`, and packaging adds
  `share/mime` (about 170 KB): globs, magic, aliases, subclasses. No
  `mime.cache`: GLib's xdgmime would mmap it, which on RISC OS makes a
  dynamic area. RISC OS file types aren't used yet (a name without an
  extension is still octet-stream).

## The heap stays in the WimpSlot (2026-10-13)
- **Checked:** none of the programs sets `__dynamic_da_name`,
  `__dynamic_da_max_size` or `__dynamic_no_da` (nm on each ELF), and
  `!Run` sets no `<prog>$Heap`, so UnixLib (5.0.3.3, `sys/_syslib.s`)
  makes no heap dynamic area: `brk_rw` grows the WimpSlot with
  Wimp_SlotSize. Its EABI dlmalloc has `DEFAULT_MMAP_MAX 0`, so no
  ARMEABISupport `mmap#N` areas. The only OS_DynamicArea call in the
  programs is UnixLib's start-up one, skipped. riscos-mesa only grows the
  screen area, for full-screen surfaces, which GTK doesn't use.
- **Why keep it:** a heap area would reserve its maximum in logical
  address space (all of it on a machine with DALimit 1 GB: what ran out
  on a 4 GB Pi 4 with programs asking 512 MB, found with AddrSpace).
  In the WimpSlot the heap only uses the memory it grows to, at the cost
  of being limited to application space.
- **Earlier text was wrong:** `!Run` comments, PORTING.md and the release
  notes said the heap was in a dynamic area; corrected.
- **Spawned TaskWindows** get 16 MB (`RISCOS_GTK_TASK_WIMPSLOT`): memory
  while the command runs, not address space.
