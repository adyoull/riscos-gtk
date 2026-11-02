# Security fixes from Ubuntu

riscos-gtk builds its libraries from their original source tarballs (Ubuntu's
`*.orig.tar.*`). Ubuntu's own packages of the same versions carry security
fixes on top, in `debian/patches`, which the original tarballs don't have.
This folder carries them over.

Each `DIR.diff` here is applied by `build/env.sh`'s `security_patches` right
after the library is unpacked into `$SRC/DIR`, before any RISC OS patch
(`patches/DIR-riscos.diff` or the like). Its first lines name Ubuntu's
package version and the patches it was made from, in their order.

| Library | Ubuntu package | Patches |
|---|---|---|
| zlib 1.3.1 | 1.3.dfsg+really1.3.1-1ubuntu3.2 (noble) | 2 |
| GLib 2.80.0 | 2.80.0-6ubuntu3.10 (noble) | 49 |
| libpng 1.6.37 | 1.6.37-3ubuntu0.7 (focal) | 23 |
| FreeType 2.10.1 | 2.10.1-2ubuntu0.4 (focal) | 6 |
| Expat 2.2.9 | 2.2.9-1ubuntu0.8 (focal) | 27 |
| gdk-pixbuf 2.42.10 | 2.42.10+dfsg-3ubuntu3.3 (noble) | 6 |
| libxml2 2.9.14 | 2.9.14+dfsg-1.3ubuntu3.10 (noble) | 32 |
| libxslt 1.1.39 | 1.1.39-0exp1ubuntu4.1 (noble) | 5 |
| libgsf 1.14.51 | 1.14.51-2ubuntu0.1 (noble) | 1 |
| libcroco 0.6.13 | 0.6.13-1ubuntu0.1 | 2 |
| libetpan 1.9.4 | 1.9.4-8 | 1 |

Left out, on purpose:
- **libpng:** CVE-2026-40930 and its follow-up fix the APNG reader, which is
  Debian's addition (libpng-1.6.37-apng.patch); our libpng has no APNG.
  Three patches without CVE names are included because later fixes build on
  them: 326 (invalid eXIf chunks), rowbytes_overflow, and 72fa126 (the tests'
  Makefile, which CVE-2026-34757-1 changes).
- **FreeType:** CVE-2022-31782 is in ft2demos, not the library.

Checked and needing nothing: pcre2 10.42 (Ubuntu's update was a rebuild),
libjpeg-turbo 2.1.5, and librsvg 2.40.20 (Ubuntu withdrew its fix for
CVE-2019-20446, which broke other programs). librsvg 2.40 and libcroco are no
longer maintained upstream; librsvg 2.40 was the last version written in C.

## Making them again

For a newer Ubuntu package, download its `debian.tar.xz` from
archive.ubuntu.com (`pool/main/<letter>/<source>/`) and run

    tools/mksecuritypatch.sh dl/ORIG DIR DEBIAN.debian.tar.xz [EXCLUDE] [EXTRA]

with the exclusions and extras above. It applies the patches whose names
contain CVE or security (and EXTRA) to a copy of the original tarball, in
`debian/patches/series` order, and writes `DIR.diff` here.

The patches are Ubuntu's and Debian's, under the licence of the library
they change.
