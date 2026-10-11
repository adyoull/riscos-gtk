# riscos-gtk's RISC OS API

Most GTK programs need no RISC OS code at all: riscos-gtk maps GTK's own
API onto the desktop (see "What happens without any code" below). This
page covers the rest:

- the start-up helper `<riscos-gtk/app.h>`;
- the functions in `<gdk/gdkriscos.h>`, for programs that want more;
- the environment variables riscos-gtk reads.

Wrap RISC OS-only code in `#ifdef GDK_WINDOWING_RISCOS`. `<gdk/gdk.h>`
defines it, so the same source still builds elsewhere. The template
(`template/hello.c`) does this.

## What happens without any code

| GTK | On RISC OS |
|---|---|
| `GtkWindow` (toplevel) | A Wimp window with title bar, back, close and toggle icons and the desktop's size icon. New windows open centred; later ones step down and right. |
| `GtkMenuBar` | Not drawn. Its menus open as a Wimp menu on Menu anywhere in the window. |
| `GtkMenu`, context menus, `GtkComboBox` pop-ups | Wimp menus. Over a widget with its own context menu, that menu is the window menu's first entry. |
| `GtkFileChooserDialog`, `GtkFileChooserNative` (save) | A RISC OS Save box: drag its icon to a Filer window or another program, or type a full name. |
| `GtkFileChooserDialog`, `GtkFileChooserNative` (open, select folder) | A RISC OS Open box that takes a file dragged to it. |
| `GtkPrintOperation` | A Toolbox-style Print box. Pages go to the printer driver !Printers has set up: text and lines as Draw paths, images as bitmaps. Print Preview opens in its own window. |
| `GtkClipboard` (text) | The global clipboard, shared with other programs. |
| Text drag and drop | Text dragged out becomes a Text file, or goes to another program by RAM transfer. |
| Files dropped on a window | A `text/uri-list` drop: handle "drag-data-received" as on other platforms. Or use `gdk_riscos_window_set_load_func` (below). |
| The icon bar | Every program gets an icon (see "The icon bar"). |
| `GtkApplication` | Select on the icon is "activate". Files double-clicked or dropped on the icon are "open". Quit on the icon bar menu is the "quit" action, and Choices... is "preferences". Closing the last window leaves the program on the icon bar. Use `G_APPLICATION_NON_UNIQUE`: there's no D-Bus. |
| The theme | The RISCOS theme: the desktop's Wimp colours, option, radio and icon sprites, and window border sprites for scroll bars. |
| Fonts | DejaVu Sans 12 at 90 dpi. With PackMan's FontConfig (UnixFC) installed, programs use the system's fonts, mapping and font cache, with their own `fonts` folder added; otherwise only their own fonts (see "Fonts"). |
| `GtkGLArea` | OpenGL 2.1 (or ES 2.0) through riscos-mesa, when the program links it in (see "OpenGL"). Without it, the area shows "OpenGL is not available". |
| EX0 EY0 (180 dpi) modes | GDK scale 2: GTK lays out at the usual size and draws at the screen's resolution. Pictures at twice the size are used where given (see "High-resolution pictures"). |
| Keys | GTK's accelerators, written in menus as RISC OS writes them (^O, F3). |

## `<riscos-gtk/app.h>`: start-up

```c
#include <riscos-gtk/app.h>
void riscos_gtk_app_init (const char *name, int *argc, char ***argv);
```

Call it first in `main()`, before `gtk_init()` or `g_application_run()`.
`name` is the application's name without the "!" (`Hello` for !Hello).
It:

- sets `FONTCONFIG_FILE` to `/<name$Dir>/fonts/fonts.conf` and
  `XDG_DATA_DIRS` to `/<name$Dir>/share`, unless they're already set;
- keeps the program's own files where RISC OS programs keep theirs:
  `XDG_CONFIG_HOME` and `XDG_DATA_HOME` are `<Choices$Write>.name`, and
  `XDG_CACHE_HOME` is `<Wimp$ScrapDir>.name.cache`. So
  `g_get_user_config_dir()`, GKeyFile settings files and GTK's
  recently-used list go there;
- sends stdout and stderr to `<Wimp$ScrapDir>.name.Log`, unless
  `RISCOS_GTK_NO_LOG` is set;
- calls `g_set_prgname (name)`;
- converts RISC OS file names in argv (`ADFS::HD4.$.Docs.Note/txt`) to
  UnixLib names (`/ADFS::HD4/$/Docs/Note.txt`). It hands the first to
  `gdk_riscos_iconbar_set_start_file`, so that if a copy of the program
  is already running, that copy loads the file and this one quits.

