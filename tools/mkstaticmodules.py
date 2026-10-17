#!/usr/bin/env python3
"""mkstaticmodules.py - link a program's GModule plugins into it.

RISC OS has no dlopen, so plugins that a program would load with
g_module_open() (Gnumeric's functions and file formats, GOffice's chart
types...) are linked into the program instead, and registered with GLib's
static module registry (g_module_register_static, riscos-gtk's GLib patch).

  mkstaticmodules.py --nm NM --objcopy OBJCOPY --ar AR --out DIR \\
      [--register FUNC] NAME=OBJ[,OBJ...] ...

For each module NAME (as g_module_open sees its file name: "fn-math/plugin"
for .../plugins/fn-math/plugin.so) and its object files:
  - every global symbol the objects define is renamed with a prefix made
    from NAME (objcopy --redefine-syms), so the same names in many plugins
    (go_plugin_init, go_plugin_header...) don't clash; references between
    the module's own objects are renamed with them;
  - the renamed objects go into DIR/<prefix>.a;
  - DIR/staticmodules.c gets a table for the module (original name ->
    renamed symbol) and FUNC (default riscos_register_static_modules)
    registers every module: call it before the program's plugins load.
Prints the archives to link, one per line. MIT licence.
"""
import argparse
import os
import re
import subprocess
import sys

DATA_TYPES = set("DBRGSCV")   # data symbols (C: common, V: weak object)
CODE_TYPES = set("TW")        # code (W: weak function)


def defined_globals(nm, objs):
    """{name: type letter} of the global symbols the objects define"""
    out = subprocess.run([nm, "-g", "--defined-only"] + objs, check=True,
                         capture_output=True, text=True).stdout
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and len(parts[1]) == 1:
            syms[parts[2]] = parts[1].upper()
    return syms


def prefix_for(name):
    return "smod_" + re.sub(r"[^A-Za-z0-9]", "_", name) + "__"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nm", default="nm")
    ap.add_argument("--objcopy", default="objcopy")
    ap.add_argument("--ar", default="ar")
    ap.add_argument("--out", required=True)
    ap.add_argument("--register", default="riscos_register_static_modules")
    ap.add_argument("modules", nargs="+", metavar="NAME=OBJ[,OBJ...]")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    decls, tables, regs, archives = [], [], [], []
    for spec in a.modules:
        name, _, objlist = spec.partition("=")
        objs = [o for o in objlist.split(",") if o]
        if not name or not objs:
            sys.exit("mkstaticmodules.py: bad module %r" % spec)
        pre = prefix_for(name)
        syms = defined_globals(a.nm, objs)
        if not syms:
            sys.exit("mkstaticmodules.py: %s defines no symbols" % name)
        work = os.path.join(a.out, pre.rstrip("_"))
        os.makedirs(work, exist_ok=True)
        mapfile = os.path.join(work, "redefine.txt")
        with open(mapfile, "w") as f:
            for s in sorted(syms):
                f.write("%s %s%s\n" % (s, pre, s))
        renamed = []
        for i, o in enumerate(objs):
            dst = os.path.join(work, "%03d-%s" % (i, os.path.basename(o)))
            subprocess.run([a.objcopy, "--redefine-syms=" + mapfile, o, dst], check=True)
            renamed.append(dst)
        lib = os.path.join(a.out, pre.rstrip("_") + ".a")
        if os.path.exists(lib):
            os.remove(lib)
        subprocess.run([a.ar, "rcs", lib] + renamed, check=True)
        archives.append(lib)

        entries = []
        for s in sorted(syms):
            t = syms[s]
            if t in CODE_TYPES:
                decls.append("extern void %s%s (void);" % (pre, s))
            else:
                decls.append("extern char %s%s[];" % (pre, s))
            entries.append('  { "%s", (gpointer) %s%s },' % (s, pre, s))
        tables.append("static const GModuleStaticSymbol %stable[] = {\n%s\n  { NULL, NULL }\n};"
                      % (pre, "\n".join(entries)))
        regs.append('  g_module_register_static ("%s", %stable);' % (name, pre))

    src = os.path.join(a.out, "staticmodules.c")
    with open(src, "w") as f:
        f.write("/* Made by riscos-gtk's tools/mkstaticmodules.py: the program's\n"
                "   built-in GModule plugins. */\n#include <gmodule.h>\n\n")
        f.write("\n".join(decls) + "\n\n")
        f.write("\n\n".join(tables) + "\n\n")
        f.write("void %s (void);\n\nvoid\n%s (void)\n{\n%s\n}\n" % (a.register, a.register, "\n".join(regs)))
    for lib in archives:
        print(lib)


if __name__ == "__main__":
    main()
