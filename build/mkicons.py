#!/usr/bin/env python3
"""mkicons.py - riscos-gtk: the RISCOS fallback icon theme.

A small icon theme for GTK programs on RISC OS, taken from the Adwaita
icon theme 3.36 (GNOME Project; LGPL-3 or CC-BY-SA 3.0): the icons GTK
itself asks for and the usual freedesktop.org names programs use for
toolbars and menus (theme/icons-list.txt). Full-colour PNGs at 16, 22,
24, 32 and 48 pixels; symbolic icons (GTK's own: arrows, the entry clear
icon, ...) made into GTK's .symbolic.png at 16 and 24 pixels (gdk-pixbuf
on RISC OS has no SVG loader), with gtk-encode-symbolic-svg.

The desktop's own Wimp sprites come first at run time (gtk/riscos/
gtkriscostheme.c makes a "RISCOS-Wimp" theme inheriting this one).

    mkicons.py ADWAITA_DIR LIST OUT_DIR

OUT_DIR gets RISCOS/ (index.theme, the icons, the licence) and hicolor/
(an empty index.theme: GTK looks for it). MIT licence (the script).
"""
import os
import shutil
import subprocess
import sys
import tempfile

SIZES = [16, 22, 24, 32, 48]
SYMBOLIC_SIZES = [16, 24]


def main():
    adw, listfile, out = sys.argv[1:4]
    names = []
    for line in open(listfile):
        line = line.split("#")[0].strip()
        if line:
            names.append(line)
    theme = os.path.join(out, "RISCOS")
    if os.path.exists(theme):
        shutil.rmtree(theme)
    dirs = set()
    found = 0
    missing = []
    for name in names:
        symbolic = name.endswith("-symbolic")
        hit = False
        if not symbolic:
            for size in SIZES:
                sd = os.path.join(adw, "%dx%d" % (size, size))
                if not os.path.isdir(sd):
                    continue
                for ctx in sorted(os.listdir(sd)):
                    src = os.path.join(sd, ctx, name + ".png")
                    if os.path.exists(src):
                        rel = "%dx%d/%s" % (size, size, ctx)
                        os.makedirs(os.path.join(theme, rel), exist_ok=True)
                        shutil.copy(src, os.path.join(theme, rel, name + ".png"))
                        dirs.add((rel, size, ctx))
                        hit = True
                        break
        # the symbolic form, for names given as -symbolic
        base = name if symbolic else None
        if base:
            sc = os.path.join(adw, "scalable")
            for ctx in sorted(os.listdir(sc)):
                svg = os.path.join(sc, ctx, base + ".svg")
                if not os.path.exists(svg):
                    continue
                for size in SYMBOLIC_SIZES:
                    rel = "%dx%d/%s" % (size, size, ctx)
                    d = os.path.join(theme, rel)
                    os.makedirs(d, exist_ok=True)
                    tmp = tempfile.mkdtemp()
                    subprocess.run(["gtk-encode-symbolic-svg", "-o", tmp, svg,
                                    "%dx%d" % (size, size)], check=True,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                    for f in os.listdir(tmp):
                        shutil.move(os.path.join(tmp, f), os.path.join(d, f))
                    shutil.rmtree(tmp)
                    dirs.add((rel, size, ctx))
                hit = True
                break
        if hit:
            found += 1
        else:
            missing.append(name)
    dirs = sorted(dirs, key=lambda d: (d[1], d[2]))
    with open(os.path.join(theme, "index.theme"), "w") as f:
        f.write("[Icon Theme]\n")
        f.write("Name=RISCOS\n")
        f.write("Comment=riscos-gtk: icons for GTK programs on RISC OS, from Adwaita 3.36 "
                "(GNOME Project, LGPL-3 or CC-BY-SA 3.0)\n")
        f.write("Inherits=hicolor\n")
        f.write("Directories=%s\n\n" % ",".join(d[0] for d in dirs))
        for rel, size, ctx in dirs:
            f.write("[%s]\nSize=%d\nContext=%s\nType=Fixed\n\n" % (rel, size, ctx.capitalize()))
    for lic in ("COPYING", "COPYING_CCBYSA3", "COPYING_LGPL"):
        src = os.path.join(os.path.dirname(adw.rstrip("/")), lic)
        if os.path.exists(src):
            shutil.copy(src, os.path.join(theme, lic))
    hic = os.path.join(out, "hicolor")
    os.makedirs(hic, exist_ok=True)
    # the program's own icons already there (e.g. Geany's,
    # hicolor/16x16/apps/*.png): their folders listed, so GTK finds them
    own = []
    for root, _dirs, files in os.walk(hic):
        if any(n.endswith(".png") for n in files):
            rel = os.path.relpath(root, hic)
            size = rel.split("/")[0].split("x")[0]
            if size.isdigit():
                own.append((rel, int(size)))
    own.sort()
    with open(os.path.join(hic, "index.theme"), "w") as f:
        f.write("[Icon Theme]\nName=Hicolor\nComment=Fallback icon theme\nHidden=true\nDirectories=%s\n\n"
                % ",".join(r for r, _ in own))
        for rel, size in own:
            f.write("[%s]\nSize=%d\nContext=Applications\nType=Fixed\n\n" % (rel, size))
    print("mkicons: %d of %d names, %d directories%s" % (
        found, len(names), len(dirs), ("; not in Adwaita: " + " ".join(missing)) if missing else ""))
    if "image-missing" in missing:
        sys.exit("mkicons: image-missing is required (GTK aborts without it)")


if __name__ == "__main__":
    main()
