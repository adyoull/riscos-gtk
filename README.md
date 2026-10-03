# riscos-gtk

GTK 3 for RISC OS: a native RISC OS GDK backend, the GLib/pango/cairo
stack cross-built with GCCSDK, and a RISC OS theme, so that GTK programs
(AbiWord and Gnumeric first) can run as ordinary Wimp tasks.

**Status: early.** The theme is done (tested on Linux GTK 3.24.41). All of
GTK 3's dependencies cross-build (GLib 2.80, cairo 1.18, pango 1.52,
harfbuzz 8.3, gdk-pixbuf 2.42, ATK, libepoxy and the font stack). GLib's
test passes on a Pi (29/29); GLib and the drawing stack pass under QEMU.
No GDK backend yet.

| Milestone | What | State |
|---|---|---|
| M0 | Repo, toolchain, theme | done |
| M1 | Dependency stack: GLib, then libpng/FreeType/expat/fontconfig, fribidi, harfbuzz, pixman, cairo, pango, gdk-pixbuf, ATK, libepoxy | all built; GLib tested on the Pi, drawing stack on QEMU |
| M2 | `gdk/riscos/`: windows, redraw through a sprite, the Wimp_PollIdle main loop | — |
| M3 | Keyboard, popups as Wimp menus, pointer shapes, clipboard | — |
| M4 | Native file chooser, drag and drop, packaging; AbiWord | — |

Design decisions and why: [docs/DECISIONS.md](docs/DECISIONS.md). What
UnixLib does and doesn't do that matters here:
[docs/PORTING-NOTES.md](docs/PORTING-NOTES.md).

## Building (Ubuntu 24.04, x86-64)

```sh
git clone -b v1.2 https://github.com/adyoull/riscos-crossdev
cd riscos-crossdev && make prebuilt && cd ..
git clone https://github.com/adyoull/riscos-gtk     # next to riscos-crossdev
cd riscos-gtk
sudo apt-get install meson ninja-build pkg-config autoconf automake libtool \
    libltdl-dev gettext gperf cmake python3-packaging \
    gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
make deps        # sources by checksum, then the libraries into sysroot/
make tests       # out/GTKTest-0.2.zip, for the Pi
make check       # the same tests as ARM Linux code under qemu-arm-aligntrap
```

riscos-crossdev's `docs/SETUP.md` lists what its toolchain and QEMU need.

## Layout

| Path | What |
|---|---|
| `build/` | `env.sh` (settings; `TARGET=riscos` or `linuxarm`), `fetch-sources.sh` + `SHA256SUMS.txt`, `build-deps.sh`, `build-tests.sh` (!GTKTest), `build-linuxarm.sh` (the QEMU tests) |
| `patches/` | Changes to the libraries, one diff per library |
| `riscos/intl/`, `riscos/dl/` | A minimal libintl (no translations yet) and a libdl whose dlopen fails |
| `riscos/fixincludes/` | Corrected UnixLib headers (see each file) |
| `theme/` | The RISCOS GTK 3 theme (see its README) |
| `tests/glib/`, `tests/render/`, `tests/app/` | The M1 tests (GLib; fonts, text and drawing) and the !GTKTest app files |
| `tests/fake/` | Fake RISC OS harnesses for the GDK backend's host tests |

## Licence

Files written for this repo are MIT ([LICENSE](LICENSE)); copied files and
the libraries keep theirs ([LICENCES.md](LICENCES.md)). GLib, pango and GTK
are LGPL: see LICENCES.md for what static linking means for releases.

## How this was made

Written with Claude (Anthropic's AI) as a coding assistant. Andrew Youll
directs the work, makes the decisions, and tests on a Raspberry Pi 4.
