# Claws Mail's objects without its link (build/build-claws.sh): with
# __riscos, main.c calls riscos_claws_init (apps/claws/riscos-claws.c),
# which build-app.sh (or the test) links in.
#   make -C src -f Makefile -f objects.mk claws-built
#   make -C src -f Makefile -f objects.mk claws-objects
claws-built: $(BUILT_SOURCES)
claws-objects: $(claws_mail_OBJECTS)
# a plug-in's objects without its libtool link (build-claws.sh links them
# in as GLib static modules):
#   make -C src/plugins/rssyl -f Makefile -f objects.mk plugin-objects
plugin-objects: $(libfeed_la_OBJECTS) $(rssyl_la_OBJECTS) $(managesieve_la_OBJECTS) $(vcalendar_la_OBJECTS)
