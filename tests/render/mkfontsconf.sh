#!/bin/sh
# mkfontsconf.sh FONTDIR CACHEDIR: a minimal fonts.conf that uses only the
# fonts in FONTDIR, with DejaVu Sans, Serif and Sans Mono as the generic
# families (sans-serif, serif, monospace, and the names font choosers list:
# Sans, Serif, Monospace, system-ui), so each is a different face when the
# package has them; a family not there falls back to DejaVu Sans. Paths are as
# the program will see them: on RISC OS, UnixLib paths such as
# /<GTKTest$Dir>/fonts. fonts.conf is XML, so '<', '>' and '&' are written
# as entities (GTKTest 0.4 didn't: "line 4: not well-formed"); fontconfig
# turns them back. The same as riscos-warzone2100's fonts.conf. MIT licence.
esc() { printf '%s' "$1" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g'; }
cat <<EOT
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <dir>$(esc "$1")</dir>
  <cachedir>$(esc "$2")</cachedir>
  <alias><family>sans-serif</family><prefer><family>DejaVu Sans</family></prefer></alias>
  <alias><family>Sans</family><prefer><family>DejaVu Sans</family></prefer></alias>
  <alias><family>system-ui</family><prefer><family>DejaVu Sans</family></prefer></alias>
  <alias><family>serif</family><prefer><family>DejaVu Serif</family></prefer><default><family>DejaVu Sans</family></default></alias>
  <alias><family>Serif</family><prefer><family>DejaVu Serif</family></prefer><default><family>DejaVu Sans</family></default></alias>
  <alias><family>monospace</family><prefer><family>DejaVu Sans Mono</family></prefer><default><family>DejaVu Sans</family></default></alias>
  <alias><family>Monospace</family><prefer><family>DejaVu Sans Mono</family></prefer><default><family>DejaVu Sans</family></default></alias>
  <config><rescan><int>0</int></rescan></config>
</fontconfig>
EOT
