# GTKHello 0.3 on the Pi (owner, 2026-10-04)

RISC OS 5, 1920x1200, 32 bpp TBGR, EIG 1,1 (see GTKHello-0.3-pi-Log.txt).

- Starts as a Wimp task; window opens and redraws.
- Button: 7 clicks, all counted.
- Check box: toggled twice (so "option off" at close is correct).
- Entry: typing and deleting work (empty at close because the text was
  deleted).
- **Correction (from the photos):** "Red" was drawn in blue and "Blue" in
  red. OS_SpriteOp copies a 32bpp TRGB sprite onto the TBGR screen without
  converting. Fixed in 0.4 (sprites in the screen's colour order).
- Closes cleanly.
- Moving at normal size: fine.
- Toggle size: the window grew but GTK kept drawing 320x180 in the corner;
  the rest was never painted (old screen showed through) and smeared when
  the big window was moved (photos GTKHello-0.3-pi-toggle-size.jpg,
  -moved-big.jpg). Fixed in 0.4: GDK takes Wimp-driven sizes from the
  configure event; the redraw fills anything the sprite doesn't cover.
- Not yet reported: corner resize, text size vs the desktop.
