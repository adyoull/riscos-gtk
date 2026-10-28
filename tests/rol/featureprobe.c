/* featureprobe.c - riscos-gtk: this round's features on a real RISC OS
 * (RISC OS on Linux's desktop, or a Pi), as a Wimp task:
 *   - SVG: gdk_pixbuf_new_from_file on an SVG (librsvg's loader, built in),
 *     its size and pixels, saved as PNG (OUTDIR.svg/png) to look at;
 *   - spelling: enchant with the en_GB dictionary in APPDIR.share.hunspell;
 *   - MimeMap: the file types gdk_riscos_open_file would give;
 *   - opening a file: gdk_riscos_open_file on OUTDIR.Page (HTML, made
 *     untyped: data, &FFD, as an attachment saved by UnixLib can be) with
 *     its MIME type: typed HTML, then Filer_Run: the browser opens it,
 *     which the screenshot shows;
 *   - gtk_show_uri_on_window on a text file.
 * Writes its results to OUTDIR.results (text output isn't kept in the
 * test bed's desktop mode).
 *   featureprobe APPDIR OUTDIR        (UnixLib names)
 * MIT licence. */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <enchant.h>
#include <hunspell.h>
#include <kernel.h>
#include <stdio.h>
#include <string.h>

static FILE *out;
static int n_tests, n_failed;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  fprintf (out, "%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (out);
}

static int
mimemap (int from, const char *what)
{
  _kernel_swi_regs r;
  memset (&r, 0, sizeof r);
  r.r[0] = from;
  r.r[1] = (int) what;
  r.r[2] = 0;
  if (_kernel_swi (0x50B00, &r, &r) != NULL)
    return -1;
  return r.r[3];
}

static void
describe_cb (const char *lang, const char *name, const char *desc, const char *file, void *data)
{
  fprintf (out, "# enchant dict: %s %s (%s) %s\n", lang, name, desc, file);
}

static gboolean
quit (gpointer d)
{
  gtk_main_quit ();
  return FALSE;
}

