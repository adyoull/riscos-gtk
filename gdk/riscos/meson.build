# riscos-gtk: the RISC OS GDK backend (Broadway's client side + the Wimp
# server in gdkriscos-server.c). Copied into gdk/riscos/ of the GTK tree by
# riscos-gtk's build/build-gtk.sh.
gdk_riscos_sources = files(
  'gdkcursor-riscos.c',
  'gdkdevice-riscos.c',
  'gdkdevicemanager-riscos.c',
  'gdkdisplay-riscos.c',
  'gdkdnd-riscos.c',
  'gdkeventsource.c',
  'gdkglobals-riscos.c',
  'gdkkeys-riscos.c',
  'gdkmonitor-riscos.c',
  'gdkproperty-riscos.c',
  'gdkscreen-riscos.c',
  'gdkselection-riscos.c',
  'gdktestutils-riscos.c',
  'gdkvisual-riscos.c',
  'gdkwindow-riscos.c',
  'gdkriscos-server.c',
)

gdk_riscos_public_headers = files(
  'gdkriscosdisplay.h',
  'gdkriscoswindow.h',
  'gdkriscoscursor.h',
  'gdkriscosmonitor.h',
  'gdkriscosvisual.h',
  'gdkriscosmenu.h',
)

install_headers(gdk_riscos_public_headers, subdir: 'gtk-3.0/gdk/riscos/')
install_headers('gdkriscos.h', subdir: 'gtk-3.0/gdk/')

gdk_riscos_deps = []

libgdk_riscos = static_library('gdk-riscos',
  gdk_riscos_sources, gdkconfig, gdkenum_h,
  include_directories: [confinc, gdkinc],
  c_args: [
    '-DGDK_COMPILATION',
    '-DG_LOG_DOMAIN="Gdk"',
  ] + common_cflags,
  link_args: common_ldflags,
  dependencies: [gdk_deps, gdk_riscos_deps])
