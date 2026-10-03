/* dl.c - riscos-gtk's libdl: UnixLib declares dlopen() and friends in
 * <dlfcn.h> but has no dynamic loading, so the functions don't exist.
 * libepoxy calls them to find GL libraries; here every dlopen fails
 * cleanly, so epoxy reports no GL (GtkGLArea isn't supported yet; a later
 * version can answer from riscos-mesa's EGL instead). MIT licence. */
#include <dlfcn.h>
#include <stddef.h>

static const char *dl_error_message;

void *
dlopen (const char *file, int mode)
{
  (void) file; (void) mode;
  dl_error_message = "No dynamic loading on RISC OS";
  return NULL;
}

void *
dlsym (void *handle, const char *name)
{
  (void) handle; (void) name;
  dl_error_message = "No dynamic loading on RISC OS";
  return NULL;
}

int
dlclose (void *handle)
{
  (void) handle;
  return 0;
}

const char *
dlerror (void)
{
  const char *m = dl_error_message;
  dl_error_message = NULL;
  return m;
}
