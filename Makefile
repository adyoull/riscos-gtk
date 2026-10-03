# riscos-gtk. Needs a riscos-crossdev v1.2 checkout with its toolchain
# installed (make prebuilt) in RCD_DIR (default ../riscos-crossdev).
#
#   make deps       download the sources and cross-build the libraries
#   make glibtest   out/GLibTest-0.1.zip for the Pi (runs the checks too)
#   make check      the GLib test on Linux under qemu-arm-aligntrap
#   make clean      remove the build products (not the downloads)
export RCD_DIR ?= $(abspath ../riscos-crossdev)

all: deps glibtest check

deps:
	build/fetch-sources.sh
	build/build-deps.sh

glibtest: deps
	build/build-glibtest.sh

check:
	build/build-linuxarm.sh

clean:
	rm -rf src sysroot out src-linuxarm stage-linuxarm

.PHONY: all deps glibtest check clean
