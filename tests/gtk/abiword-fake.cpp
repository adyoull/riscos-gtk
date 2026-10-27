/*
 * abiword-fake.cpp - AbiWord (3.0.8, the port) on the fake RISC OS, with
 * its file format plugins built in and apps/abiword/riscos-abiword.cpp's
 * start:
 *   - it starts with a document window (its data, the dialogue .ui files
 *     and strings, from ABIWORD_DATADIR);
 *   - keys typed on the fake desktop go into the document;
 *   - the document saves as OpenDocument (.odt), Word (.docx, .doc), RTF,
 *     HTML and AbiWord's .abw (built-in plugins and AbiWord's own), and
 *     each opens again with the text in it;
 *   - a file opened through the icon bar (riscos-gtk's open_file handler)
 *     gets a window of its own when the current one has been typed in;
 *   - closing the windows quits.
 *   abiword-fake APPDIR OUTDIR
 * APPDIR is the packaged !AbiWord's folders (share, fonts) for the
 * linuxarm build. AB_SHOT=FILE.ppm: a picture of the screen after typing.
 * Prints TAP. MIT licence.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xap_App.h"
#include "xap_Frame.h"
#include "fv_View.h"
#include "pd_Document.h"
#include "ie_exp.h"
#include "ie_imp.h"
#include "ut_string_class.h"
#include "ev_EditMethod.h"
#include "gr_CairoGraphics.h"
#include "spell_manager.h"
#include "xap_EncodingManager.h"
#include <iconv.h>

extern int riscos_abiword_main (int argc, char **argv);

static int n_tests, n_failed, stage, waits;
static const char *outdir;
static int started, typed_ok, files_done, iconbar_ok, frames_before, frames_after;
static int saved[8], reopened[8];
static const char *const suffixes[] = { ".odt", ".docx", ".doc", ".rtf", ".html", ".abw" };
#define N_FORMATS 6

/* RTF's reader and the text writer convert through iconv to the 8-bit
   character set (ISO-8859-1); the test machine's glibc may not have that
   converter (gconv modules for armhf), RISC OS's Iconv module does */
static int iconv_ok = 1;

static void
check_iconv (int ok, const char *name)
{
  if (iconv_ok)
    {
      n_tests++;
      if (!ok)
        n_failed++;
      printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
    }
  else
    printf ("ok %d - %s # SKIP no ISO-8859-1 converter in this glibc\n", ++n_tests, name);
  fflush (stdout);
}

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (stdout);
}

static void
add (int reason, int a, int b, int c)
{
  int *e;
  if (fake_wimp_script_len >= FAKE_SCRIPT_MAX)
    return;                    /* (past the end it would write over memory) */
  e = fake_wimp_script[fake_wimp_script_len++];
  e[0] = reason; e[1] = a; e[2] = b; e[3] = c;
}

static void
nulls (int n)
{
  while (n--)
    add (FAKE_NULL, 0, 0, 0);
}

static XAP_Frame *
frame0 (void)
{
  XAP_App *app = XAP_App::getApp ();
  return app && app->getFrameCount () > 0 ? app->getFrame (0) : NULL;
}

/* the document's text: as RTF, whose text is ASCII here */
static char *
text_of (PD_Document *doc)
{
  char *path = g_build_filename (outdir, "text-of.rtf", NULL);
  char *data = NULL;
  IEFileType ft = IE_Exp::fileTypeForSuffix (".rtf");
  UT_Error err = static_cast<AD_Document *> (doc)->saveAs (path, ft, true);

  if (err != UT_OK || !g_file_get_contents (path, &data, NULL, NULL))
    data = g_strdup ("");
  g_free (path);
  return data;
}