The application's `!Run` must set `name$Dir`. The template's does. The
environment variables are only set where they aren't already.

```c
gboolean riscos_gtk_open_uri (const char *uri);
```

This opens a web address (or any URI a RISC OS program has claimed) in
the user's browser. It goes through RISC OS's URI handler
(URI_Dispatch). If no browser is running, it starts Iris (or NetSurf, if
the Filer hasn't seen Iris) and sends the address once the browser is
running. GTK's `gtk_show_uri_on_window()` can't do this on RISC OS.
Returns FALSE, after an error box, if nothing took the address.

`app.h` is header-only: include it in one source file.

## Fonts

GTK draws text with pango, which finds fonts through fontconfig.
riscos-gtk picks the set-up when the display opens:

- **With PackMan's FontConfig installed** (UnixFC: `UnixFC$Dir` is set,
  and it has `fonts/fonts.conf`), the program uses the system's set-up,
  as other fontconfig programs do:
  - its fonts, in `UnixFont:`;
  - its font mapping;
  - its font cache, built once and shared by every program.

  The program's own `fonts` folder is added on top, so DejaVu Sans, GTK's
  default, is always there. riscos-gtk's fontconfig is 2.14.1, the
  version PackMan has, so they read the same configuration and cache.
- **Without it**, or if the system's set-up doesn't load, the program
  uses its own `fonts.conf` (`FONTCONFIG_FILE`, which
  `riscos_gtk_app_init` sets): its own fonts, with a cache in
  `<Wimp$ScrapDir>.name.fontcache`.
- **`*Set RISCOS_GTK_FONTS own`** forces the program's own set-up.

The Log says which set-up was used ("riscos-gtk: fonts: ..."). RISC OS's
own outline fonts (Homerton, Trinity...) can't be used: FreeType can't
read their format.

GSettings stay in memory: GLib's keyfile backend needs a file monitor
that RISC OS hasn't got. Keep settings in a GKeyFile under
`g_get_user_config_dir()`, which is `<Choices$Write>.name`.

## The icon bar

```c
typedef struct {
  void     (*show)      (gpointer data);
  gboolean (*open_file) (const char *unix_path, int filetype, gboolean scrap, gpointer data);
  void     (*choices)   (gpointer data);
  void     (*quit)      (gpointer data);
  gboolean (*unsaved)   (gpointer data);
} GdkRiscosIconbarHandlers;

void gdk_riscos_iconbar_set_handlers   (const GdkRiscosIconbarHandlers *handlers, gpointer data);
void gdk_riscos_iconbar_set_info       (const char *name, const char *purpose,
                                        const char *author, const char *version);
void gdk_riscos_iconbar_set_sprite     (const char *sprite);
void gdk_riscos_iconbar_add_filetype   (int filetype);
void gdk_riscos_iconbar_set_start_file (const char *riscos_path);
void gdk_riscos_iconbar_disable        (void);
```

Call these before `gtk_init()` or `g_application_run()`.

**What the icon does:**

- **Select or Adjust:** shows the program's windows. If none are open,
  it calls the `GtkApplication`'s "activate", or reopens the windows the
  program hid.
- **Menu:** opens Info >, Choices... and Quit. Choices... appears only if
  the program has settings.
- **Files:** a file double-clicked in the Filer (of a type added with
  `gdk_riscos_iconbar_add_filetype`), dropped on the icon, or saved to it
  by another program is opened.
- **Message_PreQuit:** at desktop shutdown, asks about unsaved work.
- **A second copy:** hands its file to the first copy and quits.

**Handlers:** each handler you set replaces the default for that action;
leave the others NULL. With no handlers set at all, riscos-gtk uses, in
order:

1. the `GtkApplication`: "activate", "open", and the "quit" and
   "preferences" actions;
2. the program's own menu items, found by their labels: Quit, and
   Preferences, Settings or Choices.

(Once any handlers are set, Choices... appears only if `choices` is
set.)

**`unsaved`:** when it returns TRUE, Quit and PreQuit first ask the user
in a RISC OS question box: Cancel / Discard.

**`open_file`:**

- `scrap` is TRUE for data from another program, saved in
  `<Wimp$Scrap>`. Load it as untitled; riscos-gtk deletes the file
  afterwards.
- Return FALSE if the file couldn't be opened.

**`set_info`:** the Info box's lines, in UTF-8. NULL leaves a line as it
is.

