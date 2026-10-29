/*
 * claws-fake-claws.c - riscos-gtk: claws-fake.c's calls into Claws Mail
 * that need its headers (compiled with them, linked with claws-fake.c).
 *
 * MIT licence.
 */
#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include "mainwindow.h"
#include "summaryview.h"
#include "messageview.h"
#include "mimeview.h"
#include "folder.h"
#include "prefs_common.h"
#include "stock_pixmap.h"
#include "plugins/litehtml_viewer/lh_prefs.h"
#include "about.h"
#include "compose.h"
#include "manual.h"
#include "plugin.h"
#include "folderview.h"
#include <enchant.h>

int fake_claws_show_message (const char *folder, int num);
const char *fake_claws_viewer (GtkWidget **widget);
void fake_claws_remote_content (int on);
const char *fake_claws_theme (void);
int fake_claws_promote_html (void);
GdkPixbuf *fake_claws_stock (int which);
void fake_claws_about (void);
int fake_claws_spell (void);
int fake_claws_manual (void);
int fake_claws_plugins (void);
GtkWidget *fake_claws_main_window (void);
int fake_claws_show_folder (const char *klass, const char *name, int msgnum);
void fake_claws_compose (const char *mailto);
int fake_claws_subscribe (const char *klass, const char *uri);
int fake_claws_folder_msgs (const char *klass, const char *name, char *path, int len);

/* scan folder (an identifier, "#mh/Mailbox/inbox") and show message num in
   the main window */
int
fake_claws_show_message (const char *folder, int num)
{
  MainWindow *mainwin = mainwindow_get_mainwindow ();
  FolderItem *item = folder_find_item_from_identifier (folder);

  if (!mainwin || !item)
    return 0;
  folder_item_scan (item);
  summary_show (mainwin->summaryview, item, FALSE);
  summary_select_by_msgnum (mainwin->summaryview, num, TRUE);
  return 1;
}

/* the content type of the main window's MIME viewer, if one shows the
   message (NULL: the text view), and its widget */
const char *
fake_claws_viewer (GtkWidget **widget)
{
  MainWindow *mainwin = mainwindow_get_mainwindow ();
  MimeView *mv = mainwin && mainwin->messageview ? mainwin->messageview->mimeview : NULL;

  *widget = NULL;
  if (!mv || !mv->mimeviewer || mv->type != MIMEVIEW_VIEWER)
    return NULL;
  *widget = mv->mimeviewer->get_widget (mv->mimeviewer);
  return mv->mimeviewer->factory->content_types[0];
}

void
fake_claws_remote_content (int on)
{
  lh_prefs_get ()->enable_remote_content = on;
}

const char *
fake_claws_theme (void)
{
  return prefs_common.pixmap_theme_path;
}

int
fake_claws_promote_html (void)
{
  return prefs_common.promote_html_part;
}

/* 0: Get Mail's toolbar icon, 1: the inbox folder icon */
GdkPixbuf *
fake_claws_stock (int which)
{
  GdkPixbuf *pb = NULL;
  stock_pixbuf_gdk (which == 0 ? STOCK_PIXMAP_MAIL_RECEIVE : STOCK_PIXMAP_INBOX_CLOSE, &pb);
  return pb;
}

void
fake_claws_about (void)
{
  about_show ();
}

/* Spelling: 1 Choices' dictionary is en_GB (spelling on), 2 enchant finds
   the application's en_GB dictionary (share/hunspell) and it knows
   "colour" and not "speling", 4 a compose window checks its spelling */
int
fake_claws_spell (void)
{
  int r = 0;
  const GList *l;
  EnchantBroker *b;
  EnchantDict *d;

  if (prefs_common.enable_aspell && g_strcmp0 (prefs_common.dictionary, "en_GB") == 0)
    r |= 1;
  b = enchant_broker_init ();
  d = b ? enchant_broker_request_dict (b, "en_GB") : NULL;
  if (d && enchant_dict_check (d, "colour", -1) == 0 && enchant_dict_check (d, "speling", -1) > 0)
    r |= 2;
  if (d)
    enchant_broker_free_dict (b, d);
  if (b)
    enchant_broker_free (b);
  for (l = compose_get_compose_list (); l; l = l->next)
    if (((Compose *) l->data)->gtkaspell != NULL)
      r |= 4;
  return r;
}

