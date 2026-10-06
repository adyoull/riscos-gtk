/* config.h for L3afpad on RISC OS (riscos-gtk; written by hand instead of
   configure). GPL version 2 or later, as L3afpad. */
#define PACKAGE "l3afpad"
#define PACKAGE_NAME "L3afpad"
#define PACKAGE_VERSION "0.8.18.1.11"
#define PACKAGE_STRING "L3afpad 0.8.18.1.11"
#define VERSION "0.8.18.1.11"
#define GETTEXT_PACKAGE "l3afpad"
#define LOCALEDIR "/<L3afpad$Dir>/locale"
/* no print backends on RISC OS yet */
#define ENABLE_PRINT 0
#define ENABLE_XINPUT2 0
/* the window icon (L3afpad's PNG, in the application) */
#ifndef ICONDIR
#define ICONDIR "/<L3afpad$Dir>/share/icons"
#endif
