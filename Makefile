# riscos-gtk. Needs a riscos-crossdev v1.3 checkout with its toolchain
# installed (make prebuilt) in RCD_DIR (default ../riscos-crossdev).
#
#   make deps       download the sources and cross-build the libraries
#   make tests      out/GTKTest-0.5.zip for the Pi (runs the checks too)
#   make hello      out/GTKHello-0.1.zip: GTK on the Wimp (M2)
#   make check      the same tests as ARM Linux code under qemu-arm-aligntrap
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

check:
	build/build-linuxarm.sh

clean:
	rm -rf src sysroot out src-linuxarm sysroot-linuxarm

.PHONY: all deps tests hello check clean