/* plain text export (File > Save As .txt) */
static int txt_ok;
static void
check_txt (PD_Document *doc)
{
  char *path = g_build_filename (outdir, "test.txt", NULL);
  char *data = NULL;
  UT_Error err = static_cast<AD_Document *> (doc)->saveAs (path, IE_Exp::fileTypeForSuffix (".txt"), true);

  txt_ok = err == UT_OK && g_file_get_contents (path, &data, NULL, NULL) &&
           strstr (data, "Hello from AbiWord on RISC OS.") != NULL;
  {
    XAP_EncodingManager *em = XAP_EncodingManager::get_instance ();
    const char *n8 = em->getNative8BitEncodingName (), *nn = em->getNativeEncodingName ();
    iconv_t c = iconv_open (n8, "UTF-8");
    printf ("# encodings: native 8-bit %s, native %s, UCS-4 name %s; iconv to it %s\n", n8, nn,
            ucs4Internal (), c == (iconv_t) -1 ? "fails" : "works");
    if (c != (iconv_t) -1)
      iconv_close (c);
    else
      iconv_ok = 0;
  }
  printf ("# .txt: error %d, \"%.60s\"\n", (int) err, data ? data : "");
  g_free (data);
  g_free (path);
}

static void
shot (const char *path)
{
  FILE *f = fopen (path, "wb");
  int x, y;

  if (!f)
    return;
  fprintf (f, "P6\n%d %d\n255\n", fake_screen.w, fake_screen.h);
  for (y = 0; y < fake_screen.h; y++)
    for (x = 0; x < fake_screen.w; x++)
      {
        unsigned int p = fake_screen_pixel (x, y);
        /* the fake's usual mode is TBGR: &00BBGGRR */
        fputc (p & 0xFF, f); fputc ((p >> 8) & 0xFF, f); fputc ((p >> 16) & 0xFF, f);
      }
  fclose (f);
}

/* fonts: AbiWord's font list, and what pango gives AbiWord for a family */
static int fontlist_ok, serif_ok, mono_ok, fonts_tested;
static char *
face_for (GR_Graphics *g, const char *family)
{
  GR_Font *f = g->findFont (family, "normal", "", "normal", "", "12pt", NULL);
  GR_PangoFont *pf = dynamic_cast<GR_PangoFont *> (f);
  PangoFontDescription *d;
  char *r;

  if (!pf || !pf->getPangoFont ())
    return g_strdup ("(none)");
  d = pango_font_describe (pf->getPangoFont ());
  r = g_strdup (pango_font_description_get_family (d));
  pango_font_description_free (d);
  return r;
}

static void
check_fonts (FV_View *view)
{
  GR_CairoGraphics *g = dynamic_cast<GR_CairoGraphics *> (view->getGraphics ());
  char *serif, *mono, *sans;
  size_t i;
  bool has_serif = false, has_mono = false;

  if (!g)
    return;
  fonts_tested = 1;
  const std::vector<std::string> &names = GR_CairoGraphics::getAllFontNames ();
  for (i = 0; i < names.size (); i++)
    {
      has_serif = has_serif || names[i] == "DejaVu Serif";
      has_mono = has_mono || names[i] == "DejaVu Sans Mono";
    }
  printf ("# font list: %d families:", (int) names.size ());
  for (i = 0; i < names.size () && i < 12; i++)
    printf (" %s;", names[i].c_str ());
  printf ("\n");
  fontlist_ok = has_serif && has_mono;
  sans = face_for (g, "DejaVu Sans");
  serif = face_for (g, "DejaVu Serif");
  mono = face_for (g, "DejaVu Sans Mono");
  printf ("# faces: DejaVu Sans -> %s, DejaVu Serif -> %s, DejaVu Sans Mono -> %s\n", sans, serif, mono);
  serif_ok = strcmp (serif, "DejaVu Serif") == 0;
  mono_ok = strcmp (mono, "DejaVu Sans Mono") == 0;
  g_free (sans); g_free (serif); g_free (mono);
}

