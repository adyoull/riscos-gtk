# RISC OS theme for GTK 3 (revision 2, 2026-10-03)

Imported into riscos-gtk from RISCOS-gtk3-theme.zip (sha256 9b32281c…d11).

A from-scratch GTK 3.24 theme (no Adwaita inheritance) that makes window
contents look like the RISC OS 5 desktop. Title bars and window furniture are
not themed: the planned RISC OS GDK backend gives each toplevel native Wimp
furniture (see gtk-riscos-port-feasibility.md).

## Contents
- `gtk-3.0/gtk.css`: the theme. Colours are the 16 Wimp colours as @wimp0..@wimp15 tokens.
- `gtk-3.0/assets/`: PNG option/radio icons and arrows, 1x and 2x. PNG only, so no librsvg is needed on RISC OS.
- `mkassets.py`: regenerates the assets (Python + Pillow).
- `demo.py`, `shoot.sh`: an Export-dialog demo and an Xvfb screenshot script (modes: dialog, list, menu, popup).

## Try it on Linux (no RISC OS port needed)
    cp -r RISCOS ~/.themes/
    GTK_THEME=RISCOS gtk3-widget-factory
    GTK_THEME=RISCOS python3 demo.py        # modes: dialog, list, menu
Recommended settings.ini: gtk-font-name=DejaVu Sans 10, gtk-enable-animations=false,
gtk-dialogs-use-header=false, gtk-overlay-scrolling=false.

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
- Fonts: DejaVu Sans (shipped with the port, as Warzone 2100 does) stands in for Homerton. The revision 2 screenshots were taken with Nimbus Sans (URW), which is not shipped.
- Header bars are styled as plain toolbars; apps that put window controls in them will look wrong.