**`set_sprite`:** the icon's sprite in the Wimp pool. The default is "!"
plus the program name in lower case (`!hello`) if the pool has that
sprite, and "application" otherwise. `!Boot` and `!Run` load the sprites
with `IconSprites`.

**`disable`:** no icon. `*Set RISCOS_GTK_ICONBAR 0` does the same from
outside the program.

## Files

```c
char *gdk_riscos_path_to_unix   (const char *riscos_path);
char *gdk_riscos_path_from_unix (const char *unix_path);
```

These convert between `ADFS::HD4.$.Docs.Report/txt` and
`/ADFS::HD4/$/Docs/Report.txt`. Both return a new string; free it with
`g_free`. GTK and GLib always use UnixLib names. Use these where you
show a name to the user or take one typed in RISC OS form.

**File types:** riscos-gtk's Save box sets a saved file's type from its
extension: txt, html, png, jpg, gif, pdf, zip, svg, xml, json, css, js,
bmp, tif, wav, mp3, ogg and others. A file without an extension is Data
(&FFD), unless the program sets `RISCOS_GTK_DEFAULT_FILETYPE` (for
example to `fff`). Files your own code writes get the type UnixLib gives
them; if the type matters, set it with OS_File 18 after writing.

```c
typedef gboolean (*GdkRiscosLoadFunc) (const char *riscos_path, int filetype, gpointer data);
void gdk_riscos_window_set_load_func (GdkWindow *window, GdkRiscosLoadFunc func, gpointer data);
```

This takes files dropped from the Filer on a window directly. `func`
gets the RISC OS name and the file type; return TRUE to take the file.
Use `gtk_widget_get_window (toplevel)` once the window is realised.
Pass NULL `func` to stop.