/* spelling: AbiWord's checker (enchant, hunspell built in) with the
   program's en_GB dictionary, and one added in <Choices$Write>.GTK.
   Dictionaries (here en_ZZ, a copy of en_GB) */
static int spell_gb_ok, spell_bad_ok, spell_choices_ok;
static int
spelt_ok (SpellChecker *c, const char *word)
{
  UT_UCS4String w (word);
  return c && c->checkWord (w.ucs4_str (), w.size ()) == SpellChecker::LOOKUP_SUCCEEDED;
}

static void
check_spelling (void)
{
  SpellChecker *gb = SpellManager::instance ().requestDictionary ("en-GB");
  SpellChecker *zz = SpellManager::instance ().requestDictionary ("en-ZZ");

  spell_gb_ok = spelt_ok (gb, "colour") && spelt_ok (gb, "organise");
  spell_bad_ok = gb && !spelt_ok (gb, "colur");
  spell_choices_ok = spelt_ok (zz, "colour");
  printf ("# spelling: en-GB %s, en-ZZ (Choices) %s\n", gb ? "loaded" : "missing", zz ? "loaded" : "missing");
}

static gboolean
do_files (gpointer)
{
  XAP_Frame *f = frame0 ();
  PD_Document *doc = f ? static_cast<PD_Document *> (f->getCurrentDoc ()) : NULL;
  FV_View *view = f ? static_cast<FV_View *> (f->getCurrentView ()) : NULL;
  const GdkRiscosIconbarHandlers *h;
  gpointer hd;
  char *t;
  int i;

  if (!doc || !view)
    {
      files_done = 1;
      return G_SOURCE_REMOVE;
    }
  t = text_of (doc);
  typed_ok = strstr (t, "Risc") != NULL;
  printf ("# after typing: %s\n", typed_ok ? "Risc found" : "no Risc");
  g_free (t);
  view->cmdCharInsert (std::string (" Hello from AbiWord on RISC OS."));
  if (getenv ("UnixFC$Dir"))
    check_fonts (view);
  check_spelling ();
  check_txt (doc);
  for (i = 0; i < N_FORMATS; i++)
    {
      char *name = g_strconcat ("test", suffixes[i], NULL);
      char *file = g_build_filename (outdir, name, NULL);
      /* URIs, as AbiWord's Save and Open boxes give */
      char *path = g_filename_to_uri (file, NULL, NULL);
      IEFileType ft = IE_Exp::fileTypeForSuffix (suffixes[i]);
      UT_Error err;
      printf ("# saving %s\n", suffixes[i]);
      fflush (stdout);
      err = static_cast<AD_Document *> (doc)->saveAs (path, ft, true);
      saved[i] = err == UT_OK && g_file_test (file, G_FILE_TEST_IS_REGULAR);
      if (saved[i])
        {
          PD_Document *d = new PD_Document ();
          if (d->readFromFile (path, IEFT_Unknown) == UT_OK)
            {
              char *text = text_of (d);
              reopened[i] = strstr (text, "Hello from AbiWord on RISC OS.") != NULL;
              g_free (text);
            }
          UNREFP (d);
        }
      printf ("# %s: file type %d, saved %d, reopened %d\n", suffixes[i], (int) ft, saved[i], reopened[i]);
      g_free (name);
      g_free (path);
      g_free (file);
    }
  /* the icon bar: a file dropped on the icon, the current window typed in */
  h = gdk_riscos_iconbar_get_handlers (&hd);
  frames_before = XAP_App::getApp ()->getFrameCount ();
  if (h && h->open_file && saved[0])
    {
      char *path = g_build_filename (outdir, "test.odt", NULL);
      iconbar_ok = h->open_file (path, 0xFFF, FALSE, hd);
      g_free (path);
    }
  frames_after = XAP_App::getApp ()->getFrameCount ();
  files_done = 1;
  return G_SOURCE_REMOVE;
}

/* close the icon bar's window (File > Close), then File > Quit: with one
   window, saved, AbiWord asks nothing */
