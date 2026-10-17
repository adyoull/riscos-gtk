/* A test plugin: the same symbol names as modb.c (staticmodule-test.c). MIT licence. */
const char go_plugin_header[] = "module a";
int helper_value (void);
int go_plugin_init (void) { return helper_value () + 1; }
