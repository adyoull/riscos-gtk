/*
 * staticmodule-test.c - GLib's static module registry (riscos-gtk's GLib
 * patch, g_module_register_static) with tools/mkstaticmodules.py: two
 * plugins defining the same names (go_plugin_init, go_plugin_header,
 * helper_value) linked into one program, each found by g_module_open() of
 * a path ending in its name, with or without a suffix; a missing symbol
 * and an unregistered module fail; g_module_open (NULL) gives the program.
 * Prints TAP. MIT licence.
 */
#include <gmodule.h>
#include <stdio.h>
#include <string.h>

void riscos_register_static_modules (void);

static int n_tests, n_failed;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
}

int
main (void)
{
  GModule *a, *b, *a2, *none;
  int (*init) (void) = NULL;
  const char *header = NULL;
  gpointer missing = (gpointer) 1;

  riscos_register_static_modules ();
  check (g_module_supported (), "modules are supported once some are built in");
  a = g_module_open ("/apps/!Prog/plugins/plug-a/plugin", G_MODULE_BIND_LAZY);
  b = g_module_open ("/apps/!Prog/plugins/plug-b/plugin.so", G_MODULE_BIND_LAZY);
  check (a && b && a != b, "two built-in modules open by their paths (with and without .so)");
  check (a && g_module_symbol (a, "go_plugin_init", (gpointer *) &init) && init && init () == 101,
         "module a's go_plugin_init, calling its own helper in another object");
  check (b && g_module_symbol (b, "go_plugin_init", (gpointer *) &init) && init && init () == 202,
         "module b's go_plugin_init (the same name), with its own helper");
  check (b && g_module_symbol (b, "go_plugin_header", (gpointer *) &header) && header &&
         strcmp (header, "module b") == 0, "data symbols too");
  check (a && !g_module_symbol (a, "no_such_symbol", &missing) && missing == NULL &&
         g_module_error () != NULL, "a symbol the module doesn't have: FALSE, with an error");
  a2 = g_module_open ("plug-a/plugin", G_MODULE_BIND_LAZY);
  check (a2 == a, "opening it again gives the same module");
  none = g_module_open ("/apps/!Prog/plugins/plug-c/plugin", G_MODULE_BIND_LAZY);
  check (none == NULL, "a module that isn't built in doesn't open");
  check (g_module_open ("/x/myplug-a/plugin", 0) == NULL, "the name matches whole path parts only");
  {
    GModule *self = g_module_open (NULL, 0);
    gpointer f = (gpointer) 1;
    check (self && !g_module_symbol (self, "no_such_symbol_here", &f) && f == NULL && g_module_close (self),
           "the program itself opens (as GTK asks), and a missing symbol isn't found");
  }
  if (a2)
    g_module_close (a2);
  check (a && g_module_close (a) && b && g_module_close (b), "built-in modules close");
  printf ("1..%d\n", n_tests);
  return n_failed;
}
