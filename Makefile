# riscos-gtk. Needs a riscos-crossdev v1.5 checkout with its toolchain
# installed (make prebuilt) in RCD_DIR (default ../riscos-crossdev).
#
#   make deps       download the sources and cross-build the libraries
#   make tests      out/GTKTest-0.5.zip for the Pi (runs the checks too)
#   make hello      out/GTKHello-0.5.zip: GTK on the Wimp (M2)
#   make l3afpad    out/L3afpad-<v>.zip       (the programs; versions in
#   make themetest  out/GTKThemeTest-<v>.zip   build/build-<program>.sh)
#   make claws      out/ClawsMail-<v>.zip
#   make geany      out/Geany-<v>.zip: the Geany 2.1 demo
#   make mtpaint    out/mtPaint-<v>.zip: mtPaint 3.50.14
#   make gnumeric   out/Gnumeric-<v>.zip: Gnumeric 1.12.57 (a test build)
#   make emoji      out/TTF-Noto-Color-Emoji-<v>.zip: the optional colour
#                   emoji font, a PackMan package for UnixFont (needs
#                   FontConfig; build/build-emoji.sh)
#   make gltest     out/GTKGLTest-<v>.zip: GtkGLArea with riscos-mesa (needs
#                   its devkit in MESA_DEVKIT, default $RCD_DIR/devkit/env)
#   make sdk        out/sdk/riscos-gtk-sdk-<RELEASE or dev-commit>.tar.gz:
#                   the devkit for other developers (build/make-sdk.sh)
#   make check      the same tests as ARM Linux code under qemu-arm-aligntrap
#                   (the GL test needs riscos-mesa's devkit in MESA_DEVKIT)
#   make release RELEASE=3.24.41-riscos1.0-rc6
#                   out/release/<RELEASE>/: the programs, the devkit, the
#                   complete source and MANIFEST.txt (build/make-release.sh)
#   make clean      remove the build products (not the downloads)
export RCD_DIR ?= $(abspath ../riscos-crossdev)

all: deps tests hello check

deps:
	build/check-host.sh
	build/fetch-sources.sh
	build/build-deps.sh

tests: deps
	build/build-tests.sh

hello: deps
	build/build-hello.sh

l3afpad: deps
	build/build-l3afpad.sh

themetest: deps
	build/build-themetest.sh

claws: deps
	build/build-claws.sh

gltest: deps
	build/build-gltest.sh

geany: deps
	build/build-geany.sh

mtpaint: deps
	build/build-mtpaint.sh

gnumeric: deps
	build/build-gnumeric.sh

spell: deps
	build/build-spell.sh

abiword: deps
	build/build-abiword.sh

emoji:
	build/fetch-sources.sh
	build/build-emoji.sh

check:
	build/check-host.sh check
	build/build-linuxarm.sh

release: deps
	build/make-release.sh $(RELEASE)

sdk: deps
	build/make-sdk.sh $(RELEASE)

clean:
	rm -rf src sysroot out src-linuxarm sysroot-linuxarm

.PHONY: all deps tests hello l3afpad themetest claws gltest geany mtpaint gnumeric spell abiword emoji check release sdk clean