static gboolean
quit_idle (gpointer)
{
  XAP_App *app = XAP_App::getApp ();

  if (app && app->getFrameCount () > 1)
    {
      ev_EditMethod_invoke ("closeWindow", "");
      return G_SOURCE_CONTINUE;
    }
  ev_EditMethod_invoke ("querySaveAndExit", "");
  return G_SOURCE_REMOVE;
}

static void
hook (int reason)
{
  XAP_Frame *f;
  XAP_App *app;
  UT_sint32 i;

  if (fake_wimp_script_pos () < fake_wimp_script_len - 1)
    return;
  switch (stage)
    {
    case 0:
      f = frame0 ();
      if (!f || fake_plots == 0)
        {
          if (++waits > 2000)
            stage = 99;
          else
            add (FAKE_NULL, 0, 0, 0);
          break;
        }
      started = 1;
      stage = 1;
      /* scripted events to AbiWord's window (the newest window made may
         be another, never opened) */
      {
        int k, best = -1;
        for (k = 0; k < FAKE_MAX_WINDOWS; k++)
          if (fake_windows[k].handle && (best < 0 || fake_windows[k].stack > fake_windows[best].stack))
            best = k;
        if (best >= 0)
          fake_target_window = fake_windows[best].handle;
      }
      /* typing: the document has the caret */
      {
        const char *s = "Risc";
        for (; *s; s++)
          {
            add (8, *s, 0, 0);
            nulls (2);
          }
        for (i = 0; i < 10; i++) { add (FAKE_SLEEP, 30, 0, 0); add (FAKE_NULL, 0, 0, 0); }
      }
      break;
    case 1:
      if (getenv ("AB_SHOT"))
        shot (getenv ("AB_SHOT"));
      stage = 2;
      g_idle_add (do_files, NULL);
      g_main_context_wakeup (NULL);
      nulls (10);
      break;
    case 2:
      if (!files_done)
        {
          nulls (5);
          break;
        }
      /* no "save changes?" when the windows close */
      app = XAP_App::getApp ();
      for (i = 0; app && i < app->getFrameCount (); i++)
        {
          PD_Document *d = static_cast<PD_Document *> (app->getFrame (i)->getCurrentDoc ());
          if (d)
            d->setClean ();
        }
      stage = 3;
      g_idle_add (quit_idle, NULL);
      g_main_context_wakeup (NULL);
      nulls (10);
      break;
    case 3:
      nulls (5);               /* until AbiWord has gone */
      break;
    }
}