**The directory browser:** `GtkFileChooserDialog` and
`GtkFileChooserNative` show the Save or Open box; its Browse... button
(or Menu over it) gives the directory browser, a Filer-like window with a sidebar of
discs, places and recent folders. `*Set RISCOS_GTK_CHOOSER filer` (in a
program's `!Run`, or by the user for every program) shows the browser
first. It uses the program's filters, current folder, current name and
select-multiple setting; nothing else is needed.

**Directories as the Filer sees them** (used by the browser):

```c
typedef struct {
  char   *name;          /* leaf, RISC OS form */
  int     object_type;   /* 1 file, 2 directory, 3 image file */
  int     filetype;      /* 0-&FFF; &1000 directory, &2000 application, -1 untyped */
  guint32 length, attributes;
  gint64  date;          /* centiseconds since 1900, or -1 */
} GdkRiscosDirEntry;
GPtrArray *gdk_riscos_read_dir       (const char *riscos_dir, GError **error);
char     **gdk_riscos_filing_systems (void);
char      *gdk_riscos_canonicalise   (const char *name);
char      *gdk_riscos_filetype_name  (int filetype);
```

`read_dir` lists a directory (OS_GBPB 12) as a `GPtrArray` of entries,
freed with the array. `filing_systems` gives the names FileSwitch knows
(OS_FSControl 33; free with `g_strfreev`). `canonicalise` gives a full
name (OS_FSControl 37), `filetype_name` "Text", "Sprite" and so on
(OS_FSControl 18).

**Your own Save drag** (most programs use `GtkFileChooserDialog`
instead):

```c
typedef void (*GdkRiscosSaveFunc) (const char *path, gboolean safe, gpointer data);
void     gdk_riscos_save_drag_begin (GdkDisplay *display, const char *sprite,
                                     int root_x, int root_y, int w, int h);
gboolean gdk_riscos_save_drop       (GdkDisplay *display, const char *leafname,
                                     int filetype, int size,
                                     GdkRiscosSaveFunc func, gpointer data);
void     gdk_riscos_save_done       (const char *path);
```

1. **Start the drag:** `save_drag_begin` with the file icon's sprite
   (`file_fff`) and its box, in root pixels.
2. **End the drag:** `save_drop` with the leaf name (RISC OS form), the
   type and an estimated size. It returns FALSE if the drag ended over
   nothing that takes files.
3. **Save:** `func` is called with the path (RISC OS form) to save to.
   `safe` is FALSE for `<Wimp$Scrap>`, which means the data is going to
   another program.
4. **Finish:** call `save_done (path)` after writing the file.

## Dialogue boxes and messages

```c
void     gdk_riscos_window_set_transient_dbox (GdkWindow *window, gboolean transient);
void     gdk_riscos_report_error (const char *message);
gboolean gdk_riscos_ask          (const char *message, const char *button);
```

- **`set_transient_dbox`:** opens a toplevel as RISC OS opens a dialogue
  box from a menu. A click elsewhere closes it, and GTK gets a delete
  event.
- **`report_error`:** a Wimp error box with an OK button, "Message from"
  the program.
- **`ask`:** a RISC OS question box with Cancel and `button`. Returns
  TRUE if `button` was chosen.

## The desktop's look

```c
gboolean   gdk_riscos_wimp_palette (guint32 rgb[16]);
GdkPixbuf *gdk_riscos_wimp_sprite  (const char *name);
GdkPixbuf *gdk_riscos_wimp_tool_sprite (const char *name);
int        gdk_riscos_scale_factor (void);
GdkPixbuf *gdk_riscos_wimp_sprite_scaled      (const char *name, int scale);
GdkPixbuf *gdk_riscos_wimp_tool_sprite_scaled (const char *name, int scale);
```

- **`wimp_palette`:** the 16 Wimp colours, as 0xRRGGBB.
- **`wimp_sprite`, `wimp_tool_sprite`:** a sprite from the Wimp pool
  (`optoff`, `file_fff`, `small_dir`) or a window border sprite (`uicon`,
  `vbarmid`), as an RGBA pixbuf at GDK pixel size. NULL if there's no
  such sprite.
- **`scale_factor`:** 2 in an EX0 EY0 mode, otherwise 1.
- **`..._scaled`:** the picture with `scale` times the pixels: the
  desktop's 180 dpi sprite if it has one, otherwise the 90 dpi one
  doubled.

## High-resolution pictures

In an EX0 EY0 mode GTK draws at twice the pixels. A picture GTK scales
up is the right size but soft. To make one sharp:

- **Icons from an icon theme:** give GTK a 2x size; it picks it as on
  any HiDPI screen.
- **CSS images:** use `-gtk-scaled(url("x.png"), url("x@2.png"))`.
- **A GdkPixbuf you make at twice the size:** tag it, and `GtkImage` and
  `GtkCellRendererPixbuf` show it at half its pixel size:

  ```c
  g_object_set_data (G_OBJECT (pixbuf), "riscos-gtk-scale", GINT_TO_POINTER (2));
  ```
- **Drawing with cairo:** you get the device scale for free. Text and
  paths are sharp. Draw images at twice the pixels with
  `cairo_scale (cr, 0.5, 0.5)`.
- **Application icons:** add a `!Sprites11` file with 68 x 68 and
  34 x 34 sprites at 180 dpi (the template has one).

## OpenGL (GtkGLArea)

`GtkGLArea` draws with riscos-mesa: Mesa's software renderer, which
does OpenGL 2.1 with GLSL 1.20, or OpenGL ES 2.0. It is linked into
the program, and only into programs that ask for it, because Mesa adds
about 9 MB. To have OpenGL:

1. **Install riscos-mesa's devkit,** 20.3.5-15 or later, and set
   `RISCOS_MESA_DEVKIT` to it before sourcing `env.sh`. riscos-crossdev's
   `devkit/install.sh` puts it in its `devkit/env`, which is the
   default.
2. **Link with `$RISCOS_GTK_GL_LIBS`** before `$RISCOS_GTK_LIBS`, or
   with `pkg-config --libs --static riscos-gtk-gl`:

   ```sh
   $CC $RISCOS_GTK_CFLAGS prog.c -o prog.elf $RISCOS_GTK_GL_LIBS $RISCOS_GTK_LIBS
   ```

   This is `-Wl,-u,eglGetProcAddress` plus riscos-mesa's `libEGL.a`,
   `libOSMesa.a` and `-lstdc++`. GTK refers to EGL weakly; the `-u`
   pulls libEGL in. With meson, use `dependency('riscos-gtk-gl')`;
   `env.sh` writes that pkg-config file when it finds the devkit.
3. **Package it as usual.** `riscos-gtk-package` adds riscos-mesa's
   licences (`LicenceMesa`) when it finds Mesa in the program.

`examples/glarea/` is a complete program: a GLSL triangle with the
frame rate shown.

What to know:

- **The version:**
  - GTK asks for OpenGL 3.2 core. riscos-mesa has no core profiles, so
    you get a 2.1 compatibility context, which GTK calls "legacy":
    `gdk_gl_context_is_legacy()` returns TRUE.
  - Write GLSL 1.20 (`#version 120`), or `#version 100` ES shaders,
    which riscos-mesa also takes in a desktop context.
  - Fixed-function GL (`glBegin`, matrices) works too.
- **OpenGL ES:** `*Set GDK_GL gles` gives ES 2.0 contexts instead.
  - As in GTK 3 everywhere, a `GtkGLArea`'s own `use-es` property
    doesn't change this, because its context shares with the window's.
- **GL functions:** call them through libepoxy (`<epoxy/gl.h>`), as any
  GTK program does. Epoxy finds them with riscos-mesa's
  `eglGetProcAddress`.
- **Speed:**
  - Rendering is on the processor, so the cost grows with the area's
    size and what each pixel does. GTKGLTest and `examples/glarea` show
    the frame rate, so you can measure.
  - After each frame, GDK copies the area's pixels into the window, and
    only the area's.
  - Use `gtk_gl_area_queue_render()` from a tick callback for
    animation, and stop when nothing moves.
- **180 dpi modes:** the area renders at twice the pixels, so it's
  sharp. The default "resize" handler sets the viewport in those pixels.
- **How it's made:**
  - Each GL context is an EGL context on a small pbuffer, and GtkGLArea
    draws into its own framebuffer object.
  - GDK reads the finished frame back into the window's backing sprite.
  - Windows themselves are never painted with GL.
- **Not supported:** rendering into the window directly (EGL window
  surfaces), full screen, and GL 3 features. For a game that wants the
  whole screen or every frame per second, use riscos-mesa's SDL2 or
  EGL directly instead of GTK.

## Size icon placement

```c
gboolean gdk_riscos_window_get_size_grip        (GdkWindow *window, gint *width, gint *height);
void     gdk_riscos_window_set_size_grip_column (GdkWindow *window, gint x, gint width);
void     gdk_riscos_window_set_size_grip_place  (GdkWindow *window, gint x, gint y, gint width);
void     gdk_riscos_window_set_size_strip       (GdkWindow *window, cairo_surface_t *image);
```

riscos-gtk places the size icon itself:

- under a GTK scroll bar that runs into the window's bottom right
  corner;
- otherwise at the foot of an empty scroll bar outside the window's
  right edge.

These functions are for programs that lay out their own scroll bars.

## Printing at a lower level

`GtkPrintOperation` is the way to print. GTK's own RISC OS print code
uses `gdk_riscos_printer_info`, `gdk_riscos_print_begin`, `_page` and
`_end`. They print cairo recording surfaces through the PDriver module,
and are there for programs that drive the printer themselves.

## Used by GTK itself

Programs don't need these:

- `gdk_riscos_iconbar_create`, `_enabled`, `_info_window`,
  `_menu_position`;
- `gdk_riscos_iconbar_get_handlers`;
- `gdk_riscos_set_event_watch`.

## Environment variables

Set them in `!Run` with `*Set`, or with `g_setenv()` before
`gtk_init()`.

| Variable | Effect |
|---|---|
| `RISCOS_GTK_DEFAULT_FILETYPE` | Type (hex) for saved names without an extension, e.g. `fff` |
| `RISCOS_GTK_ICONBAR` = 0 | No icon bar icon |
| `RISCOS_GTK_SCALE` = 1 or 2 | Override the GDK scale (normally 2 only in EX0 EY0 modes) |
| `RISCOS_GTK_CENTRE` = 0 | Don't centre new windows |
| `RISCOS_GTK_MENUBAR` (any value) | Draw the menu bar in the window as well |
| `RISCOS_GTK_MENUS` (any value) | GTK's own menus instead of Wimp menus (for testing) |
| `RISCOS_GTK_CHOOSER` = `filer` | The directory browser instead of the Save and Open boxes (their Browse... button also gives it) |
| `RISCOS_GTK_NATIVE_CHOOSER` = 0 | GTK's own file chooser instead of the Save and Open boxes |
| `RISCOS_GTK_SIZE_GRIP` = 0 | No size icon |
| `RISCOS_GTK_WIMP_THEME` = 0 | The RISCOS theme without the desktop's colours and sprites |
| `RISCOS_GTK_PRINT_BITMAP` (any value) | Print everything as bitmaps, not Draw paths |
| `RISCOS_GTK_DEBUG` (any value) | More detail in the Log: Wimp events, the theme's CSS |
| `GDK_GL` = `gles` | OpenGL ES 2.0 contexts for GtkGLArea instead of OpenGL 2.1 |
| `RISCOS_GTK_FONTS` = `own` | Only the program's own fonts and font cache, even with UnixFC installed |
| `RISCOS_GTK_NO_LOG` (any value) | `riscos_gtk_app_init` leaves stdout and stderr alone |

Each program's Log starts with lines giving the task name, the screen
mode, the GDK scale and what the theme found on the desktop. A problem
report should include it.
