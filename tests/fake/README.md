# Fake RISC OS for host tests (for M2 onwards)

The GDK backend (`gdk/riscos/`, M2) is mostly Wimp calls, so most of it can
be tested on Linux against a fake RISC OS before it ever reaches a Pi, the
way riscos-mesa, riscos-ffmpeg and riscos-unixlib test theirs. Nothing here
is built yet; these are the starting points, copied unchanged.

| Here | From | Use in riscos-gtk |
|---|---|---|
| `mesa-egl/fake_riscos.c`, `.h`, `fake/` | riscos-mesa `tests/host-harness/egl/` | A scripted Wimp task: Wimp_Initialise, Create/Open/Close/DeleteWindow, Poll/PollIdle delivering redraws, null events, keys, resizes, clicks and menu choices; Redraw/Update/GetRectangle; a 32bpp screen in memory; OS_SpriteOp create/plot; mode and VDU variables. The base for the GDK backend's tests (the cairo surface → sprite → screen path can be checked pixel by pixel). |
| `mesa-egl/portrun.c` | same | The runner that drives a ported program with a script of events (`FRAMES`, `KEYS`, `PPM`). M2's pass test ("a window with a button that redraws and clicks") can run as a portrun script on Linux. |
| `sdl-wimp/wimp-events.c`, `run.sh`, `swis.h` | riscos-mesa `tests/host-harness/sdl-wimp/` | Feeds Wimp_Poll blocks to an event handler and records the SWIs and events: Message_PreQuit/Quit, the close icon, keys passed on. The model for testing the GDK event source. |

riscos-crossdev's own fake (`fake/riscos.c`, `fake.h`, `fake.mk`: a SWI
dispatcher with handlers installed by the test, built `-m32`) is used from
`$RCD_DIR` rather than copied. Other fakes to borrow from when needed:
riscos-unixlib `tests/host/fake` and `tests/host/ticker` (UnixLib internals,
the pthread ticker), riscos-ffmpeg `tests/host/fake_riscos.c` (TaskWindow),
midisynth `tests/fake_swi.c`, riscos-matinee `tests/host/ui_test.c` (a
Toolbox/Wimp UI test).

The sprite plotting itself (eig caching, BGRA → RISC OS pixel order) is in
riscos-mesa's SDL driver, `patches/sdl2/src.video.riscos.SDL_riscosframebuffer.c.p`.

All of the files above are Andrew Youll's own work under the MIT licence
(riscos-mesa's `LICENSE`).
