# Licences

Files written for riscos-gtk are under the MIT licence ([LICENSE](LICENSE)):
the theme (`theme/`), `riscos/intl`, the tests, `build/build-linuxarm.sh`,
`build/build-glibtest.sh`, the Makefile and the documentation. Files copied
or adapted from other projects keep their original licence, listed below.

## Copied or adapted files

| File(s) | From | Licence |
|---|---|---|
| `build/env.sh`, `build/build-deps.sh`, `build/fetch-sources.sh` | [riscos-warzone2100](https://github.com/adyoull/riscos-warzone2100) `build/` (2.3.9-12 tree) | GPL version 2 or later |
| `tests/fake/mesa-egl/*`, `tests/fake/sdl-wimp/*` | [riscos-mesa](https://github.com/adyoull/riscos-mesa) `tests/host-harness/` (20.3.5-10 tree) | MIT (riscos-mesa's own files) |
| `gdk/riscos/*` except `gdkriscos-server.c`, `gdktransfer-riscos.c`, `gdkpointer-riscos.c`, `gdkwimptheme-riscos.c`, `gdkriscoswimp.h`, `gdkriscosmenu.h` and `meson.build` | GTK 3.24.41 `gdk/broadway/` (client side), renamed broadway → riscos, with riscos-gtk's changes | LGPL version 2.1 or later, as GTK |
| `gdk/riscos/gdkriscos-server.c`, `gdk/riscos/gdktransfer-riscos.c`, `gdk/riscos/gdkpointer-riscos.c`, `gdk/riscos/gdkwimptheme-riscos.c`, `gdk/riscos/gdkriscoswimp.h`, `gdk/riscos/gdkriscosmenu.h`, `gdk/riscos/meson.build`, `gtk/riscos/*` (Wimp menus, the Wimp theme) | Written for riscos-gtk; part of GTK and its backend, so under GTK's licence | LGPL version 2.1 or later |
| `patches/gtk+3.0-3.24.41-riscos.diff` | Changes to GTK | LGPL version 2.1 or later, as GTK |
| `patches/glib-2.80.0-riscos.diff` | Changes to GLib | LGPL version 2.1 or later, as GLib |
| `patches/fontconfig-2.14.1-riscos.diff` | riscos-warzone2100 `patches/fontconfig/`; changes to fontconfig | fontconfig's licence (MIT-style) |
| `patches/gdk-pixbuf-2.42.10-riscos.diff` | Changes to gdk-pixbuf | LGPL version 2.1 or later, as gdk-pixbuf |
| `patches/libepoxy-1.5.10-riscos.diff` | Changes to libepoxy | MIT, as libepoxy |

The build scripts' licence covers the scripts only. Programs and libraries
built with them are not affected by it: their licences come from their
own sources and the libraries they link (below).

## Libraries the build uses

None of these is in the repo: `build/fetch-sources.sh` downloads them by
checksum. A RISC OS program built here links them statically.

| Library | Licence |
|---|---|
| GLib 2.80.0 (glib, gobject, gio, gmodule, gthread) | LGPL version 2.1 or later |
| PCRE2 10.42 | BSD (3-clause) |
| libffi 3.4.6 | MIT |
| zlib 1.3.1 | zlib licence |
| libpng 1.6.37 | libpng licence |
| libjpeg-turbo 2.1.5 | IJG licence and BSD (3-clause) |
| FreeType 2.10.1 | FreeType licence (BSD-style), or GPL version 2 |
| expat 2.2.9, fontconfig 2.14.1, pixman 0.42.2, libepoxy 1.5.10 | MIT / MIT-style |
| fribidi 1.0.13 | LGPL version 2.1 or later |
| harfbuzz 8.3.0 | MIT ("Old MIT") |
| cairo 1.18.0 | LGPL version 2.1 or MPL 1.1 |
| pango 1.52.1, gdk-pixbuf 2.42.10, ATK 2.36.0, GTK 3.24.41 | LGPL version 2.1 or later |
| DejaVu fonts 2.37 | Bitstream Vera / Arev licence (free, redistributable) |
| UnixLib 5.0.3.1 and GCC's runtime (from riscos-crossdev) | see riscos-crossdev's LICENCES.md |

**Static linking and the LGPL.** GLib (and later pango, GTK, ATK, cairo
parts) are LGPL. A program that links them statically may be distributed
only if users can relink it against a modified library, so a release must
ship the program's object files and a link script (a "relink kit") with
the sources of the LGPL libraries and our changes to them. UnixLib has no
dlopen, so shared libraries aren't an option today. Test builds (such as
!GLibTest) are covered by shipping the sources and this repo.
