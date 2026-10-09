# Licences

Files written for riscos-gtk are under the MIT licence ([LICENSE](LICENSE)):
the theme (`theme/`), `riscos/intl`, the tests, `build/build-linuxarm.sh`,
`build/build-glibtest.sh`, the Makefile and the documentation. Files copied
or adapted from other projects keep their original licence, listed below.

## Copied or adapted files

| File(s) | From | Licence |
|---|---|---|
| `build/env.sh`, `build/build-deps.sh`, `build/fetch-sources.sh` | [riscos-warzone2100](https://github.com/adyoull/riscos-warzone2100) `build/` (2.3.9-12 tree) | GPL version 2 or later |
| `tests/data/riscos-free-icons/*` (test data: PNGs of RISC OS 5 sprites) | [marutan/riscos-free-icons](https://github.com/marutan/riscos-free-icons), (c) RISC OS Developments Ltd | Apache License 2.0 |
| `tests/fake/mesa-egl/*`, `tests/fake/sdl-wimp/*` | [riscos-mesa](https://github.com/adyoull/riscos-mesa) `tests/host-harness/` (20.3.5-10 tree) | MIT (riscos-mesa's own files) |
| `gdk/riscos/*` except `gdkriscos-server.c`, `gdktransfer-riscos.c`, `gdkpointer-riscos.c`, `gdkwimptheme-riscos.c`, `gdkriscoswimp.h`, `gdkriscosmenu.h` and `meson.build` | GTK 3.24.41 `gdk/broadway/` (client side), renamed broadway → riscos, with riscos-gtk's changes | LGPL version 2.1 or later, as GTK |
| `gdk/riscos/gdkriscos-server.c`, `gdk/riscos/gdktransfer-riscos.c`, `gdk/riscos/gdkpointer-riscos.c`, `gdk/riscos/gdkwimptheme-riscos.c`, `gdk/riscos/gdkriscoswimp.h`, `gdk/riscos/gdkriscosmenu.h`, `gdk/riscos/meson.build`, `gtk/riscos/*` (Wimp menus, the Wimp theme, Save and Open boxes) | Written for riscos-gtk; part of GTK and its backend, so under GTK's licence | LGPL version 2.1 or later |
| `theme/icons-list.txt`, the apps' `share/icons/RISCOS` (made by `build/mkicons.py`, MIT) | [Adwaita icon theme](https://gitlab.gnome.org/GNOME/adwaita-icon-theme) 3.36.1, GNOME Project (the licence files go with the icons) | LGPL version 3 or CC-BY-SA 3.0 |
| `patches/l3afpad-0.8.18.1.11-riscos.diff`, `apps/l3afpad/*` (config.h, riscos-l3afpad.c, the app files; Licence is the GPL-2 text) | Changes to [L3afpad](https://github.com/stevenhoneyman/l3afpad) 0.8.18.1.11 (Tarot Osuji, Steven Honeyman); its icon made into !Sprites | GPL version 2 or later, as L3afpad |
| `patches/claws-mail-4.4.0-riscos.diff` (including `src/common/ssl_acornssl.c` and the `gnutls-acornssl` headers), `apps/claws/*` (riscos-claws.c, objects.mk, unaligned-safe.txt, the app files; Licence is Claws Mail's COPYING, GPL-3; LicenceEtPan libetpan's COPYRIGHT) | Changes to [Claws Mail](https://www.claws-mail.org/) 4.4.0 (the Claws Mail team) | GPL version 3 or later, as Claws Mail |
| libetpan 1.9.4 (built by `build/build-claws.sh`, linked into !ClawsMail; not in this repository) | [libetpan](https://github.com/dinhvh/libetpan), DINH Viet Hoa | BSD-style (its COPYRIGHT, shipped as LicenceEtPan) |
| Claws Mail's litehtml viewer plug-in and the litehtml library in it (in Claws Mail's tarball; built by `build/build-claws.sh`, linked into !ClawsMail) | Claws Mail team; [litehtml](http://www.litehtml.com/), Yuri Kobets | Plug-in GPL 3 or later; litehtml BSD-style (its LICENSE, shipped as LicenceLiteHTML) |
| `apps/claws/litehtml/http.h`, `http.cpp` (the viewer's image loader, replacing its libcurl one) | Written for riscos-gtk, from the plug-in's http.cpp interface | GPL version 3 or later, as Claws Mail |
| gumbo 0.12.0 (built by `build/build-claws.sh`, linked into !ClawsMail; not in this repository) | [gumbo-parser](https://codeberg.org/gumbo-parser/gumbo-parser), Google and contributors | Apache License 2.0 (its COPYING, shipped as LicenceGumbo) |
| `tools/mkclawsicons.py` and the icons it draws (`apps/claws/app/share/claws-mail/themes/RISCOS`, `apps/claws/app/!Sprites`) | Written for riscos-gtk: original icons in the style of the RISC OS desktop | MIT |
| `riscos/licences/LGPL-2.1.txt` | The GNU LGPL 2.1 text (Free Software Foundation), copied into every package as LicenceLGPL | Verbatim copying permitted |
| `tests/claws/mailserver.py`, `tests/gtk/claws-fake.c`, `tests/gtk/claws-fake-claws.c` | Written for riscos-gtk | MIT |
| The AcornSSL section of `tests/fake/mesa-egl/fake_riscos.c` (test code, not in any package) | Adapted from [riscos-ffmpeg](https://github.com/adyoull/riscos-ffmpeg)'s `tests/host/fake_acornssl.c` | GPL version 2 or later, as riscos-ffmpeg |
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
| UnixLib 5.0.3.3 (riscos-unixlib release) and GCC's runtime (from riscos-crossdev) | see riscos-crossdev's LICENCES.md |

**Static linking and the LGPL.** GLib, pango, GTK, ATK, gdk-pixbuf,
fribidi and cairo are LGPL and are linked statically (UnixLib has no
dlopen). LGPL 2.1 section 6 is met by publishing, with each release, the
complete source: this repository at the release commit plus every upstream
tarball (`build/make-release.sh` makes `riscos-gtk-<release>-src.tar`),
from which every program, open source itself, can be rebuilt against a
changed library. Each package carries `LicenceLGPL` and a `Source` file
saying where that archive is. Details: docs/SOURCE-AND-LICENCES.md.
