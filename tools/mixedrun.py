#!/usr/bin/env python3
"""mixedrun.py RUNFILE NAME PROG SLOT - riscos-gtk: a program's !Run for a
mixed-linked build (RISCOS_GTK_MIXED=1; build/build-app.sh and
build/build-mixed.sh): after "Set NAME$Dir", !SharedLibs (with
ld-riscos/so/2) and !GTKLibs are booted from !Boot.Resources if they
haven't been, and checked; the WimpSlot becomes SLOT K (the program
only: the libraries are in SOManager's dynamic area), and PROG$LD$Env
points the loader at !GTKLibs. MIT licence.
"""
import re, sys
p, name, prog, slot = sys.argv[1:5]
s = open(p).read()
checks = (
  "| MIXED-LINK TEST (riscos-crossdev 1.11-test1): %(prog)s,e1f is an ELF program\n"
  "| with UnixLib linked in; GTK and the libraries under it are the shared\n"
  "| libraries in !GTKLibs, loaded by !SharedLibs (SOManager, ld-riscos/so/2)\n"
  "| !SharedLibs and !GTKLibs are installed in !Boot.Resources (PackMan);\n"
  "| the boot sequence boots them, and if it hasn't yet, this does\n"
  'If "<SharedLibs$Dir>" = "" Then IfThere Boot:Resources.!SharedLibs.!Boot Then Run Boot:Resources.!SharedLibs.!Boot\n'
  'If "<GTKLibs$Dir>" = "" Then IfThere Boot:Resources.!GTKLibs.!Boot Then Run Boot:Resources.!GTKLibs.!Boot\n'
  'If "<SharedLibs$Dir>" = "" Then Error %(name)s (mixed test) needs !SharedLibs with ld-riscos/so/2 in !Boot.Resources\n'
  "Unset %(name)s$SO2\n"
  "IfThere SharedLibs:lib.ld-riscos-eabihf/so Then Set %(name)s$SO2 1\n"
  'If "<%(name)s$SO2>" = "" Then Error The !SharedLibs booted has no ld-riscos/so/2: %(name)s (mixed test) needs a newer one (GCCSDK 8 or later, as Iris has) in !Boot.Resources.\n'
  'If "<GTKLibs$Dir>" = "" Then Error %(name)s (mixed test) needs !GTKLibs in !Boot.Resources (install it with PackMan)\n'
) % {"name": name, "prog": prog}
s = s.replace("| A UnixLib program converted to Absolute (AIF), statically linked with GTK\n"
  "| 3.24.52 and riscos-gtk's RISC OS GDK backend: it needs SharedUnixLibrary,\n"
  "| ARMEABISupport and Iconv (PackMan). It runs as a normal Wimp task.\n",
  "| It needs SharedUnixLibrary 1.16, ARMEABISupport and Iconv (PackMan).\n")
s, n = re.subn(r"(Set %s\$Dir <Obey\$Dir>\n)" % re.escape(name), lambda m: m.group(1) + checks, s, count=1)
assert n == 1, "Set NAME$Dir"
s, n = re.subn(r"(\|[^\n]*\n)*WimpSlot -min \d+K -max \d+K\n",
  "| The WimpSlot holds the program (the shared libraries are in SOManager's\n"
  "| dynamic area); the C heap grows it as needed\n"
  "WimpSlot -min %sK -max %sK\n"
  "| SOManager reads <program>$LD$Env: the libraries are in !GTKLibs\n"
  'Set %s$LD$Env "LD_LIBRARY_PATH=/GTKLibs:"\n' % (slot, slot, prog), s, count=1)
assert n == 1, "WimpSlot"
open(p, "w").write(s)
