# The ld-riscos/so/2 loader and SOManager 3.04, for !SharedLibs

riscos-gtk's programs are hard-float EABI ELF programs, linked "mixed"
(riscos-crossdev 1.11): they need the dynamic loader `ld-riscos/so/2` and
SOManager 3.x. PackMan's SharedLibs (riscos.info, 4.0) has only the older
`ld-riscos/so/1`. These three files are added to the user's
`!Boot.Resources.!SharedLibs` by !GTKLibs: `build/build-gtklibs.sh` puts
them in `!GTKLibs.SharedLibs` (dated 27 May 2021, SOManager 3.04's date),
and !GTKLibs' !Boot copies them into !SharedLibs each time it is booted:
the loader where it's missing, SOManager where it's missing or older (by
date). COPYRIGHT-ld goes in `!GTKLibs.SharedLibs` too.

| File | What |
|---|---|
| `SOManager,ffa` | SOManager 3.04 (27 May 2021), "(c) GCCSDK Developers" |
| `lib/ld-riscos-eabihf.so,e1f` | the hard-float EABI dynamic loader (GCCSDK GCC 10.2.0) |
| `lib/ld-riscos.so.2,1c8` | the link `ld-riscos/so/2` → `ld-riscos-eabihf/so` (type &1C8) |

They are GCCSDK's (free software; the dynamic loader's licence is in
`COPYRIGHT-ld`, from !SharedLibs' docs), taken unchanged from the
!SharedLibs that Iris ships, which crossdev's MixTest and riscos-gtk's
mixed programs were tested with. Their source is GCCSDK's, at
riscos.info.