int
main (int argc, char **argv)
{
  static const char svg[] =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"60\">"
    "<rect width=\"60\" height=\"60\" fill=\"#ff0000\"/>"
    "<circle cx=\"90\" cy=\"30\" r=\"25\" fill=\"#0000ff\"/></svg>";
  char *p, *dict;
  GdkPixbuf *pb;
  GError *err = NULL;
  EnchantBroker *b;
  EnchantDict *d;
  int t;

  if (argc < 3)
    return 2;
  /* the dictionary: set before GLib reads the data folders (it keeps
     them), as the programs do */
  dict = g_build_filename (argv[1], "share", NULL);
  g_setenv ("XDG_DATA_DIRS", dict, TRUE);
  g_free (dict);
  gtk_init (&argc, &argv);
  p = g_build_filename (argv[2], "results", NULL);
  out = fopen (p, "w");
  g_free (p);
  if (!out)
    return 3;

  /* SVG */
  p = g_build_filename (argv[2], "probe.svg", NULL);
  g_file_set_contents (p, svg, -1, NULL);
  pb = gdk_pixbuf_new_from_file (p, &err);
  check (pb && gdk_pixbuf_get_width (pb) == 120 && gdk_pixbuf_get_height (pb) == 60,
         "SVG: gdk_pixbuf_new_from_file at its size (120x60)");
  if (err)
    fprintf (out, "# %s\n", err->message);
  g_clear_error (&err);
  if (pb)
    {
      const guchar *px = gdk_pixbuf_read_pixels (pb);
      int rs = gdk_pixbuf_get_rowstride (pb), n = gdk_pixbuf_get_n_channels (pb);
      const guchar *a = px + 30 * rs + 20 * n, *c = px + 30 * rs + 90 * n;
      check (a[0] > 230 && a[2] < 30 && c[2] > 230 && c[0] < 30, "SVG: red and blue where they should be");
      g_free (p);
      p = g_build_filename (argv[2], "svg.png", NULL);
      check (gdk_pixbuf_save (pb, p, "png", NULL, NULL), "SVG: saved as PNG to look at");
      g_object_unref (pb);
    }
  g_free (p);

  /* a gradient (the SVG beside the results: Picture.svg), as PNG */
  {
    char *gsvg = g_build_filename (argv[2], "..", "Picture.svg", NULL);
    GdkPixbuf *g = gdk_pixbuf_new_from_file (gsvg, NULL);
    if (g)
      {
        char *png = g_build_filename (argv[2], "gradient.png", NULL);
        gdk_pixbuf_save (g, png, "png", NULL, NULL);
        g_free (png);
        g_object_unref (g);
      }
    g_free (gsvg);
  }
  /* spelling */
  {
    /* UTF-8 to UTF-8 through iconv (enchant's hunspell provider did this
       for UTF-8 dictionaries) */
    GIConv cd = g_iconv_open ("UTF-8", "UTF-8");
    char in[] = "colour", outb[32], *ip = in, *op = outb;
    gsize il = 6, ol = sizeof outb;
    gsize r = cd != (GIConv) -1 ? g_iconv (cd, &ip, &il, &op, &ol) : (gsize) -2;
    fprintf (out, "# iconv UTF-8 to UTF-8: open %s, convert %d\n", cd != (GIConv) -1 ? "yes" : "no", (int) r);
    if (cd != (GIConv) -1)
      g_iconv_close (cd);
  }
  {
    /* hunspell itself, on the same files */
    char *aff = g_build_filename (argv[1], "share", "hunspell", "en_GB.aff", NULL);
    char *dic = g_build_filename (argv[1], "share", "hunspell", "en_GB.dic", NULL);
    FILE *f = fopen (dic, "r");
    int lines = 0;
    char buf[512];
    Hunhandle *h;
    if (f)
      {
        while (fgets (buf, sizeof buf, f))
          lines++;
        fclose (f);
      }
    h = Hunspell_create (aff, dic);
    fprintf (out, "# hunspell: %s: %d lines; encoding %s; colour %d, speling %d\n", dic, lines,
             h ? Hunspell_get_dic_encoding (h) : "-", h ? Hunspell_spell (h, "colour") : -1,
             h ? Hunspell_spell (h, "speling") : -1);
    if (h)
      Hunspell_destroy (h);
    g_free (aff);
    g_free (dic);
  }
  b = enchant_broker_init ();
  d = b ? enchant_broker_request_dict (b, "en_GB") : NULL;
  check (d != NULL, "spelling: enchant has the en_GB dictionary");
  if (d)
    {
      size_t ns = 0;
      char **sg = enchant_dict_suggest (d, "speling", -1, &ns);
      enchant_dict_describe (d, describe_cb, NULL);
      fprintf (out, "# enchant: %d suggestions for speling, first %s; word chars \"%s\"\n", (int) ns,
               ns ? sg[0] : "-", enchant_dict_get_extra_word_characters (d));
      if (sg)
        enchant_dict_free_string_list (d, sg);
    }
  if (d)
    fprintf (out, "# enchant: colour removed %d added %d; Choices$Write %s\n",
             enchant_dict_is_removed (d, "colour", -1), enchant_dict_is_added (d, "colour", -1),
             g_getenv ("Choices$Write") ? g_getenv ("Choices$Write") : "-");
  if (d)
    fprintf (out, "# enchant: colour %d, speling %d, organise %d (%s)\n",
             enchant_dict_check (d, "colour", -1), enchant_dict_check (d, "speling", -1),
             enchant_dict_check (d, "organise", -1), enchant_dict_get_error (d) ? enchant_dict_get_error (d) : "no error");
  check (d && enchant_dict_check (d, "colour", -1) == 0 && enchant_dict_check (d, "speling", -1) > 0,
         "spelling: colour is right, speling isn't");
  if (d)
    enchant_broker_free_dict (b, d);
  if (b)
    enchant_broker_free (b);

  /* MimeMap */
  t = mimemap (2, "text/csv");
  fprintf (out, "# MimeMap: text/csv &%03X, application/pdf &%03X, csv &%03X, svg &%03X\n",
           t & 0xFFF, mimemap (2, "application/pdf") & 0xFFF, mimemap (3, "csv") & 0xFFF,
           mimemap (3, "svg") & 0xFFF);
  check (t == 0xDFE, "MimeMap: text/csv is CSV (&DFE)");

  /* opening a file (an attachment's: untyped, its MIME type known) */
  p = g_build_filename (argv[2], "Page", NULL);
  g_file_set_contents (p, "<html><body style=\"background:#ffd\"><h1>Opened by gdk_riscos_open_file</h1>"
                       "<p>An untyped file, typed HTML from its MIME type, then Filer_Run.</p></body></html>\n", -1, NULL);
  {
    char *ro = gdk_riscos_path_from_unix (p);
    _kernel_swi_regs r;
    memset (&r, 0, sizeof r);
    r.r[0] = 18;
    r.r[1] = (int) ro;
    r.r[2] = 0xFFD;                       /* data: as UnixLib might leave it */
    _kernel_swi (0x08, &r, &r);
    check (gdk_riscos_open_file (p, "text/html", &err), "gdk_riscos_open_file: Filer_Run started");
    if (err)
      fprintf (out, "# %s\n", err->message);
    g_clear_error (&err);
    memset (&r, 0, sizeof r);
    r.r[0] = 5;
    r.r[1] = (int) ro;
    _kernel_swi (0x08, &r, &r);
    fprintf (out, "# Page is now &%03X\n", ((unsigned) r.r[2] >> 8) & 0xFFF);
    check ((((unsigned) r.r[2] >> 8) & 0xFFF) == 0xFAF, "and the file was typed HTML first");
    g_free (ro);
  }
  g_free (p);

  /* gtk_show_uri_on_window on a text file */
  p = g_build_filename (argv[2], "Notes.txt", NULL);
  g_file_set_contents (p, "Opened by gtk_show_uri_on_window on RISC OS.\n", -1, NULL);
  {
    char *uri = g_filename_to_uri (p, NULL, NULL);
    check (gtk_show_uri_on_window (NULL, uri, GDK_CURRENT_TIME, &err), "gtk_show_uri_on_window: a file:// URI opened");
    if (err)
      fprintf (out, "# %s\n", err->message);
    g_clear_error (&err);
    g_free (uri);
  }
  g_free (p);
  fprintf (out, "1..%d\n# featureprobe: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  fclose (out);
  /* stay a little, for the opened programs to come up */
  g_timeout_add (8000, quit, NULL);
  gtk_main ();
  return n_failed;
}
