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
| `patches/glib-2.80.0-riscos.diff` | Changes to GLib | LGPL version 2.1 or later, as GLib |

## Libraries the build uses

None of these is in the repo: `build/fetch-sources.sh` downloads them by
checksum. A RISC OS program built here links them statically.

| Library | Licence |
|---|---|
| GLib 2.80.0 (glib, gobject, gio, gmodule, gthread) | LGPL version 2.1 or later |
| PCRE2 10.42 | BSD (3-clause) |
| libffi 3.4.6 | MIT |
| zlib 1.3.1 | zlib licence |
| libpng, FreeType, expat, fontconfig (next, as riscos-warzone2100) | libpng licence; FreeType licence (BSD-style) or GPL 2; MIT; MIT-style |
| DejaVu fonts 2.37 | Bitstream Vera / Arev licence (free, redistributable) |
| UnixLib 5.0.3.1 and GCC's runtime (from riscos-crossdev) | see riscos-crossdev's LICENCES.md |

**Static linking and the LGPL.** GLib (and later pango, GTK, ATK, cairo
parts) are LGPL. A program that links them statically may be distributed
only if users can relink it against a modified library, so a release must
ship the program's object files and a link script (a "relink kit") with
the sources of the LGPL libraries and our changes to them. UnixLib has no
dlopen, so shared libraries aren't an option today. Test builds (such as
!GLibTest) are covered by shipping the sources and this repo.
