#!/bin/bash
# usage: theme/shoot.sh mode...   modes: dialog list menu popup factory
# Screenshots demo.py (or gtk3-widget-factory) in the RISCOS theme under
# Xvfb, with the settings the RISC OS GDK backend gives every program
# (DejaVu Sans 12 at 90 dpi, no animations), from a private HOME so the
# machine's own GTK settings don't leak in. Needs Xvfb, ImageMagick,
# python3-gi with GTK 3 (PYTHON=python3.x to pick one) and gtk-3-examples;
# writes shot_MODE.png here.
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d)
mkdir -p "$T/.themes/RISCOS" "$T/.config/gtk-3.0"
cp -r "$HERE/gtk-3.0" "$T/.themes/RISCOS/"
cat > "$T/.config/gtk-3.0/settings.ini" <<'INI'
[Settings]
gtk-theme-name=RISCOS
gtk-font-name=DejaVu Sans 12
gtk-xft-dpi=92160
gtk-enable-animations=false
gtk-dialogs-use-header=false
gtk-overlay-scrolling=false
INI
Xvfb :9 -screen 0 1000x760x24 >/dev/null 2>&1 & XP=$!
sleep 1
export HOME=$T XDG_CONFIG_HOME=$T/.config GTK_THEME=RISCOS DISPLAY=:9 NO_AT_BRIDGE=1
for m in "$@"; do
  if [ "$m" = factory ]; then
    gtk3-widget-factory 2>"$T/e.log" & P=$!; sleep 2.5
    import -window root "$T/full.png"; kill $P; out=shot_widget_factory.png
  else
    (cd "$HERE" && ${PYTHON:-python3} demo.py "$m" 2>"$T/e.log") & P=$!
    sleep 1.2; import -window root "$T/full.png"; wait $P; out=shot_$m.png
  fi
  convert "$T/full.png" -bordercolor black -border 1 -trim +repage "$HERE/$out"
  grep -i -E "warn|error" "$T/e.log" | grep -v -i "trigger event" | head -3
done
kill $XP; rm -rf "$T"
