# Decisions

From the feasibility study (2026-09-24) and the owner's choices since.

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

## Owner's choices (2026-10-03)
- Folder `RiscOS/GTK/`, repo `riscos-gtk`, rules as the other ports
  (git bundles delivered, the owner pushes; handoffs outside git).
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
Owner's choice: the stock RISC OS 5/Pi look (3D grey, Homerton, standard
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
Owner's choice ("context menu how RISC OS does, middle click anywhere").
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
Wimp sprite later if the owner prefers the system look.
Update (0.9, owner): the shapes use the desktop's pointer colours (palette
kept), and the text pointer is the Wimp's ptr_write when it exists.

## The theme takes the desktop's Wimp colours and sprites (2026-10-05)
Owner's choice over real Wimp icons as widgets (which would fight GTK's
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
Owner: 0.18's box "looks nothing like a RISC OS saving DnD dialogue";
source given: the Toolbox manual (riscos.com, ResEd and chapter 14,
SaveAs). So the box copies the SaveAs object's default window: draggable
file_xxx sprite, writable filename (centred, caret at the end), Cancel and
Save (default) action buttons; no Selection option (GTK has no such
notion); title bar only. Transient (Wimp_CreateMenu) when opened by a
click or from a menu, static from the keyboard, as the Toolbox does; Save
with a leaf name reports the module's own error. GDK window furniture now
follows GTK's decorations/functions/resizable/type hint, so other GTK
dialogue boxes lose close/back/size icons the same way.

## Theme checked against the Toolbox manual (2026-10-06)
Owner asked to confirm the theme against riscos.com's Toolbox manual; its
gadget pictures (chapter 16) were measured pixel by pixel. Owner then:
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
Owner's choice (of: desktop sprites + fallback / Adwaita only / a new
RISC OS-style set). Names RISC OS has a picture for come from the Wimp
sprite pool at start-up, like the rest of the theme; everything else
from a trimmed Adwaita 3.36 (PNG). The list of names is
theme/icons-list.txt.

## L3afpad, the first real program; Menu over a text view (2026-10-06)
L3afpad (tiny, GTK 3 only, GtkFileChooserDialog, text view, menus) was
ported as !L3afpad. RISC OS runs one copy of a program, so "New" empties
the window instead of starting another copy. Its window is all text view,
so the program's menu would be unreachable with Menu (the view's own
context menu would take the click): in a window with a menu, Menu over a
multi-line text view now gives the program's menu, as RISC OS editors do;
single-line fields keep their Cut/Copy/Paste menu. Printing is off until
GTK has a RISC OS print route.
