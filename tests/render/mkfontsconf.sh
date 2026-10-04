#!/bin/sh
# mkfontsconf.sh FONTDIR CACHEDIR: a minimal fonts.conf that uses only the
# fonts in FONTDIR, with DejaVu Sans as the default sans-serif. Paths are as
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
  <config><rescan><int>0</int></rescan></config>
</fontconfig>
EOT
