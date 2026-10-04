# Decisions

From the feasibility study (2026-09-24) and the owner's choices since.

## GTK 3.24 with a new native GDK backend
- GTK 3, not GTK 4: GTK 3 draws through cairo image surfaces, which suits
  the Wimp's redraw model; GTK 4 wants GL/Vulkan and newer dependencies.
- A GDK backend (`gdk/riscos/`, modelled on broadway's client side, ~6–9k
  lines), **not** a wrapper mapping GTK widgets onto Toolbox gadgets. The
  gadget set is far too small (no tree view, text view, drawing area or
  size-negotiating layout) and real programs depend on GTK's widget
  internals. GTK draws its own widgets; the theme makes them look like
  RISC OS.
- Native where GTK already has a hook:
  - window furniture is the Wimp's (an unknown backend gets no client-side
    decorations); the theme styles header bars as plain toolbars;
  - popups and menus open as Wimp menus (Wimp_CreateMenu with a window
    handle); a combo's menu opens to the right of its button, as a RISC OS
    pop-up does (backend positioning, not CSS);
  - `GtkFileChooserNative` gets a RISC OS version (a save box with a file
    icon to drag to the Filer; drag from the Filer to open), shaped like
    GTK's Win32 and Quartz versions;
  - later: GMenuModel → Wimp menus, GtkStatusIcon → icon bar icon.
- Payoff programs: AbiWord (fonts, clipboard, file chooser, printing),
  then Gnumeric. LibreOffice and Calligra are out of scope; GIMP stays
  blocked on its plug-in processes.

## Toolchain and libraries
- riscos-crossdev tagged releases (now 1.3: GCC 10.2, UnixLib 5.0.3.2, static only),
  never main. Build scripts adapted from riscos-warzone2100, which already
  builds FreeType, expat, fontconfig and libpng for this toolchain.
- Static linking only (UnixLib has no dlopen): gdk-pixbuf loaders and IM
  modules built in, no print backends at first, no GIO modules.
- Library versions follow Ubuntu 24.04 (GLib 2.80.0, GTK 3.24.41, which
  the theme was made with), using Ubuntu's orig tarballs, which the cloud
  build machine can download.
- Libraries are configured with prefix `/riscos-gtk` and installed into
  `sysroot/riscos-gtk`, so no build-machine path is compiled in. !Run
  files point GLib and GTK at their data with environment variables.

## Owner's choices (2026-10-03)
- Folder `RiscOS/GTK/`, repo `riscos-gtk`, rules as the other ports
  (git bundles delivered, the owner pushes; handoffs outside git).
- Own files MIT; copied files keep their licences.
- DejaVu Sans stands in for Homerton (as Warzone 2100 ships it).
- M1 passes on QEMU first; the Pi run of !GLibTest comes later.