int
main (int argc, char **argv)
{
  char *a[] = { argv[0], NULL };
  int status, i;

  if (argc < 3)
    return 2;
  outdir = argv[2];
  /* the system's fonts through a stand-in UnixFC (as gnumeric-fake) */
  if (argc > 3)
    {
      char *fc = g_build_filename (outdir, "UnixFC", NULL);
      char *fcfonts = g_build_filename (fc, "fonts", NULL);
      char *sys = g_build_filename (outdir, "UnixFont", NULL);
      char *cache = g_build_filename (outdir, "UnixFC-cache", NULL);
      char *text, *sysconf;
      int k;

      g_mkdir_with_parents (fcfonts, 0755);
      g_mkdir_with_parents (sys, 0755);
      g_mkdir_with_parents (cache, 0755);
      for (k = 3; k < argc; k++)
        {
          char *data, *base = g_path_get_basename (argv[k]);
          char *dst = g_build_filename (sys, base, NULL);
          gsize len;
          if (g_file_get_contents (argv[k], &data, &len, NULL))
            {
              g_file_set_contents (dst, data, len, NULL);
              g_free (data);
            }
          g_free (dst);
          g_free (base);
        }
      text = g_strdup_printf ("<?xml version=\"1.0\"?>\n<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
                              "<fontconfig>\n  <dir>%s</dir>\n  <cachedir>%s</cachedir>\n</fontconfig>\n", sys, cache);
      sysconf = g_build_filename (fcfonts, "fonts.conf", NULL);
      g_file_set_contents (sysconf, text, -1, NULL);
      g_setenv ("UnixFC$Dir", fc, TRUE);
      g_free (text); g_free (sysconf); g_free (fc); g_free (fcfonts); g_free (sys); g_free (cache);
    }
  g_setenv ("ABIWORD_DATADIR", g_build_filename (argv[1], "share", "abiword-3.0", NULL), TRUE);
  g_setenv ("FONTCONFIG_FILE", g_build_filename (argv[1], "fonts", "fonts.conf", NULL), TRUE);
  g_setenv ("XDG_DATA_DIRS", g_build_filename (argv[1], "share", NULL), TRUE);
  g_setenv ("XDG_CONFIG_HOME", outdir, TRUE);
  g_setenv ("ABIWORD_NO_LOG", "1", TRUE);
  /* a dictionary the user added to <Choices$Write>.GTK.Dictionaries */
  {
    char *choices = g_build_filename (outdir, "Choices", NULL);
    char *dd = g_build_filename (choices, "GTK", "Dictionaries", NULL);
    const char *ext[] = { "dic", "aff" };
    int k;
    g_mkdir_with_parents (dd, 0755);
    for (k = 0; k < 2; k++)
      {
        char *name = g_strdup_printf ("en_GB.%s", ext[k]), *zz = g_strdup_printf ("en_ZZ.%s", ext[k]);
        char *src = g_build_filename (argv[1], "share", "hunspell", name, NULL);
        char *dst = g_build_filename (dd, zz, NULL), *data;
        gsize len;
        if (g_file_get_contents (src, &data, &len, NULL))
          {
            g_file_set_contents (dst, data, len, NULL);
            g_free (data);
          }
        g_free (name); g_free (zz); g_free (src); g_free (dst);
      }
    g_setenv ("Choices$Write", choices, TRUE);
    g_free (choices); g_free (dd);
  }
  fake_set_screen (1280, 1024, 0, 5);
  fake_wimp_nulls = 40;
  fake_wimp_hook = hook;
  status = riscos_abiword_main (1, a);

  check (started, "AbiWord starts with a document window");
  check (typed_ok, "keys typed on the desktop go into the document");
  for (i = 0; i < N_FORMATS; i++)
    {
      char what[96];
      g_snprintf (what, sizeof what, "saved as %s, and it opens again with the text", suffixes[i]);
      if (strcmp (suffixes[i], ".rtf") == 0 || strcmp (suffixes[i], ".doc") == 0)
        check_iconv (saved[i] && reopened[i], what);   /* (AbiWord writes .doc as RTF) */
      else
        check (saved[i] && reopened[i], what);
    }
  check_iconv (txt_ok, "saved as plain text (.txt)");
  if (fonts_tested)
    {
      check (fontlist_ok, "with UnixFC, AbiWord's font list has the system's fonts (DejaVu Serif, Sans Mono)");
      check (serif_ok, "and DejaVu Serif is drawn in DejaVu Serif");
      check (mono_ok, "and DejaVu Sans Mono in DejaVu Sans Mono");
    }
  check (spell_gb_ok, "spelling: British English words pass (the program's en_GB dictionary)");
  check (spell_bad_ok, "and a misspelt word is caught");
  check (spell_choices_ok, "a dictionary added to <Choices$Write>.GTK.Dictionaries is found");
  check (iconbar_ok && frames_after == frames_before + 1,
         "a file dropped on the icon bar icon opens in a window of its own");
  check (stage == 3, "File > Quit with the documents saved quits AbiWord");
  check (status == 0, "and AbiWord exits normally");
  printf ("# exit status %d, %d -> %d windows\n", status, frames_before, frames_after);
  printf ("1..%d\n", n_tests);
  return n_failed;
}
