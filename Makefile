# riscos-gtk. Needs a riscos-crossdev v1.3 checkout with its toolchain
# installed (make prebuilt) in RCD_DIR (default ../riscos-crossdev).
#
#   make deps       download the sources and cross-build the libraries
#   make tests      out/GTKTest-0.5.zip for the Pi (runs the checks too)
#   make hello      out/GTKHello-0.5.zip: GTK on the Wimp (M2)
#   make l3afpad    out/L3afpad-<v>.zip       (the programs; versions in
#   make themetest  out/GTKThemeTest-<v>.zip   build/build-<program>.sh)
#   make claws      out/ClawsMail-<v>.zip
#   make sdk        out/sdk/riscos-gtk-sdk-<RELEASE or dev-commit>.tar.gz:
#                   the devkit for other developers (build/make-sdk.sh)
#   make check      the same tests as ARM Linux code under qemu-arm-aligntrap
#   make release RELEASE=3.24.41-riscos1.0-rc4
#                   out/release/<RELEASE>/: the programs, the devkit, the
#                   complete source and MANIFEST.txt (build/make-release.sh)
#   make clean      remove the build products (not the downloads)
export RCD_DIR ?= $(abspath ../riscos-crossdev)

all: deps tests hello check

deps:
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

check:
	build/build-linuxarm.sh

release: deps
	build/make-release.sh $(RELEASE)

sdk: deps
	build/make-sdk.sh $(RELEASE)

clean:
	rm -rf src sysroot out src-linuxarm sysroot-linuxarm

.PHONY: all deps tests hello l3afpad themetest claws check release sdk clean
