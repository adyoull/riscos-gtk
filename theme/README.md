# RISC OS theme for GTK 3 (revision 3, 2026-10-04)

Imported into riscos-gtk from RISCOS-gtk3-theme.zip (sha256 9b32281c…d11).

A from-scratch GTK 3.24 theme (no Adwaita inheritance) that makes window
contents look like the RISC OS 5 desktop. Title bars and window furniture are
not themed: the planned RISC OS GDK backend gives each toplevel native Wimp
furniture (see gtk-riscos-port-feasibility.md).

## Contents
- `gtk-3.0/gtk.css`: the theme. Colours are the 16 Wimp colours as @wimp0..@wimp15 tokens.
- `gtk-3.0/assets/`: PNG option/radio icons and arrows, 1x and 2x. PNG only, so no librsvg is needed on RISC OS.
- `mkassets.py`: regenerates the assets (Python + Pillow).
- `demo.py`, `shoot.sh`: an Export-dialog demo and an Xvfb screenshot script (modes: dialog, list, menu, popup, factory). shoot.sh uses a private HOME with the settings the RISC OS backend gives every program, so the shots show what the Pi should show.

## Try it on Linux (no RISC OS port needed)
    mkdir -p ~/.themes/RISCOS && cp -r gtk-3.0 ~/.themes/RISCOS/
    GTK_THEME=RISCOS gtk3-widget-factory
    GTK_THEME=RISCOS python3 demo.py        # modes: dialog, list, menu, popup
    ./shoot.sh dialog list menu popup factory   # the screenshots
settings.ini to match RISC OS: gtk-font-name=DejaVu Sans 12, gtk-xft-dpi=92160
(90 dpi), gtk-enable-animations=false, gtk-dialogs-use-header=false,
gtk-overlay-scrolling=false.

On RISC OS no settings file is needed: riscos-gtk's GDK backend returns
these as the desktop's settings (gdk/riscos/gdkscreen-riscos.c), so every
GTK program gets the RISCOS theme, DejaVu Sans 12 at 90 dpi and no
animations. A program can still override them with GtkSettings.

## Sizes
RISC OS sizes are in OS units; the desktop runs at 2 OS units per pixel
(EIG 1), and the backend sets 90 dpi so that point sizes match RISC OS
(12pt = 15 pixels). The theme uses the Style Guide's heights:
| RISC OS | OS units | pixels | GTK |
|---|---|---|---|
| action button (R5) | 52 | 26 | button |
| default action button in its channel (R6) | 68 | 34 | button.default |
| writable icon (R7), display field (R2) | 52 | 26 | entry, label.display-field |
| option / radio row | 44 | 22 | checkbutton, radiobutton |
| menu entry | 44 | 22 | menu menuitem |
| scroll bar width | 40 | 20 | scrollbar |
| list row | 44 | ~23 | treeview row |

## Mapping to RISC OS
| RISC OS | GTK |
|---|---|
| action button (slab out, slab in when pressed) | button |
| default action button (sunken channel) | button.default |
| writable icon (white, slab in, red caret) | entry |
| number range with adjuster arrows | spinbutton |
| option / radio icons | check / radio |
| labelled box | frame |
| pop-up (display field + menu button with right arrow) | combobox |
| display field (grey, slabbed in) | entry.display-field, label.display-field (opt-in class) |
| Wimp menu ticks (bare tick, no box) | check/radio menu items |
| Wimp menu (white, inverted highlight, dotted separators) | menu |
| scroll bar (grey well, arrows at both ends) | scrollbar |
| interactive help (cream) | tooltip |

## Revision 3.2 (2026-10-05)
- The default button's channel is a box-shadow only, with no margin: the
  margin came and went as GTK restyled the default button when it took
  focus, and everything above the button row jumped. Leave 4px round a
  default button in layouts.

