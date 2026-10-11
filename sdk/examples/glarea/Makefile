# riscos-gtk devkit example: OpenGL in a GtkGLArea. First, once per shell:
#   export RCD_DIR=/path/to/riscos-crossdev      (v1.5)
#   (riscos-mesa's devkit: crossdev's make devkit, or set RISCOS_MESA_DEVKIT)
#   . /path/to/riscos-gtk-sdk/env.sh
# then make: glarea.elf, then !GLArea and GLArea-1.00.zip in out/.
# (RISCOS_MESA_DEVKIT defaults to crossdev's devkit/env.) MIT licence.
NAME     = GLArea
PROGRAM  = glarea
VERSION  = 1.00
SOURCES  = glarea.c

ifndef RISCOS_GTK_SDK
$(error source the riscos-gtk devkit's env.sh first (see the top of this Makefile))
endif
ifndef RISCOS_GTK_GL_LIBS
$(error no riscos-mesa devkit: set RISCOS_MESA_DEVKIT, then source env.sh again)
endif

all: out/$(NAME)-$(VERSION).zip

# OpenGL: $(RISCOS_GTK_GL_LIBS) links riscos-mesa's EGL and OSMesa in
$(PROGRAM).elf: $(SOURCES)
	$(CC) $(RISCOS_GTK_CFLAGS) -Wall $(SOURCES) -o $@ $(RISCOS_GTK_GL_LIBS) $(RISCOS_GTK_LIBS)

out/$(NAME)-$(VERSION).zip: $(PROGRAM).elf $(wildcard app/*)
	riscos-gtk-package $(NAME) $(PROGRAM).elf $(VERSION) app out

clean:
	rm -rf $(PROGRAM).elf out

.PHONY: all clean
