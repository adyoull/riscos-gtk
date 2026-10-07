# Claws Mail's objects without its link (build/build-claws.sh): with
# __riscos, main.c calls riscos_claws_init (apps/claws/riscos-claws.c),
# which build-app.sh (or the test) links in.
#   make -C src -f Makefile -f objects.mk claws-built
#   make -C src -f Makefile -f objects.mk claws-objects
claws-built: $(BUILT_SOURCES)
claws-objects: $(claws_mail_OBJECTS)
