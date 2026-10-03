# riscos-gtk

GTK 3 for RISC OS: a native RISC OS GDK backend, the GLib/pango/cairo
stack cross-built with GCCSDK, and a RISC OS theme, so that GTK programs
(AbiWord and Gnumeric first) can run as ordinary Wimp tasks.

**Status: early.** The theme is done (tested on Linux GTK 3.24.41). GLib
2.80 cross-builds and passes its test under QEMU; the first RISC OS test
(!GLibTest) is waiting for a run on a Pi. No GDK backend yet.

| Milestone | What | State |
|---|---|---|
| M0 | Repo, toolchain, theme | done |
| M1 | Dependency stack: GLib, then libpng/FreeType/expat/fontconfig, fribidi, harfbuzz, pixman, cairo, pango, gdk-pixbuf, ATK, libepoxy | GLib built and tested on QEMU |
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
    libltdl-dev gettext python3-packaging gcc-arm-linux-gnueabihf
make deps        # sources by checksum, then the libraries into sysroot/
make glibtest    # out/GLibTest-0.1.zip, for the Pi
make check       # the same test as ARM Linux code under qemu-arm-aligntrap
```

riscos-crossdev's `docs/SETUP.md` lists what its toolchain and QEMU need.

## Layout

| Path | What |
|---|---|
| `build/` | `env.sh` (settings), `fetch-sources.sh` + `SHA256SUMS.txt`, `build-deps.sh` (RISC OS), `build-linuxarm.sh` (QEMU test build), `build-glibtest.sh` |
| `patches/` | Changes to the libraries, one diff per library |
| `riscos/intl/` | A minimal libintl (no translations yet) |
| `theme/` | The RISCOS GTK 3 theme (see its README) |
| `tests/glib/` | The M1 GLib test and its !GLibTest app files |
| `tests/fake/` | Fake RISC OS harnesses for the GDK backend's host tests |

## Licence

Files written for this repo are MIT ([LICENSE](LICENSE)); copied files and
the libraries keep theirs ([LICENCES.md](LICENCES.md)). GLib, pango and GTK
are LGPL: see LICENCES.md for what static linking means for releases.

## How this was made

Written with Claude (Anthropic's AI) as a coding assistant. Andrew Youll
directs the work, makes the decisions, and tests on a Raspberry Pi 4.
