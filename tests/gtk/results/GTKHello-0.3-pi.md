# GTKHello 0.3 on the Pi (owner, 2026-10-04)

RISC OS 5, 1920x1200, 32 bpp TBGR, EIG 1,1 (see GTKHello-0.3-pi-Log.txt).

- Starts as a Wimp task; window opens and redraws.
- Button: 7 clicks, all counted.
- Check box: toggled twice (so "option off" at close is correct).
- Entry: typing and deleting work (empty at close because the text was
  deleted).
- "Red Green Blue" in the right colours: the TRGB backing sprites plot
  correctly on a TBGR screen, so no TBGR path is needed.
- Closes cleanly.
- Not yet reported: resize, move, toggle size, text size vs the desktop.
