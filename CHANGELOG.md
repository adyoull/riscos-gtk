# Changelog

## Unreleased (print residue: !L3afpad 0.8, GTKThemeTest 0.32, 2026-10-07)
- **Printing left the Print box's area unredrawn** over the window and the
  pinboard (owner's photo). The print job stayed selected while GTK went
  back to Wimp_Poll between pages, so the redraws the Wimp asked for went
  to the printer driver instead of the screen. The job is now suspended
  (PDriver_SelectJob 0) except while a page prints and at EndJob/AbortJob.
- Fake printer: SelectJob tracks the selected job; Wimp_Poll counts polls
  made while a job is selected. l3afpad-fake has 35 checks, adding "no
  Wimp_Poll while the job is selected" (a mutation without the suspend
  gives 7).

## Unreleased (printing: !L3afpad 0.7, GTKThemeTest 0.31, 2026-10-06)
- **Printing** through RISC OS's printer drivers (owner's choices: the
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
- **Relinked against UnixLib 5.0.3.3** (owner's request; riscos-unixlib
  handoff 2026-10-06-all-ports-unixlib-5.0.3.3: fork with threads, waits
  in read/write/stdio (eventfd, used by GLib, can be cancelled), getserv*,
  heap gaps, swprintf, exit codes). No riscos-crossdev release has it yet,
  so (owner's choice) `build/unixlib-overlay.sh` makes a copy of crossdev
  1.3 with the v5.0.3.3 release's libunixlib.a (sha256 e24f16c3…, checked);
  the public headers are the same as 5.0.3.2's, so only the link changes.
  `build/env.sh` sets UNIXLIB_VERSION from the library's checksum and
  notes when it isn't 5.0.3.3. Every program checked to contain 5.0.3.3's
  `__pthread_held_wait`. !Help files say "riscos-crossdev 1.3 and UnixLib
  5.0.3.3". PThreadTicker stays 0.03.

## !L3afpad 0.5, GTKThemeTest 0.29 (2026-10-06)
- **Windows open in the middle of the screen** (owner: the application and
  Choices windows should). A toplevel GTK hasn't placed (no position
  asked for: GTK_WIN_POS_NONE, no gtk_window_move, so no GDK_HINT_POS /
  USER_POS) is centred on the screen the first time it's shown, then
  reopens where it was (gdkwindow-riscos.c, `placed`). Windows GTK places
  keep their place: Save/Open boxes at the pointer, message dialogues on
  their parent. `RISCOS_GTK_CENTRE=0` turns it off.
- Tests: l3afpad-fake 27 (L3afpad's window and the Choices window open
  centred).

## !L3afpad 0.4, GTKThemeTest 0.28 (2026-10-06)
- **The icon bar done as our other RISC OS programs do it** (owner: 0.3's
  was "nothing like RISC OS"; looked at Reel, !FFmpeg's front end, YTDL,
  NetStats, RDPClient):
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
- **The icon bar** (owner: "GTK apps are missing an icon bar entry").
  Owner's choices: every program gets one automatically, with hooks; a
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
- **Options > Font crashed !L3afpad** (owner's Log: "Unknown type
  PangoFontFamily specified in treemodel model", then an EMT trap).
  GtkBuilder finds types it hasn't seen by looking up `*_get_type` in the
  program with GModule, which a statically linked RISC OS program can't
  do; GTK's own types are covered by gtk_test_register_all_types, but the
  font chooser's list store names Pango types. GTK patch (gtkbuilder.c):
  on RISC OS, a table of the Pango, GdkPixbuf and GIO types builder files
  name (PangoFontFamily/Face, GAppInfo, GIcon, GFile, GMenu...).
- **Size grip** (owner: "the draggable corner in the bottom right"). The
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
- 0.24 confirmed on the Pi (owner's photo of the Log: no warnings left,
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
- Owner's 0.23 Log: icons from sprites 25 of 25 (every Wimp sprite looked
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
- Icons (owner's choice: the desktop's sprites first, a shipped set for
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
  pixel by pixel. Owner: keep the theme's colours (only layout and sizes
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
- Noted, not changed (colours, owner's choice): the manual's default
  button channel is cream, labelled boxes are light grey/white, slider
  bars grey. GTK 3 paints box-shadows in list order (last on top), so the
  default button shows only its outer grey ring; left as it is.

## Unreleased (GTKThemeTest 0.20, 2026-10-06)
- Owner: 0.19's Save box shape and size were wrong (wide and short). Now
  laid out from measurements of the Toolbox manual's picture of the
  SaveAs box (1 pixel = 2 OS units): work area 155 x 148, icon 34 x 34
  centred 6 from the top, writable 26 high inset 18/17, the Selection
  option (shaded: GTK has no selection to save) 6 in, Cancel 64 x 28 and
  Save 56 x 28 (default channel round it) along the bottom, 10 below.
  Widths grow only if the desktop font needs it.

## Unreleased (GTKThemeTest 0.19, 2026-10-06)
- Owner: 0.18 works, but the Save box looked nothing like RISC OS's.
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
- Owner: 0.16's scroll bars are correct on the Pi (tool sprites through
  the pointer table). "All sheets (mixed)" didn't change when clicked:
  GTK keeps a check button inconsistent until the program clears it, and
  themetest didn't. It now clears it on the first toggle (mixed -> on ->
  off), as a tri-state option should behave.

## Unreleased (GTKThemeTest 0.16, 2026-10-05)
- Owner's 0.15 Log: Wimp_ReadSysInfo 9's block: +16 and +20 both
  &201A3058, which points to a table of addresses &2047xxxx (the sprites,
  elsewhere); +24 &234, +28 &20, +32 an address inside the block. New
  fourth way: the table of sprite pointers at +16's address (count from
  +4). Debug also prints the first table entries as sprites.
- The fake Wimp's tool block is now laid out as the Pi's (header, +16/+20
  -> table -> sprites outside the block); the suite takes that way.

## Unreleased (GTKThemeTest 0.15, 2026-10-05)
- Owner's 0.14 Log: Wimp sprites 7 of 7 (pool via Wimp_BaseOfSprites;
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
- Owner's 0.13 Log: "Wimp sprites 0 of 7; window border sprites 0 of 27";
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
- Owner on 0.12: "still doesnt use the RISC OS widgets". The theme now
  always writes one line to the program's log: Wimp colours read or not,
  Wimp sprites found (of 7), window border sprites found (of 27), and
  whether the PNGs could be written (and where), to see on the Pi which
  step fails.

## Unreleased (GTKThemeTest 0.12, 2026-10-05)
- Owner (photo, 0.11): GTK's scroll bars didn't follow the desktop's
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
- Owner: redo the theming with the Wimp's sprites read. At start-up
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
- Owner (0.9: "cursors look good"): the scroll wheel did nothing. Windows
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
- Owner: pointers in the RISC OS pointer colours, and the text pointer
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
- Owner: GTK programs should use menus as RISC OS does, Menu anywhere.
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
- Theme toward the RISC OS 5 default desktop (owner's choice: fonts and
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
- GTKTest 0.5 on the owner's Pi (UnixLib 5.0.3.2-rc2): glibtest 29/29,
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
  passes riscos-crossdev's checks; on the owner's Pi: 29/29.
- `tests/fake/`: riscos-mesa's fake Wimp/screen and event harnesses, for
  the GDK backend's host tests.
