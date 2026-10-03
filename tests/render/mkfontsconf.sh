#!/bin/sh
# mkfontsconf.sh FONTDIR CACHEDIR: a minimal fonts.conf that uses only the
# fonts in FONTDIR, with DejaVu Sans as the default sans-serif. Paths are as
# the program will see them (on RISC OS, UnixLib paths such as
# /<RenderTest$Dir>/fonts). MIT licence.
cat <<EOT
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <dir>$1</dir>
  <cachedir>$2</cachedir>
  <alias><family>sans-serif</family><prefer><family>DejaVu Sans</family></prefer></alias>
  <alias><family>Sans</family><prefer><family>DejaVu Sans</family></prefer></alias>
</fontconfig>
EOT
