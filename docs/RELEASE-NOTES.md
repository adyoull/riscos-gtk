# riscos-gtk 3.24.41-riscos1 — release candidate 1

**Status: candidate for review.** These notes describe what RC1 contains and
what still has to happen before it is released. Items marked
*[decision]* need the maintainer's choice; *[Pi]* needs sign-off on real
hardware ([RC-TEST-PLAN.md](RC-TEST-PLAN.md)).

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

**Version:** `3.24.41-riscos1` names the GTK release it's built from and
the riscos-gtk revision, as riscos-ffmpeg and riscos-mesa do *[decision]*.

## What's in RC1

| Package | Version | What |
|---|---|---|
| `L3afpad-0.18.zip` | L3afpad 0.8.18.1.11, RISC OS port 0.18 | A simple text editor. Each document has its own window; it prints, uses Save/Open boxes, and has a Choices window |
| `ClawsMail-0.5.zip` | Claws Mail 4.4.0, RISC OS port 0.5 | An email client: POP3, IMAP and SMTP, with all TLS through AcornSSL, and local MH folders. Starts on the icon bar; closing its window leaves it there |
| `GTKThemeTest-0.42.zip` | 0.42 | Every common GTK widget on four pages: a demonstration and a test of the theme and backend |
| `riscos-gtk-3.24.41-riscos1-rc1-src.tar` | | The complete source: this repository at the release commit and every upstream tarball (see [SOURCE-AND-LICENCES.md](SOURCE-AND-LICENCES.md)) |
| `MANIFEST.txt` | | SHA-256 of each file, the commit, and the toolchain and UnixLib used |

GTKHello and GTKTest are development tests and are not part of the
release *[decision]*.

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

**Network** (ClawsMail)
- **TLS:** all of it goes through AcornSSL: pop3s, imaps, smtps and
  STARTTLS. AcornSSL checks server certificates and asks the user about
  any it can't verify.
- **IMAP** runs in a thread of its own, so the desktop keeps going while
  it talks to the server.

## Known issues and limits in RC1

- **Pi sign-off still needed for:**
  - the size-grip / outside-scroll-bar rule (L3afpad 0.15–0.18,
    GTKThemeTest 0.39–0.42);
  - most of ClawsMail: 0.4 has been started on the Pi (fixes below),
    but POP3, IMAP and SMTP against real servers through AcornSSL have not
    been signed off *[Pi]*.
- **Fixed after the first look at ClawsMail 0.4 on the Pi:**
  - two size icons at first start (the old one stayed in the window's
    corner until something redrew it): GTK now redraws the corner when
    the grip moves to the outside scroll bar;
  - it opened its main window at start: it now starts on the icon bar,
    and Select opens the window (the first time only, after the set-up
    wizard, the window opens by itself);
  - closing the main window quit it: it now hides the window and stays
    on the icon bar; Quit is on the icon bar menu and in File.
- **RISC OS theme changes** take effect at the next start of a program
  (RISC OS themes only change on a reboot anyway).
- **GTK's Page Setup dialogue** isn't replaced: the paper size comes from
  the printer driver.
- **No translations:** messages are in English only.
- **Each program carries its own copy of the libraries,** about 20 MB.
- **ClawsMail:**
  - no plug-ins, spell checking or Claws Mail certificate manager;
  - news (NNTP) is built but untested;
  - IMAP with STARTTLS takes the same code path as imaps but hasn't been
    tested against a server;
  - attachments can't yet be dragged out to the Filer or opened with RISC
    OS programs;
  - web links open nothing.
- **ClawsMail's settings,** and its mailbox by default, live in
  `<Choices$Write>.ClawsMail`.
- **Warnings in Claws Mail's Log:** a few harmless GTK warnings appear
  during start-up (a title set before its window exists; button sizes).

## Building

[BUILDING.md](BUILDING.md). In short:
- **Toolchain:** riscos-crossdev v1.3, with UnixLib 5.0.3.3 added by
  `build/unixlib-overlay.sh`, because no crossdev release carries 5.0.3.3
  yet *[decision: release with the overlay documented, or wait for
  crossdev 1.4]*.
- **Packages:** `make deps` builds the libraries, then
  `make release RELEASE=3.24.41-riscos1-rc1` builds the packages, the
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
| claws-fake | 18 |

All pass at the RC commit.

On the Pi: the sign-off list is [RC-TEST-PLAN.md](RC-TEST-PLAN.md).

## Thanks and licences

- **GTK, GLib, pango, cairo and their libraries:** LGPL and MIT-style.
- **L3afpad:** GPL 2, by Tarot Osuji and Steven Honeyman.
- **Claws Mail:** GPL 3, by the Claws Mail team.
- **libetpan:** BSD-style, by DINH Viet Hoa.
- **Nettle:** LGPL 3 / GPL 2.
- **Adwaita icons:** LGPL 3 / CC-BY-SA 3.0.
- **DejaVu fonts:** their own free licence.
- **riscos-gtk's own files:** MIT.

Details: [../LICENCES.md](../LICENCES.md) and
[SOURCE-AND-LICENCES.md](SOURCE-AND-LICENCES.md).
