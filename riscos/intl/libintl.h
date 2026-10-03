/* libintl.h - riscos-gtk's minimal libintl: no message catalogues yet.
 * Every lookup returns the untranslated string (English), as GNU gettext
 * does when no catalogue is installed. Enough for GLib, pango and GTK,
 * which only need the functions to exist. MIT licence (see LICENSE). */
#ifndef RISCOS_GTK_LIBINTL_H
#define RISCOS_GTK_LIBINTL_H
#ifdef __cplusplus
extern "C" {
#endif
char *gettext(const char *msgid);
char *dgettext(const char *domain, const char *msgid);
char *dcgettext(const char *domain, const char *msgid, int category);
char *ngettext(const char *msgid, const char *plural, unsigned long n);
char *dngettext(const char *domain, const char *msgid, const char *plural, unsigned long n);
char *dcngettext(const char *domain, const char *msgid, const char *plural, unsigned long n, int category);
char *textdomain(const char *domain);
char *bindtextdomain(const char *domain, const char *dir);
char *bind_textdomain_codeset(const char *domain, const char *codeset);
#ifdef __cplusplus
}
#endif
#endif
