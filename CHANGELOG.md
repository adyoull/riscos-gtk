# Changelog

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
