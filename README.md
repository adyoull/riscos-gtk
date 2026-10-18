# riscos-gtk

GTK 3 for RISC OS: a native RISC OS GDK backend, the GLib/pango/cairo
stack cross-built with GCCSDK, and a RISC OS theme, so that GTK programs
(AbiWord and Gnumeric first) can run as ordinary Wimp tasks.

**Status: public beta (3.24.41-riscos1.0-rc7).** GTK
programs run on RISC OS as ordinary Wimp tasks:
- **Windows:** Wimp windows with the Wimp's own furniture.
- **Menus:** Wimp menus, opened with Menu anywhere in a window.
- **Files:** the RISC OS Save and Open boxes, or a Filer-like directory
  browser with a sidebar of discs and places; drag and drop, and the
  global clipboard.
- **The icon bar:** an icon for every program.
- **Other programs:** commands a program starts run as RISC OS tasks (a
  TaskWindow when it reads their output; the DDE's throwback too).
- **Printing:** through !Printers.
- **OpenGL:** `GtkGLArea` draws with riscos-mesa (OpenGL 2.1 or ES 2.0,
  rendered on the processor) in programs linked with it.
- **Look:** taken from the running desktop's colours and sprites.

Two real programs come with it: **!L3afpad**, a text editor, and
**!ClawsMail**, an email client (POP3, IMAP and SMTP, with TLS through
AcornSSL; HTML mail; links open in Iris or NetSurf). **!Geany**, the
programmer's editor (Geany 2.1), is a demo of a large program built
against the devkit, **!mtPaint** a painting program (mtPaint 3.50.14), and
**!Gnumeric** a spreadsheet (Gnumeric 1.12.57, new in RC7). **!GTKThemeTest** shows every common widget, and
**!GTKGLTest** a GtkGLArea.

- What RC7 contains, requirements, known issues and how to report problems:
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
| M4 | Save/Open boxes, the icon bar, printing, packaging; real programs | done: !L3afpad and !ClawsMail, both tested on the Pi |
| M5 | The devkit for other developers' GTK programs (`sdk/`, `make sdk`): headers, static libraries, pkg-config, packaging, a template program, API reference and porting guide; then AbiWord or Gnumeric | devkit built; tested by building the template from the devkit alone (make, meson, CMake) and on the fake desktop |

Design decisions and why: [docs/DECISIONS.md](docs/DECISIONS.md). What
UnixLib does and doesn't do that matters here:
[docs/PORTING-NOTES.md](docs/PORTING-NOTES.md).

## Building (short version)

```sh
git clone -b v1.8.2 https://github.com/adyoull/riscos-crossdev
(cd riscos-crossdev && make prebuilt && make devkit)   # UnixLib 5.0.3.3; riscos-mesa for GL
cd riscos-gtk
export RCD_DIR=$PWD/../riscos-crossdev
make deps                                   # the libraries
make l3afpad claws themetest geany mtpaint gnumeric  # the programs, in out/
make release RELEASE=3.24.41-riscos1.0-rc7    # programs + devkit + complete source + MANIFEST
make check                                  # the QEMU tests
```

The full version, with the host packages, is in
[docs/BUILDING.md](docs/BUILDING.md).

## Layout

| Path | What |
|---|---|
| `build/` | `env.sh` (settings; `TARGET=riscos` or `linuxarm`), `fetch-sources.sh` + `SHA256SUMS.txt`, `build-deps.sh`, `build-app.sh` (packaging), `build-l3afpad.sh`, `build-claws.sh`, `build-themetest.sh`, `build-gltest.sh`, `make-release.sh`, `make-sdk.sh` (the devkit), `build-linuxarm.sh` (the QEMU tests) |
| `apps/` | The programs' RISC OS parts: `l3afpad/`, `claws/` (start-up, icon bar, app files) |
| `cairo/` | riscos-gtk's addition to cairo (printing as Draw paths) |
| `gtk/riscos/` | riscos-gtk's GTK sources: Wimp menus, the theme from the desktop, Save/Open and Print boxes, the icon bar |
| `riscos/licences/` | The LGPL text and the `Source` notice put in every package |
| `gdk/riscos/` | The RISC OS GDK backend: GTK's Broadway client side, renamed, with `gdkriscos-server.c` doing the Wimp work |
| `patches/` | Changes to the libraries, one diff per library (GTK's adds the `riscos_backend` option) |
| `riscos/intl/`, `riscos/dl/` | A minimal libintl (no translations yet) and a libdl whose dlopen fails |
| `theme/` | The RISCOS GTK 3 theme (see its README) |
| `tests/glib/`, `tests/render/`, `tests/app/` | The M1 tests (GLib; fonts, text and drawing) and the !GTKTest app files |
| `tests/gtk/` | hello.c (!GTKHello) and hellotest.c; themetest.c (!GTKThemeTest) with its fake-Wimp tests; l3afpad-fake.c, claws-fake.c (with `tests/claws/mailserver.py`), template-fake.c (the devkit's template); gltest.c (!GTKGLTest) and gltest-fake.c, with riscos-mesa's libraries |
| `tests/fake/` | Fake RISC OS harnesses for the GDK backend's host tests; `mesa-shim.c` lets riscos-mesa's RISC OS libraries run in them |
| `sdk/` | The devkit's own files: `env.sh`, `bin/` (compiler wrappers, `riscos-gtk-package`, `riscos-gtk-meson-cross`), `include/riscos-gtk/app.h`, `docs/` (README, API, PORTING), `template/` (a small program to start from); `build/make-sdk.sh` puts them with the libraries |

## Licence

Files written for this repo are MIT ([LICENSE](LICENSE)); copied files and
the libraries keep theirs ([LICENCES.md](LICENCES.md)). GLib, pango and GTK
are LGPL and linked statically: each release publishes the complete
source alongside the programs ([docs/SOURCE-AND-LICENCES.md](docs/SOURCE-AND-LICENCES.md)).

## How this was made

Written with Claude (Anthropic's AI) as a coding assistant. Andrew Youll
directs the work, makes the decisions, and tests on a Raspberry Pi 4.
