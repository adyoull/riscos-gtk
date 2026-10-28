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
| `tools/riscosdraw.py` (the shared drawing), `tools/mkriscosicons.py`, `tools/riscosicons_apps.py` and the icons they draw (including !Gnumeric's and !Geany's own icon replacements) (the RISCOS theme's own: every app's `share/icons/RISCOS`, listed in its `README`) | Written for riscos-gtk: original icons in the style of the RISC OS desktop | MIT |
| `tools/mkclawsicons.py` and the icons it draws (`apps/claws/app/share/claws-mail/themes/RISCOS`, including the `@2` pictures, `apps/claws/app/!Sprites` and `!Sprites11`) | Written for riscos-gtk: original icons in the style of the RISC OS desktop | MIT |
| `tools/mkl3afpadsprites.py` | Written for riscos-gtk | MIT |
| `patches/geany-2.1-riscos.diff`, `apps/geany/*` (riscos-geany.c, the app files; Licence is Geany's COPYING, GPL-2), `build/build-geany.sh` | Changes to and RISC OS start-up for [Geany](https://www.geany.org/) 2.1 (the Geany contributors) | GPL version 2 or later, as Geany |
| `apps/geany/app/!Sprites`, `!Sprites11` (made by `tools/mkgeanysprites.py`, MIT) | Geany's icon, `icons/scalable/geany.svg` | GPL version 2 or later, as Geany |
| `gdk/riscos/gdkfiles-riscos.c` (the directory calls, the icon bar's discs), `gdk/riscos/gdkspawn-riscos.c` (programs started as tasks), the GLib patch's spawn hooks | Written for riscos-gtk; part of GTK's backend | LGPL version 2.1 or later |
| `tests/gtk/browser-fake.c`, `tests/gtk/geany-fake.c`, `tools/mkgeanysprites.py` | Written for riscos-gtk | MIT |
| `patches/mtpaint-3.50.14-riscos.diff` (including its RISC OS file picker in `src/fpick.c`), `apps/mtpaint/*` (riscos-mtpaint.c, the app files; Licence is mtPaint's COPYING, GPL-3), `build/build-mtpaint.sh` | Changes to and RISC OS start-up for [mtPaint](https://mtpaint.sourceforge.net/) 3.50.14 (Mark Tyler, Dmitry Groshev) | GPL version 3 or later, as mtPaint |
| `apps/mtpaint/app/!Sprites`, `!Sprites11` (made by `tools/mkmtpaintsprites.py`, MIT) | mtPaint's icon, `doc/mtpaint.png` | GPL version 3 or later, as mtPaint |
| `tests/gtk/menuclick-fake.c`, `tests/gtk/spawn-fake.c`, `tests/gtk/mtpaint-fake.c`, `tests/gtk/mkpng.py`, `tools/mkmtpaintsprites.py` | Written for riscos-gtk | MIT |
| `patches/gnumeric-1.12.57-riscos.diff`, `apps/gnumeric/*` (riscos-gnumeric.c, the app files; Licence is Gnumeric's COPYING with the GPL 2 and 3 texts), `build/build-gnumeric.sh` | Changes to and RISC OS start-up for [Gnumeric](http://www.gnumeric.org/) 1.12.57 (the Gnumeric developers) | GPL version 2 or 3, as Gnumeric |
| `apps/gnumeric/app/!Sprites`, `!Sprites11` (made by `tools/mkgnumericsprites.py`, MIT) | Gnumeric's icon, `icons/*/apps/org.gnumeric.gnumeric.png` | GPL version 2 or 3, as Gnumeric |
| `patches/goffice-0.10.57-riscos.diff` | Changes to [GOffice](https://gitlab.gnome.org/GNOME/goffice) 0.10.57 (the GOffice developers) | GPL version 2 or 3, as GOffice |
| libxml2 2.9.14, libxslt 1.1.39 (built by `build/build-gnumeric.sh`, linked into !Gnumeric; not in this repository) | [libxml2, libxslt](https://gitlab.gnome.org/GNOME/libxml2), Daniel Veillard and others | MIT |
| libgsf 1.14.51 (built by `build/build-gnumeric.sh`, linked into !Gnumeric; not in this repository) | [libgsf](https://gitlab.gnome.org/GNOME/libgsf), the GNOME project | LGPL version 2.1 |
| The apps' `share/mime` (made by `build/mkmime.sh`, MIT, from shared-mime-info 2.4's `freedesktop.org.xml`; not otherwise in this repository) | [shared-mime-info](https://gitlab.freedesktop.org/xdg/shared-mime-info), freedesktop.org (its COPYING goes with it as `Licence`) | GPL version 2 or later |
| `tools/mkstaticmodules.py`, `tools/mkgnumerichelp.py`, `tests/render/svgtest.c`, `tests/rol/featureprobe.c`, `tools/mkgnumericsprites.py`, `tests/gtk/gnumeric-fake.c`, `tests/gtk/staticmodule/*` | Written for riscos-gtk | MIT |
| The GLib patch's static module registry (`g_module_register_static`, in `gmodule/`) | Written for riscos-gtk; part of GLib | LGPL version 2.1 or later, as GLib |
| `gdk/riscos/gdkglcontext-riscos.c`, `.h` | Written for riscos-gtk, based on GTK's `gdk/wayland/gdkglcontext-wayland.c`; part of GTK's backend | LGPL version 2.1 or later |
| `gdk/riscos/gdkfontconfig-riscos.c`, `gdk/riscos/gdkschemas-riscos.c`; `gdk/riscos/gdkschemas-riscos.h` (generated by `tools/mkgtkschemas.sh` from GTK's gtk/org.gtk.Settings.*.gschema.xml) | Written for riscos-gtk; the schemas are GTK's | LGPL version 2.1 or later, as GTK |
| `tools/mkgtkschemas.sh`, `tests/gtk/schemas-fake.c`, `tests/gtk/fontconfig-fake.c` | Written for riscos-gtk | MIT |
| `tests/fake/mesa-shim.c` | Adapted from [riscos-mesa](https://github.com/adyoull/riscos-mesa)'s `tests/host-harness/mesa/arm/shim.c` | MIT |
| `tests/gtk/gltest.c`, `gltest.h`, `gltest-fake.c`, `tests/gtk/gltest-app/*`, `build/build-gltest.sh` | Written for riscos-gtk | MIT |
| `sdk/*` (the devkit's `env.sh`, `bin/` scripts, `riscos-gtk/app.h`, CMake toolchain file, `Source` template, documentation, the template program and `examples/glarea`, with their app files), `build/make-sdk.sh`, `tools/mktemplatesprites.py` and the template's sprites | Written for riscos-gtk | MIT |
| `apps/l3afpad/app/!Sprites11` (made by mkl3afpadsprites.py) | L3afpad's icon, `data/l3afpad_original.svg` (Tarot Osuji, Steven Honeyman) | GPL version 2 or later, as L3afpad |
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

## The devkit

`riscos-gtk-sdk-<release>.tar.gz` (build/make-sdk.sh) holds:

- **The libraries below,** as static libraries with their headers. It
  leaves out the ones only ClawsMail uses: libetpan, Nettle and gumbo.
- **riscos-gtk's own devkit files,** under MIT.
- **What an application is packaged with:**
  - the RISCOS theme (MIT);
  - Adwaita's icons (LGPL 3 or CC-BY-SA 3.0);
  - the DejaVu fonts, with their licence;
  - the LGPL text.

PThreadTicker is copied into applications from riscos-crossdev's
toolchain, with its own licence file. The libraries are LGPL and
statically linked, so the devkit is published with riscos-gtk's source
archive for the same release (docs/SOURCE-AND-LICENCES.md). The devkit's
`docs/README.md` says what a developer must ship with a program.

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
| Noto Color Emoji 2.051, only in the optional `TTF-Noto-Color-Emoji` package (build/build-emoji.sh; the font file from the noto-emoji repository's v2.051 tag) | SIL Open Font Licence 1.1 (in the package's `RiscPkg/Copyright` and `!UnixFont.doc.noto-color-emoji.LICENSE`) |
| riscos-mesa (Mesa 20.3.5's OSMesa and riscos-mesa's EGL; its devkit), only in programs that link it for OpenGL (!GTKGLTest) | MIT-style: riscos-mesa's LICENCES.txt, shipped in the program as LicenceMesa |
| UnixLib 5.0.3.3 (riscos-crossdev 1.4/1.5, built from the riscos-unixlib v5.0.3.3 release) and GCC's runtime (from riscos-crossdev) | see riscos-crossdev's LICENCES.md |

**Static linking and the LGPL.** GLib, pango, GTK, ATK, gdk-pixbuf,
fribidi and cairo are LGPL and are linked statically (UnixLib has no
dlopen). LGPL 2.1 section 6 is met by publishing, with each release, the
complete source: this repository at the release commit plus every upstream
tarball (`build/make-release.sh` makes `riscos-gtk-<release>-src.tar`),
from which every program, open source itself, can be rebuilt against a
changed library. Each package carries `LicenceLGPL` and a `Source` file
saying where that archive is. Details: docs/SOURCE-AND-LICENCES.md.
