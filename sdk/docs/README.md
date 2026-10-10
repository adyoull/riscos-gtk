# riscos-gtk devkit

The riscos-gtk devkit is everything you need to build your own GTK 3
programs for RISC OS, without building riscos-gtk itself. It includes:

- GTK 3.24.41 with riscos-gtk's RISC OS backend;
- GLib, pango, cairo, gdk-pixbuf, ATK and their libraries, as static
  libraries, with headers and pkg-config files;
- a script that packages your program as a RISC OS application;
- a template program to start from.

Programs built with it run as ordinary Wimp tasks:

- Wimp windows with the Wimp's own furniture;
- Wimp menus;
- the RISC OS Save and Open boxes;
- drag and drop and the global clipboard;
- an icon on the icon bar;
- printing through !Printers;
- the desktop's own colours and sprites;
- 180 dpi (EX0 EY0) screen modes.

You don't need any RISC OS-specific code for most of this.

`VERSION` says which riscos-gtk release and commit this devkit was built
from.

## What you need

- **A Linux machine** (x86_64; WSL works) with bash, GNU make, python3
  and pkg-config. A GTK program that uses GResources or GSettings also
  needs GLib's host tools (`glib-compile-resources`,
  `glib-compile-schemas`: Debian/Ubuntu's `libglib2.0-dev-bin`).
- **riscos-crossdev v1.3**, the GCCSDK cross-compiler, with UnixLib
  5.0.3.3 added. The devkit's libraries are built against 5.0.3.3, and
  no crossdev release carries it yet. Take `libunixlib.a` from the
  riscos-unixlib v5.0.3.3 release, then use the devkit's
  `bin/unixlib-overlay.sh` to make a copy of the toolchain that links it:

  ```sh
  git clone -b v1.3 https://github.com/adyoull/riscos-crossdev
  (cd riscos-crossdev && make prebuilt)
  riscos-gtk-sdk-<release>/bin/unixlib-overlay.sh riscos-crossdev riscos-crossdev-ul5033 libunixlib.a
  ```
- **On the RISC OS machine:** RISC OS 5, in a 16 or 32 bpp screen mode,
  with SharedUnixLibrary 1.16 or later, ARMEABISupport and Iconv from
  PackMan.

## Quick start

```sh
tar -xzf riscos-gtk-sdk-<release>.tar.gz
export RCD_DIR=$PWD/riscos-crossdev-ul5033
. riscos-gtk-sdk-<release>/env.sh
cp -r riscos-gtk-sdk-<release>/template hello && cd hello
make
```

This builds `out/!Hello` and `out/Hello-1.00.zip`. Copy the zip to the
RISC OS machine and unpack it with SparkFS or !InfoZip; the file types
are kept. Double-click !Hello, then click Select on its icon bar icon.

## Building your own program

- **One file or a few:**

  ```sh
  $CC $RISCOS_GTK_CFLAGS myprog.c -o myprog.elf $RISCOS_GTK_LIBS
  riscos-gtk-package MyProg myprog.elf 1.00 app out
  ```

  `app/` holds your application's own files. Copy the template's:
  `!Run`, `!Boot`, `!Help`, `!Sprites`, `!Sprites11` and `Licence`.
- **pkg-config:** after `env.sh`, `pkg-config` finds the devkit's
  libraries, not the host's. Link with `--static`:
  `pkg-config --cflags gtk+-3.0` and `pkg-config --libs --static gtk+-3.0`.
- **autotools:**

  ```sh
  ./configure --host=arm-riscos-gnueabihf --disable-shared
  ```

  CC, CFLAGS, LDFLAGS and pkg-config's variables are already set.
- **meson:**

  ```sh
  meson setup build --cross-file "$(riscos-gtk-meson-cross)"
  ```
- **CMake:**

  ```sh
  cmake -DCMAKE_TOOLCHAIN_FILE=$RISCOS_GTK_SDK/share/riscos-gtk/riscos-gtk.cmake
  ```

  Find GTK with `pkg_check_modules(GTK REQUIRED gtk+-3.0)`, and use the
  static variables: `${GTK_STATIC_INCLUDE_DIRS}`,
  `${GTK_STATIC_CFLAGS_OTHER}`, `${GTK_STATIC_LIBRARY_DIRS}` and
  `${GTK_STATIC_LIBRARIES}`.

Then run riscos-crossdev's checks on the linked ELF:
`$RCD_DIR/checks/check-all.sh myprog.elf`. These check static linking,
stack probes, the FPU and UnixLib. `riscos-gtk-package` runs them for
you.

## What's here

| Path | What |
|---|---|
| `env.sh` | Source it: the compiler, pkg-config, `RISCOS_GTK_CFLAGS`, `RISCOS_GTK_LIBS` |
| `bin/` | Compiler wrappers, `riscos-gtk-package`, `riscos-gtk-meson-cross`, `unixlib-overlay.sh` |
| `sysroot/riscos-gtk/` | Headers, static libraries, pkg-config files, aclocal macros |
| `share/riscos-gtk/` | What packaging puts in an application: theme, icons, schemas, fonts, licences |
| `template/` | The template program |
| `docs/API.md` | riscos-gtk's RISC OS functions: the icon bar, files, printing, the desktop |
| `docs/PORTING.md` | Porting an existing GTK program: what works, what doesn't, and why |
| `docs/LICENCES.md` | The licences of everything in the devkit |

## Licences: what you must ship

GTK and its libraries are under the LGPL version 2.1, and the devkit
links them statically. So with any program you distribute you must:

- **Include the LGPL** and say where the libraries' source is.
  `riscos-gtk-package` adds `LicenceLGPL` and a `Source` file for you,
  pointing at the riscos-gtk source archive for this release.
- **Let people relink your program** with changed libraries. Either
  publish your program's source, or make its object files available.
  Set `PROGRAM_SOURCE_URL` when packaging, and `Source` will say where
  they are.

Your own code can be under any licence that allows this. The template
is MIT. Fonts, icons and PThreadTicker carry their own licences, which
are copied into the application.

## Reporting problems

Report problems in the riscos-gtk GitHub issues
(https://github.com/adyoull/riscos-gtk/issues). Please include:

- the program's Log, `<Wimp$ScrapDir>.<Program>.Log`;
- your machine, RISC OS version and screen mode.