/* Help > Manual: the manual in the application is there */
int
fake_claws_manual (void)
{
  return manual_available (MANUAL_MANUAL_CLAWS);
}

/* the plug-ins loaded: 1 RSSyl, 2 ManageSieve, 4 vCalendar, 8 AttachWarner,
   16 Address Keeper */
int
fake_claws_plugins (void)
{
  GSList *l;
  int r = 0;

  for (l = plugin_get_list (); l; l = l->next)
    {
      const char *n = plugin_get_name (l->data);
      if (g_strcmp0 (n, "RSSyl") == 0)
        r |= 1;
      else if (g_strcmp0 (n, "ManageSieve") == 0)
        r |= 2;
      else if (g_strcmp0 (n, "vCalendar") == 0)
        r |= 4;
      else if (g_strcmp0 (n, "AttachWarner") == 0)
        r |= 8;
      else if (g_strcmp0 (n, "Address Keeper") == 0)
        r |= 16;
    }
  return r;
}

static Folder *
folder_of_class (const char *klass)
{
  GList *l;

  for (l = folder_get_list (); l; l = l->next)
    {
      Folder *f = l->data;
      if (f->klass && g_strcmp0 (f->klass->idstr, klass) == 0)
        return f;
    }
  return NULL;
}

/* subscribe the folder of class klass ("rssyl", "vCalendar") to uri, as
   its folder menu's Subscribe does: -1 no such folder */
int
fake_claws_subscribe (const char *klass, const char *uri)
{
  Folder *f = folder_of_class (klass);

  if (!f || !f->klass->subscribe)
    return -1;
  return f->klass->subscribe (f, uri) ? 1 : 0;
}

static gboolean
find_named (GNode *node, gpointer data)
{
  gpointer *d = data;
  FolderItem *item = node->data;

  if (item && item->name && g_strcmp0 (item->name, d[0]) == 0)
    {
      d[1] = item;
      return TRUE;
    }
  return FALSE;
}

/* the messages in the folder item called name in the folder of class
   klass, once scanned (-1: no such item); its folder on disc into path */
int
fake_claws_folder_msgs (const char *klass, const char *name, char *path, int len)
{
  Folder *f = folder_of_class (klass);
  gpointer d[2] = { (gpointer) name, NULL };
  FolderItem *item;
  char *p;

  if (path && len)
    path[0] = 0;
  if (!f || !f->node)
    return -1;
  g_node_traverse (f->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, find_named, d);
  item = d[1];
  if (!item)
    return -1;
  folder_item_scan (item);
  p = folder_item_get_path (item);
  if (p && path)
    g_strlcpy (path, p, len);
  g_free (p);
  return item->total_msgs;
}

/* the main window (shown or not) */
GtkWidget *
fake_claws_main_window (void)
{
  MainWindow *mw = mainwindow_get_mainwindow ();
  return mw ? mw->window : NULL;
}

/* the main window shown on the folder item called name (in the folder of
   class klass), its message msgnum selected (0: none), for a screenshot */
int
fake_claws_show_folder (const char *klass, const char *name, int msgnum)
{
  MainWindow *mw = mainwindow_get_mainwindow ();
  Folder *f = folder_of_class (klass);
  gpointer d[2] = { (gpointer) name, NULL };

  if (!mw || !f || !f->node)
    return 0;
  g_node_traverse (f->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, find_named, d);
  if (!d[1])
    return 0;
  main_window_popup (mw);
  folderview_select (mw->folderview, d[1]);
  if (msgnum)
    summary_select_by_msgnum (mw->summaryview, msgnum, TRUE);
  return 1;
}

/* a compose window for a mailto: URL, as Write does */
void
fake_claws_compose (const char *mailto)
{
  compose_new (NULL, mailto, NULL);
}