## Revision 3.1 (2026-10-04)
- Menu hover rules use child selectors (`menuitem:hover > label`): GTK 3
  makes a submenu's widgets CSS descendants of the item it hangs from, so
  the highlighted menu bar title made its whole menu white on white. The
  theme test (tests/gtk/themetest.c) shows every widget; on Linux:
  `THEMETEST_QUIT=2000 ./themetest "" /dev/null PAGE` with GTK_THEME=RISCOS.

## Revision 3 changes (RISC OS 5 default look)
- Font and sizes: DejaVu Sans 12 at 90 dpi for Homerton.Medium 12pt (the
  RISC OS desktop default; set by the backend); heights from the Style Guide
  (table above).
- Buttons and fields: R5 action buttons 26px with 12px side padding; the R6
  default button sits in a proper 4px channel (grey well, dark top-left,
  white bottom-right); writables and display fields 26px; adjuster arrows
  18px; pop-ups match the field height; labelled-box labels in the normal
  weight.
- Radio icons were lit from the wrong side (dark top-right): now slab in,
  dark top-left and light bottom-right like every other 3D item.
- Scroll bars as the Wimp's own: mid-grey well (Wimp colour 3, the window
  scroll outer colour), light-grey raised slider (colour 1), raised arrow
  buttons that press in, a black line parting the bar from the work area,
  20px wide.
- Menus as Wimp menus: 1-pixel black outline, no top/bottom padding, 22px
  entries indented as the Wimp does, short-cuts in the entry's colour (white
  when highlighted, grey when shaded), dotted separators.
- Lists: 23px rows (ScrollList pitch), 2px header slabs, black/white
  selection.
- All 16 Wimp colours defined (@wimp0..@wimp15).
- Keyboard focus outlines stay: GTK shows them only after Tab is used, so a
  mouse user sees focus only as the red caret, as on RISC OS.

## Revision 2 changes
- Option and radio icons redrawn: square edges are pixel-exact at 1x and 2x; radio rings sized in whole pixels.
- Mixed ("inconsistent") states for options and radios.
- Check and radio menu items show a bare tick or dot, as Wimp menus do.
- Combo boxes look like Toolbox pop-ups: a display field with a separate menu button.
- New `display-field` class (GTK 3 CSS can't tell when an entry is non-editable).
- Hover feedback removed throughout; the Wimp has none.
- Default button keeps its channel when pressed; linked button groups share an edge.
- Switch I/O glyphs hidden.

## Known gaps
- Combo menus open over the field, GTK-style. A RISC OS pop-up opens to the right of its menu button; that's positioning code for the GDK backend, not CSS.
- GtkSwitch has no RISC OS equivalent; it is drawn as a slab in a well.
- Icons still come from the icon theme (Adwaita). A RISC OS icon set is separate work.
- Fonts: DejaVu Sans (shipped with the port, as Warzone 2100 does) stands in for Homerton; it is a little wider. RISC OS outline fonts can't be read by FreeType.
- Menus have no title bar (GTK menus have none); real Wimp menus are planned for the backend (M3).
- Option and radio icons are still 16px; RISC OS 5's own sprites are a little larger at 12pt.
- Pitfall: adding `-GtkTreeView-horizontal-separator` to the treeview rule made GTK 3.24.41 silently fall back to Adwaita (the only sign is a warning about Adwaita's bullet-symbolic.svg). If shoot.sh prints that warning, the theme was not used.
- Header bars are styled as plain toolbars; apps that put window controls in them will look wrong.

## On RISC OS: the desktop's colours and sprites (riscos-gtk 0.11)
When a GTK program starts on RISC OS, gtk/riscos/gtkriscostheme.c adds
CSS above this theme: @wimp0-@wimp15 become the desktop's Wimp colours,
and check/radio icons, the combo box button and the spin button arrows
use the Wimp's own sprites (optoff, opton, radiooff, radioon, gright,
left, right). So write colours in this theme as @wimpN, not literals, or
they won't follow the desktop. RISCOS_GTK_WIMP_THEME=0 turns it off.
