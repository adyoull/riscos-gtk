# Source and licences for a riscos-gtk release

What a release has to carry, and why. File-by-file licences are in
[../LICENCES.md](../LICENCES.md).

## What each program contains

Every package is one statically linked program. UnixLib has no dlopen,
so shared libraries aren't possible today.

| In the program | Licence | Obligation |
|---|---|---|
| The program itself | L3afpad: GPL v2 or later. Claws Mail: GPL v3 or later. Geany: GPL v2 or later. mtPaint: GPL v3 or later. Gnumeric (with GOffice): GPL v2 or v3. GTKThemeTest, GTKGLTest: MIT | GPL: the complete corresponding source, under the GPL |
| riscos-gtk's own code: the GDK backend `gdk/riscos/`, `gtk/riscos/`, `cairo/`, the patches, `apps/*/riscos-*.c` | LGPL 2.1+ (backend code that becomes part of GTK); GPL as the program (`apps/`); MIT (the rest) | Source, as part of the library or program it belongs to |
| GTK 3.24.41, GLib 2.80, pango, ATK, gdk-pixbuf, fribidi | LGPL 2.1 or later | See "the LGPL and static linking" below |
| cairo 1.18 | LGPL 2.1 or MPL 1.1 | Taken as LGPL 2.1 |
| harfbuzz, pixman, expat, fontconfig, libffi, libepoxy, zlib, libpng, libjpeg-turbo, FreeType, PCRE2 | MIT, BSD, zlib, libpng, IJG or FreeType licences | The notices (LICENCES.md) |
| Claws Mail only: Nettle 3.10.2 | LGPL 3 or GPL 2 | Covered by the GPL program's source |
| Claws Mail only: libetpan 1.9.4 | BSD-style | Its notice: `LicenceEtPan` in the package |
| Claws Mail only: its litehtml viewer plug-in, with litehtml | Plug-in GPL 3 or later; litehtml BSD-style | litehtml's notice: `LicenceLiteHTML` in the package |
| Claws Mail only: gumbo 0.12.0 | Apache License 2.0 | Its licence: `LicenceGumbo` in the package |
| Gnumeric only: libgsf 1.14.51 | LGPL 2.1 | As the other LGPL libraries, below |
| Gnumeric only: libxml2 2.9.14, libxslt 1.1.39 | MIT | The notices (LICENCES.md) |
| GTKGLTest only: riscos-mesa (Mesa 20.3.5) | MIT | The notice; source: riscos-mesa's release |
| UnixLib 5.0.3.3, GCC's runtime | UnixLib: mostly BSD-style and public domain, some LGPL parts. GCC's runtime: GPL with the GCC Runtime Library Exception | UnixLib's source is the riscos-unixlib v5.0.3.3 release, which the source archive and each package's `Source` point to |

The package also carries some data and modules:

| In the package | Licence |
|---|---|
| DejaVu fonts | Bitstream Vera / Arev: free; its `Licence` in `fonts/` |
| Adwaita icons (`share/icons`) | LGPL 3 / CC-BY-SA 3.0 |
| The MIME database (`share/mime`), from shared-mime-info 2.4 | GPL 2 or later; its `Licence` in `share/mime`; source: shared-mime-info 2.4 (in the source archive's downloads) |
| The RISCOS theme | MIT |
| ClawsMail's RISC OS icons (`share/claws-mail/themes/RISCOS`, `!Sprites`), drawn by `tools/mkclawsicons.py` | MIT |
| The RISC OS-style icons (`share/icons/RISCOS`), drawn by `tools/mkriscosicons.py` and `tools/riscosicons_apps.py` | MIT |
| PThreadTicker module | Its own `PThreadTicker-Licence` |

## The LGPL and static linking

LGPL 2.1 section 6 lets a program be distributed linked with the library
if it comes with two things:
- the library's complete source;
- the program as source or object code, so the user can change the
  library and relink.

Section 6(d) allows this to be offered from the same place as the
program.

riscos-gtk meets this with the source archive published with each
release, `riscos-gtk-<release>-src.tar`, made by
`build/make-release.sh`. The archive holds:
- the repository at the release commit: the backend, the patches, every
  program's RISC OS code, the build scripts and these documents;
- every upstream tarball named in `build/SHA256SUMS.txt`: the
  libraries, L3afpad, Claws Mail, Nettle, libetpan, gumbo, Geany,
  mtPaint, Gnumeric, GOffice, libgsf, libxml2, libxslt and
  shared-mime-info.

With the riscos-crossdev toolchain, `docs/BUILDING.md` rebuilds every
program from it, including after changing a library. Every program's
own source is in the archive, so no separate object-file relink kit is
needed. (LICENCES.md used to say a release needed one; that was written
before the programs were open source.)

## What every package carries

The packaging script (`build/build-app.sh`) adds these to every package:
- `LicenceLGPL`: the LGPL 2.1 text.
- `Source`: which riscos-gtk commit and release the package was built
  from, what it links, and where the source archive is. It also offers a
  copy for three years.
- The program's own licence: `Licence`. L3afpad: GPL 2. Claws Mail: GPL
  3. Geany: GPL 2. mtPaint: GPL 3. Gnumeric: its COPYING with the GPL 2
  and 3 texts. GTKThemeTest, GTKGLTest: MIT.
- For Claws Mail, `LicenceEtPan`, `LicenceLiteHTML` and `LicenceGumbo`.
- `fonts/Licence`, `PThreadTicker-Licence`, `share/mime/Licence`.
- `!Help`: lists the above.

## Publishing a release

1. **Build:** `make release RELEASE=<release>` from a committed tree.
   Set `SOURCE_URL` to the release page, so every package's `Source`
   names it: the GitHub release of adyoull/riscos-gtk,
   `https://github.com/adyoull/riscos-gtk/releases/tag/v<release>`.
2. **Publish together, on one page:** every program's zip, the devkit
   (`riscos-gtk-sdk-<release>.tar.gz`), the source tar and
   `MANIFEST.txt`.
3. **Tag the commit** as `v<release>`. It is the commit `MANIFEST.txt`
   names.
4. **Keep the source tar available for at least three years,** as the
   `Source` file promises.

## The devkit

The devkit (`build/make-sdk.sh`, made by `make release` too) gives other
developers the libraries as static libraries, so it is under the same
terms as the programs:

- **It points to the source.** Its `VERSION` file and the `Source` file
  it puts in each program name riscos-gtk's source archive for the same
  release.
- **Developers must pass the terms on.** Programs packaged with its
  `riscos-gtk-package` carry `LicenceLGPL` and a `Source` file. The
  devkit's README tells developers to make their own program's source,
  or its object files, available for relinking.
- **Development builds are different.** One made outside a release
  (`dev-<commit>`) points to the commit on GitHub instead. Only publish
  devkits made by `make release`.
