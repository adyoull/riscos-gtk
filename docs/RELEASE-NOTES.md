# riscos-gtk 3.24.41-riscos1.0 — public beta (release candidate 4)

**Status: public beta.** RC4 is the second public beta build. It adds
support for high-resolution (EX0 EY0, 180 dpi) screen modes. It has been tested on a Raspberry Pi 4, but not yet widely: please
report anything that goes wrong (see "Reporting problems" below). The
final 3.24.41-riscos1.0 will be this build with whatever the beta turns
up fixed. Items marked *[Pi]* still need a check on real hardware
([RC-TEST-PLAN.md](RC-TEST-PLAN.md)).

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

**Version:** `3.24.41-riscos1.0`: the GTK release it's built from
(3.24.41), the riscos-gtk revision (riscos1) and a point release (.0;
fixes to this release become riscos1.1, riscos1.2 ...). Release
candidates add `-rcN`.

## What's in RC4

| Package | Version | What |
|---|---|---|
| `L3afpad-0.21.zip` | L3afpad 0.8.18.1.11, RISC OS port 0.21 | A simple text editor. Each document has its own window; it prints, uses Save/Open boxes, and has a Choices window |
| `ClawsMail-0.10.zip` | Claws Mail 4.4.0, RISC OS port 0.10 | An email client: POP3, IMAP and SMTP, with all TLS through AcornSSL, and local MH folders. Shows HTML mail laid out; links open in your browser (Iris or NetSurf); RISC OS icons. Starts on the icon bar; closing its window leaves it there |
| `GTKThemeTest-0.45.zip` | 0.45 | Every common GTK widget on four pages: a demonstration and a test of the theme and backend |
| `riscos-gtk-3.24.41-riscos1.0-rc4-src.tar` | | The complete source: this repository at the release commit and every upstream tarball (see [SOURCE-AND-LICENCES.md](SOURCE-AND-LICENCES.md)) |
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
- **Memory:** each program takes about 20 MB of WimpSlot, plus its heap
  in a dynamic area.

## Features of the RISC OS backend

**Windows and drawing**
- **Windows:** each GTK window is a Wimp window. GTK draws into a 32bpp
  sprite and the backend plots it.
- **Colours:** right on both TRGB and TBGR screens.
- **High resolution (EX0 EY0, 180 dpi) modes:** windows, text and
  widgets are their usual size and drawn at the screen's resolution
  (GDK scale 2). Desktop sprites are shown at 180 dpi where the desktop
  has them, otherwise doubled. ClawsMail's icons and both programs'
  application icons are drawn at 180 dpi too.
- **Placement:** new windows open centred on the screen; a program's
  next window opens 48 OS units down and to the right of the last.
- **Toggle size:** works, and the window is laid out again at the new
  size.
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
- **One menu per window:** Menu anywhere in a window gives the same menu.
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
- **Drag and drop:** files dropped on windows and fields are loaded.
  Text dragged out of a field becomes a Text file, or goes to another
  program by RAM transfer.
- **Clipboard:** the global clipboard is shared with other programs, in
  Latin-1 or UTF-8.
- **File types:** set from the name's extension; names without one are
  saved as Text in L3afpad.

**The icon bar**
- **Every program has an icon,** as our other ports do. Select shows the
  window; Menu gives Info, Choices... (if the program has settings) and
  Quit.
- **Loading files:** files double-clicked (DataOpen), dropped on the icon
  or saved to it are opened.
- **Unsaved work:** PreQuit asks about it.
- **One copy:** starting a second copy hands its file to the first.

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
- **File and folder icons** come from the desktop's sprites, with GNOME
  Adwaita's icons for everything else.
- **Fonts:** DejaVu Sans, shipped in each package.

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

## Known issues and limits in RC4

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
- **Toolchain:** riscos-crossdev v1.3, with UnixLib 5.0.3.3 added by
  `build/unixlib-overlay.sh`, because no crossdev release carries 5.0.3.3
  yet. The overlay is documented rather than waiting for crossdev 1.4.
- **Packages:** `make deps` builds the libraries, then
  `make release RELEASE=3.24.41-riscos1.0-rc4` builds the packages, the
  source archive and the manifest.

## Tests

Under QEMU (`make check`), the libraries and programs built as ARM Linux
code against a fake RISC OS and Wimp:

| Suite | Checks |
|---|---|
| glibtest | 29 |
| rendertest | 22 |
| hellotest | 15 |
| themetest-fake | 38 |
| edittest-fake | 44 |
| toolsprites-fake | 3 |
| printvec-fake | 7 |
| l3afpad-fake | 44 |
| claws-fake | 36 |
| template-fake (the devkit's template; at EIG 1 and EIG 0) | 9 + 9 |
| gltest-fake (GtkGLArea on riscos-mesa's RISC OS libraries; at EIG 1 and EIG 0) | 9 + 9 |
| gltest-nogl (GtkGLArea without riscos-mesa linked in) | 2 |

All pass at the RC commit.

On the Pi: the sign-off list is [RC-TEST-PLAN.md](RC-TEST-PLAN.md).

## Thanks and licences

- **GTK, GLib, pango, cairo and their libraries:** LGPL and MIT-style.
- **L3afpad:** GPL 2, by Tarot Osuji and Steven Honeyman.
- **Claws Mail:** GPL 3, by the Claws Mail team.
- **libetpan:** BSD-style, by DINH Viet Hoa.
- **litehtml:** BSD-style, by Yuri Kobets (in Claws Mail's LiteHTML
  viewer, GPL 3).
- **gumbo:** Apache License 2.0, Google and contributors.
- **Nettle:** LGPL 3 / GPL 2.
- **Adwaita icons:** LGPL 3 / CC-BY-SA 3.0.
- **DejaVu fonts:** their own free licence.
- **riscos-gtk's own files:** MIT, including ClawsMail's RISC OS icons.

Details: [../LICENCES.md](../LICENCES.md) and
[SOURCE-AND-LICENCES.md](SOURCE-AND-LICENCES.md).
