# Building riscos-gtk

Everything is cross-built on Linux. The result is statically linked
RISC OS programs in `out/`. The steps below are the ones the release
candidate was built with; `MANIFEST.txt` in a release records the commit,
the toolchain and the UnixLib.

## 1. The host

This was built on Ubuntu 24.04, x86-64.

```sh
sudo apt-get install meson ninja-build pkg-config autoconf automake libtool \
    libltdl-dev gettext gperf cmake python3-packaging python3-pil \
    libglib2.0-dev-bin libgtk-3-bin \
    gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

- `libglib2.0-dev-bin` provides glib-compile-schemas.
- `libgtk-3-bin` provides gtk-encode-symbolic-svg, used for the Adwaita
  icons.
- The `arm-linux-gnueabihf` compilers are only needed for `make check`.

## 2. The toolchain: riscos-crossdev 1.3, with UnixLib 5.0.3.3

```sh
git clone -b v1.3 https://github.com/adyoull/riscos-crossdev
(cd riscos-crossdev && make prebuilt)
```

The release links **UnixLib 5.0.3.3**, which no riscos-crossdev release
carries yet (1.3 has 5.0.3.2). The public headers are unchanged, so only
the library is swapped:
1. Take `libunixlib.a` from the riscos-unixlib v5.0.3.3 release; its
   sha256 is `e24f16c3…`.
2. Make an overlay copy of the toolchain with it:

```sh
riscos-gtk/build/unixlib-overlay.sh riscos-crossdev riscos-crossdev-ul5033 libunixlib.a
export RCD_DIR=$PWD/riscos-crossdev-ul5033
```

`build/env.sh` reports which UnixLib a build links, and `make-release.sh`
records it.

When a riscos-crossdev release with 5.0.3.3 exists, use it directly and
drop the overlay.

## 3. The sources and libraries

```sh
git clone https://github.com/adyoull/riscos-gtk     # or unpack the release's source tar
cd riscos-gtk
make deps
```

`make deps` runs two scripts:
- `build/fetch-sources.sh` puts the upstream tarballs in `dl/` and checks
  them against `build/SHA256SUMS.txt`. A release's source tar already has
  them in `dl/`.
- `build/build-deps.sh` builds and patches the libraries into
  `sysroot/riscos-gtk`: zlib, libffi, PCRE2, GLib, libpng, FreeType,
  expat, fontconfig, libjpeg-turbo, fribidi, harfbuzz, pixman, cairo,
  pango, gdk-pixbuf, ATK, libepoxy, then GTK with the RISC OS backend.

Each library's changes are one diff in `patches/`. riscos-gtk's own GTK
and GDK sources are in `gdk/riscos/` and `gtk/riscos/`, and cairo's in
`cairo/`.

Nettle and libetpan aren't built here: `build/build-claws.sh` builds them
into the same sysroot the first time it runs.

## 4. The programs and the release

```sh
make l3afpad themetest claws                 # out/<Program>-<version>.zip
make release RELEASE=3.24.41-riscos1.0-rc2     # out/release/3.24.41-riscos1.0-rc2/
```

`make release` runs `build/make-release.sh`:
- **It refuses a tree with uncommitted changes.**
- **It builds the three programs.** Their versions are set in
  `build/build-l3afpad.sh`, `build-themetest.sh` and `build-claws.sh`.
- **It runs riscos-crossdev's checks on each program:** that it is
  static, has stack probes, uses only VFPv3, matches UnixLib, and has no
  unaligned casts in the source.
- **It writes three more things:**
  - `riscos-gtk-<release>-src.tar`: the repository at the commit, plus
    every upstream tarball;
  - `MANIFEST.txt`: the files' SHA-256, the commit, the toolchain and the
    UnixLib.

## 5. The tests (QEMU)

```sh
export RCD_DIR=$PWD/../riscos-crossdev    # crossdev's own UnixLib is fine here
make check
```

This builds the same libraries and programs as ARM Linux code (in
`src-linuxarm/` and `sysroot-linuxarm/`), linked against a fake RISC OS
and Wimp (`tests/fake/`). It runs each test under riscos-crossdev's
`qemu-arm-aligntrap`, which also stops on any unaligned access.

The Claws Mail test starts `tests/claws/mailserver.py`, a local POP3,
SMTP and IMAP server. Every suite must pass before a release.

## Known build issues

- **The toolchain's `.la` files:** riscos-crossdev 1.3's `.la` files
  (libm, libstdc++, libunixlib) name paths from the machine that built
  them (`/opt/b13/...`). libtool reads them when it links through the
  C++ driver. `build-claws.sh` links libetpan as C to avoid this. It will
  be reported to riscos-crossdev for 1.4.
- **Older toolchains:** `env.sh` stops if the toolchain is older than
  riscos-crossdev 1.3 (UnixLib 5.0.3.2, static only).
