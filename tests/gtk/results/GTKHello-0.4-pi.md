# GTKHello 0.4 on the Pi (2026-10-04)

Photo: GTKHello-0.4-pi-toggle-size.jpg (toggled to full screen).

- "Red Green Blue" in the right colours on the TBGR desktop: the
  screen-order sprites fix it.
- Toggle size: GTK lays the window out at full screen (the entry stretches
  across the window) and the whole window is painted.
- Dragging the window (normal and big): fine, no smearing.
- M2 (GTK on the Wimp) is done on the Pi. Corner resize not separately
  reported.

## Log (GTKHello-0.4-pi-Log.txt)
- `backing sprites TBGR (copied from cairo, R/B swapped), mode word &301680B5`.
- &301680B5 = a RISC OS 3.5-style mode word: type 6 (32bpp, bits 27-31),
  90 x 90 dpi, bit 0 set. That format has no colour-order field, so it
  always means TBGR (0x00BBGGRR). The sprite matches the screen and the
  plot is a straight copy.
- Not yet known: whether a selector with ModeFlags TRGB produces a RISC OS
  5 mode word (type 15 with flags) or also falls back to type 6. The 0.3
  swap suggests the plot either ignores or never got the TRGB flag.
