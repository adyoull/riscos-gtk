# Building riscos-gtk

Everything is cross-built on Linux. The result is statically linked
RISC OS programs in `out/`. The steps below are the ones the release
candidate was built with; `MANIFEST.txt` in a release records the commit,
the toolchain and the UnixLib.

## 1. The host

This was built on Ubuntu 24.04, x86-64.

```sh
sudo apt-get install meson ninja-build pkg-config autoconf automake libtool \
    gperf cmake curl unzip xz-utils python3-packaging python3-pil \
    libglib2.0-dev-bin shared-mime-info libgdk-pixbuf2.0-bin \
    gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

- `libglib2.0-dev-bin` provides glib-compile-resources and
  glib-compile-schemas; `shared-mime-info` update-mime-database
  (`build/mkmime.sh`); `python3-pil` the icon drawing;
  `libgdk-pixbuf2.0-bin` gdk-pixbuf-pixdata, which GTK's build uses for
  its resources.
- The `arm-linux-gnueabihf` compilers are only needed for `make check`.
- `build/check-host.sh` (run by `make deps` and `make check`) says what's
  missing.
- GTK's host tools aren't needed: riscos-gtk draws its symbolic icons
  itself (it used gtk-encode-symbolic-svg, `libgtk-3-bin`, before).

## 2. The toolchain: riscos-crossdev 1.8.2 (UnixLib 5.0.3.3)

The release riscos-gtk is built with is named in `.riscos-deps`
(`riscos-crossdev v1.8.2`).

```sh
git clone -b v1.8.2 https://github.com/adyoull/riscos-crossdev
(cd riscos-crossdev && make prebuilt && make devkit)
export RCD_DIR=$PWD/riscos-crossdev
```

Or in riscos-crossdev's container image, which has the toolchain, the
devkit and the host packages but three (`apt-get install python3-pil
shared-mime-info libgdk-pixbuf2.0-bin` first, as the workflows do):

```sh
docker run --rm -v "$PWD":/src ghcr.io/adyoull/riscos-crossdev:1.8.2 make deps tests hello check sdk
```

- **`make prebuilt`:** riscos-crossdev 1.8.2's toolchain is 1.4's: GCCSDK
  GCC 10.2 with **UnixLib 5.0.3.3**, built from the riscos-unixlib
  v5.0.3.3 release's `unixlib-riscos.diff`. It is static only, with
  PThreadTicker 0.03.
- **`make devkit`:** riscos-mesa's devkit (20.3.5-16 in crossdev 1.8.2),
  in `riscos-crossdev/devkit/env`. Only !GTKGLTest and the OpenGL test
  need it.
- **UnixLib check:** `build/env.sh` stops a RISC OS build whose
  toolchain doesn't link UnixLib 5.0.3.3 (crossdev 1.3 and older), and
  `make-release.sh` records the UnixLib in the manifest.
- **History:** before crossdev 1.4, riscos-gtk built with crossdev 1.3
  and the 5.0.3.3 library swapped in by `build/unixlib-overlay.sh`. That
  script is gone.

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

Nettle, libetpan and gumbo aren't built here: `build/build-claws.sh`
builds them into the same sysroot the first time it runs. It also builds
Claws Mail's litehtml viewer (HTML mail) from Claws Mail's own tarball and
links it in, with `apps/claws/litehtml/http.cpp` in place of its libcurl
image loader.

ClawsMail's RISC OS icons are in the repository
(`apps/claws/app/share/claws-mail/themes/RISCOS`, `apps/claws/app/!Sprites`).
`tools/mkclawsicons.py` draws them (needs Pillow); run it again after
changing a drawing:

```sh
python3 tools/mkclawsicons.py apps/claws/app/share/claws-mail/themes/RISCOS /tmp/spr
python3 tools/png2sprite.py 'apps/claws/app/!Sprites,ff9' '!clawsmail=/tmp/spr/!clawsmail.png' 'sm!clawsmail=/tmp/spr/sm!clawsmail.png'
```

## 4. The programs and the release

```sh
make l3afpad themetest claws                 # out/<Program>-<version>.zip
make release RELEASE=3.24.41-riscos1.0-rc10    # out/release/3.24.41-riscos1.0-rc10/
```

`make emoji` makes `out/TTF-Noto-Color-Emoji-<version>.zip`, the optional
colour emoji font as a PackMan package: Noto Color Emoji in
`Apps/Fonts/!UnixFont/truetype/noto-color-emoji`, depending on UnixFont
and FontConfig, with `fc-cache` triggers, as PackMan's TTF-DejaVu-Core.
The programs don't carry it; they find it through FontConfig.


`make gltest` builds !GTKGLTest, a GtkGLArea test, with riscos-mesa
linked in. It needs riscos-mesa's devkit (20.3.5-15 or later) in
`MESA_DEVKIT`. The default is `$RCD_DIR/devkit/env`, where crossdev's
`devkit/install.sh` puts it.

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

The OpenGL test (`gltest-fake`) links riscos-mesa's RISC OS libraries
into the ARM Linux test, so it needs their devkit in `MESA_DEVKIT`, as
for `make gltest`. Without it, that one test is skipped.

The Claws Mail test starts `tests/claws/mailserver.py`, a local POP3,
SMTP and IMAP server. Every suite must pass before a release.

## Known build issues

- **The toolchain's `.la` files:** riscos-crossdev's `.la` files
  (libm, libstdc++, libunixlib) name paths from the machine that built
  them (`/opt/b14/...` in 1.4 and 1.5). libtool reads them when it links
  through the C++ driver. `build-claws.sh` links libetpan as C to avoid
  this. Reported to riscos-crossdev (handoff 2026-10-11).
- **Older toolchains:** `env.sh` stops if the toolchain is older than
  riscos-crossdev 1.3 (UnixLib 5.0.3.2, static only), and a RISC OS build
  if it doesn't link UnixLib 5.0.3.3 (crossdev 1.4 and later).

## Building on GitHub

`.github/workflows/riscos-build.yml` builds, tests and makes the devkit
on every push and pull request, in the riscos-crossdev image that
`.riscos-deps` names: `make deps tests hello check sdk`, after installing
`PACKAGES` (python3-pil, shared-mime-info and libgdk-pixbuf2.0-bin: riscos-crossdev's
image holds only what several ports share). GTKTest, GTKHello
and the devkit tarball are kept as the run's download. `dl/` is cached.

`.github/workflows/riscos-deps-update.yml` checks every day for a newer
release of riscos-crossdev. When there is one, it builds with it and opens
a pull request saying whether that passed. Nothing is merged or released
by itself: merge, test on a Pi, release (riscos-crossdev `docs/CI.md`).
GTK programs built on riscos-gtk can pin it the same way (`riscos-gtk
v3.24.41-riscos1.0`) once its devkit is published with each release.
