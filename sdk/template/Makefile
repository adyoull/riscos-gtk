# riscos-gtk devkit template. First, once per shell:
#   export RCD_DIR=/path/to/riscos-crossdev      (v1.5)
#   . /path/to/riscos-gtk-sdk/env.sh
# then:
#   make            the program (hello.elf), then !Hello and Hello-1.00.zip in out/
#   make clean
# Change NAME, PROGRAM, VERSION and SOURCES for your own program, and the
# files in app/ (its !Run, !Help, sprites and licence). MIT licence.
NAME     = Hello
PROGRAM  = hello
VERSION  = 1.00
SOURCES  = hello.c
CFLAGS_EXTRA = -Wall -Wno-deprecated-declarations
LIBS_EXTRA =

ifndef RISCOS_GTK_SDK
$(error source the riscos-gtk devkit's env.sh first (see the top of this Makefile))
endif

all: out/$(NAME)-$(VERSION).zip

$(PROGRAM).elf: $(SOURCES)
	$(CC) $(RISCOS_GTK_CFLAGS) $(CFLAGS_EXTRA) $(SOURCES) -o $@ $(LIBS_EXTRA) $(RISCOS_GTK_LIBS)

# riscos-crossdev's checks (static linking, stack probes, FPU, UnixLib),
# then the application and its zip
out/$(NAME)-$(VERSION).zip: $(PROGRAM).elf $(wildcard app/*)
	riscos-gtk-package $(NAME) $(PROGRAM).elf $(VERSION) app out

# GCC's warnings for code that may make unaligned accesses (RISC OS
# aborts on them; see the devkit's docs/PORTING.md)
check-unaligned:
	$(RCD_DIR)/checks/check-unaligned.sh $(shell pkg-config --cflags gtk+-3.0) $(SOURCES)

clean:
	rm -rf $(PROGRAM).elf out

.PHONY: all clean check-unaligned
