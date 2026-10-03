/* intl.c - riscos-gtk's minimal libintl (see libintl.h). MIT licence. */
#include <string.h>
#include "libintl.h"

static char current_domain[64] = "messages";

char *gettext(const char *msgid) { return (char *) msgid; }
char *dgettext(const char *d, const char *msgid) { (void) d; return (char *) msgid; }
char *dcgettext(const char *d, const char *msgid, int c) { (void) d; (void) c; return (char *) msgid; }
char *ngettext(const char *msgid, const char *plural, unsigned long n)
{ return (char *) (n == 1 ? msgid : plural); }
char *dngettext(const char *d, const char *msgid, const char *plural, unsigned long n)
{ (void) d; return ngettext(msgid, plural, n); }
char *dcngettext(const char *d, const char *msgid, const char *plural, unsigned long n, int c)
{ (void) d; (void) c; return ngettext(msgid, plural, n); }

char *textdomain(const char *domain)
{
  if (domain && *domain) {
    strncpy(current_domain, domain, sizeof current_domain - 1);
    current_domain[sizeof current_domain - 1] = '\0';
  }
  return current_domain;
}
/* No catalogues: report the directory/codeset back as GNU gettext would. */
char *bindtextdomain(const char *d, const char *dir) { (void) d; return (char *) (dir ? dir : "/<Locale$Dir>"); }
char *bind_textdomain_codeset(const char *d, const char *cs) { (void) d; return (char *) (cs ? cs : "UTF-8"); }
