/* A test plugin: the same symbol names as moda.c. MIT licence. */
const char go_plugin_header[] = "module b";
int helper_value (void) { return 200; }
int go_plugin_init (void) { return helper_value () + 2; }
