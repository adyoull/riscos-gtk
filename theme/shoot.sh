#!/bin/bash
# usage: shoot.sh mode...   (screenshots demo.py in each mode under Xvfb)
Xvfb :9 -screen 0 900x700x24 >/dev/null 2>&1 & XP=$!
sleep 1
for m in "$@"; do
  DISPLAY=:9 python3 demo.py $m 2>/tmp/e.log & P=$!
  sleep 1.2; DISPLAY=:9 import -window root /tmp/full_$m.png; wait $P
  convert /tmp/full_$m.png -bordercolor black -border 1 -trim +repage shot_$m.png
  grep -i -E "warn|error" /tmp/e.log | head -3
done
kill $XP
