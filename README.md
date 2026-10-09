# riscos-gtk

GTK 3 for RISC OS: a native RISC OS GDK backend, the GLib/pango/cairo
stack cross-built with GCCSDK, and a RISC OS theme, so that GTK programs
(AbiWord and Gnumeric first) can run as ordinary Wimp tasks.

**Status: release candidate 1 (3.24.41-riscos1.0-rc1), under review.** GTK
programs run on RISC OS as ordinary Wimp tasks:
- **Windows:** Wimp windows with the Wimp's own furniture.
- **Menus:** Wimp menus, opened with Menu anywhere in a window.
- **Files:** the RISC OS Save and Open boxes, drag and drop, and the global
  clipboard.
- **The icon bar:** an icon for every program.
- **Printing:** through !Printers.
- **Look:** taken from the running desktop's colours and sprites.

Two real programs come with it: **!L3afpad**, a text editor, and
**!ClawsMail**, an email client (POP3, IMAP and SMTP, with TLS through
AcornSSL). **!GTKThemeTest** shows every common widget.

- What RC1 contains, requirements and known issues:
  [docs/RELEASE-NOTES.md](docs/RELEASE-NOTES.md)
- The Pi sign-off list: [docs/RC-TEST-PLAN.md](docs/RC-TEST-PLAN.md)
- Building: [docs/BUILDING.md](docs/BUILDING.md)
- What a release must carry under the licences:
  [docs/SOURCE-AND-LICENCES.md](docs/SOURCE-AND-LICENCES.md)

| Milestone | What | State |
|---|---|---|
| M0 | Repo, toolchain, theme | done |
| M1 | Dependency stack: GLib, libpng/FreeType/expat/fontconfig, fribidi, harfbuzz, pixman, cairo, pango, gdk-pixbuf, ATK, libepoxy | done; tested on the Pi (GTKTest) |
| M2 | `gdk/riscos/`: windows, redraw through a sprite, the Wimp_PollIdle main loop | done; tested on the Pi (GTKHello) |
| M3 | Keyboard, Wimp menus, pointer shapes, clipboard, drag and drop, scroll wheel | done; tested on the Pi (GTKThemeTest) |
| M4 | Save/Open boxes, the icon bar, printing, packaging; real programs | done: !L3afpad (tested on the Pi); !ClawsMail (QEMU only, Pi sign-off pending) |
| M5 | Next: a devkit for other developers' GTK programs; AbiWord or Gnumeric | — |

Design decisions and why: [docs/DECISIONS.md](docs/DECISIONS.md). What
UnixLib does and doesn't do that matters here:
[docs/PORTING-NOTES.md](docs/PORTING-NOTES.md).

## Building (short version)

```sh
git clone -b v1.3 https://github.com/adyoull/riscos-crossdev
(cd riscos-crossdev && make prebuilt)
riscos-gtk/build/unixlib-overlay.sh riscos-crossdev riscos-crossdev-ul5033 libunixlib.a   # UnixLib 5.0.3.3
cd riscos-gtk
export RCD_DIR=$PWD/../riscos-crossdev-ul5033
make deps                                   # the libraries
make l3afpad claws themetest                # the programs, in out/
make release RELEASE=3.24.41-riscos1.0-rc1    # programs + complete source + MANIFEST
make check                                  # the QEMU tests
```

The full version, with the host packages and the UnixLib overlay, is in
[docs/BUILDING.md](docs/BUILDING.md).

## Layout

| Path | What |
|---|---|
| `build/` | `env.sh` (settings; `TARGET=riscos` or `linuxarm`), `fetch-sources.sh` + `SHA256SUMS.txt`, `build-deps.sh`, `build-app.sh` (packaging), `build-l3afpad.sh`, `build-claws.sh`, `build-themetest.sh`, `make-release.sh`, `build-linuxarm.sh` (the QEMU tests) |
| `apps/` | The programs' RISC OS parts: `l3afpad/`, `claws/` (start-up, icon bar, app files) |
| `cairo/` | riscos-gtk's addition to cairo (printing as Draw paths) |
| `gtk/riscos/` | riscos-gtk's GTK sources: Wimp menus, the theme from the desktop, Save/Open and Print boxes, the icon bar |
| `riscos/licences/` | The LGPL text and the `Source` notice put in every package |
| `gdk/riscos/` | The RISC OS GDK backend: GTK's Broadway client side, renamed, with `gdkriscos-server.c` doing the Wimp work |
| `patches/` | Changes to the libraries, one diff per library (GTK's adds the `riscos_backend` option) |
| `riscos/intl/`, `riscos/dl/` | A minimal libintl (no translations yet) and a libdl whose dlopen fails |
| `theme/` | The RISCOS GTK 3 theme (see its README) |
| `tests/glib/`, `tests/render/`, `tests/app/` | The M1 tests (GLib; fonts, text and drawing) and the !GTKTest app files |
| `tests/gtk/` | hello.c (!GTKHello) and hellotest.c; themetest.c (!GTKThemeTest) with its fake-Wimp tests; l3afpad-fake.c, claws-fake.c (with `tests/claws/mailserver.py`) |
| `tests/fake/` | Fake RISC OS harnesses for the GDK backend's host tests |

## Licence

Files written for this repo are MIT ([LICENSE](LICENSE)); copied files and
the libraries keep theirs ([LICENCES.md](LICENCES.md)). GLib, pango and GTK
are LGPL and linked statically: each release publishes the complete
source alongside the programs ([docs/SOURCE-AND-LICENCES.md](docs/SOURCE-AND-LICENCES.md)).

## How this was made

Written with Claude (Anthropic's AI) as a coding assistant. Andrew Youll
directs the work, makes the decisions, and tests on a Raspberry Pi 4.
