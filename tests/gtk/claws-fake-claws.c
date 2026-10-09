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

int fake_claws_show_message (const char *folder, int num);
const char *fake_claws_viewer (GtkWidget **widget);
void fake_claws_remote_content (int on);
const char *fake_claws_theme (void);
int fake_claws_promote_html (void);
GdkPixbuf *fake_claws_stock (int which);

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
