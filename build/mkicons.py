#!/usr/bin/env python3
"""mkicons.py - riscos-gtk: the RISCOS fallback icon theme.

A small icon theme for GTK programs on RISC OS, taken from the Adwaita
icon theme 3.36 (GNOME Project; LGPL-3 or CC-BY-SA 3.0): the icons GTK
itself asks for and the usual freedesktop.org names programs use for
toolbars and menus (theme/icons-list.txt). Full-colour PNGs at 16, 22,
24, 32 and 48 pixels; symbolic icons (GTK's own: arrows, the entry clear
icon, ...) as GTK's .symbolic.png at 16 and 24 pixels (gdk-pixbuf on RISC
OS has no SVG loader): riscos-gtk draws them all; a name it doesn't draw
is made from Adwaita's SVG with gtk-encode-symbolic-svg if the build
machine has it (libgtk-3-bin), and left out otherwise.

riscos-gtk's own icons (tools/mkriscosicons.py: original, in the style of
the RISC OS desktop, as ClawsMail's) then replace Adwaita's for the names
they draw: toolbar, menu and dialogue box icons, at 16, 22, 24, 32 and 48
pixels. They need DejaVu's TTFs, from the DejaVu zip in $DL (or
RISCOS_ICON_FONTS, a folder of them); without, Adwaita's stay.

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


def own_names():
    """the names tools/mkriscosicons.py draws (empty without Pillow)"""
    here = os.path.dirname(os.path.abspath(__file__))
    sys.dont_write_bytecode = True
    sys.path.insert(0, os.path.join(here, "..", "tools"))
    try:
        import mkriscosicons
    except ImportError:
        return set()
    return {n for n, _c, _f in mkriscosicons.ICONS + mkriscosicons.SYMBOLIC}


def draw_own(theme):
    """riscos-gtk's own icons into theme: (dirs, {name: context}), or None"""
    import zipfile
    here = os.path.dirname(os.path.abspath(__file__))
    sys.dont_write_bytecode = True
    sys.path.insert(0, os.path.join(here, "..", "tools"))
    try:
        import mkriscosicons
    except ImportError as e:            # no Pillow
        print("mkicons: riscos-gtk's own icons not drawn (%s)" % e)
        return None
    fonts = os.environ.get("RISCOS_ICON_FONTS")
    tmp = None
    if not fonts:
        dl = os.environ.get("DL") or os.path.join(here, "..", "dl")
        z = os.path.join(dl, "dejavu-fonts-ttf-2.37.zip")
        if not os.path.exists(z):
            print("mkicons: riscos-gtk's own icons not drawn (no %s)" % z)
            return None
        tmp = fonts = tempfile.mkdtemp()
        with zipfile.ZipFile(z) as zf:
            for n in zf.namelist():
                if n.endswith(".ttf"):
                    with open(os.path.join(fonts, os.path.basename(n)), "wb") as f:
                        f.write(zf.read(n))
    dirs = mkriscosicons.draw_theme(fonts, theme)
    if tmp:
        shutil.rmtree(tmp)
    names = {n: ctx for n, ctx, _f in mkriscosicons.ICONS + mkriscosicons.SYMBOLIC}
    with open(os.path.join(theme, "README"), "w") as f:
        f.write("RISCOS icon theme (riscos-gtk). These icons are riscos-gtk's own, drawn by\n"
                "tools/mkriscosicons.py (MIT licence):\n  %s\n"
                "The others are Adwaita 3.36's (GNOME Project; LGPL-3 or CC-BY-SA 3.0: COPYING*).\n"
                % " ".join(sorted(names)))
    print("mkicons: %d of riscos-gtk's own icons" % len(names))
    return dirs, names


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
    own = own_names()
    have_encoder = shutil.which("gtk-encode-symbolic-svg") is not None
    for name in names:
        symbolic = name.endswith("-symbolic")
        hit = False
        if symbolic and name in own:
            found += 1          # drawn by tools/mkriscosicons.py below
            continue
        if symbolic and not have_encoder:
            missing.append(name)
            continue
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
    drawn = draw_own(theme)
    if drawn:
        drawn_dirs, drawn_names = drawn
        # Adwaita's copy of a drawn name in another context goes, so each
        # name is in one folder per size
        for rel, size, ctx in list(dirs):
            for n in drawn_names:
                for suffix in (".png", ".symbolic.png"):
                    f = os.path.join(theme, rel, n + suffix)
                    mine = os.path.join(theme, "%dx%d" % (size, size), drawn_names[n], n + suffix)
                    if os.path.exists(f) and ctx != drawn_names[n] and os.path.exists(mine):
                        os.remove(f)
        dirs |= drawn_dirs
        missing = [m for m in missing if m not in drawn_names]
    dirs = sorted(dirs, key=lambda d: (d[1], d[2]))
    with open(os.path.join(theme, "index.theme"), "w") as f:
        f.write("[Icon Theme]\n")
        f.write("Name=RISCOS\n")
        f.write("Comment=riscos-gtk: icons for GTK programs on RISC OS: riscos-gtk's own "
                "(MIT) and Adwaita 3.36's (GNOME Project, LGPL-3 or CC-BY-SA 3.0)\n")
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
    # and hicolor's standard folders (freedesktop's hicolor-icon-theme):
    # GTK looks for icons a program adds from its resources
    # (gtk_icon_theme_add_resource_path, e.g. Gnumeric's toolbar icons) as
    # part of hicolor, only in folders hicolor's index.theme lists
    contexts = {"actions": "Actions", "apps": "Applications", "categories": "Categories",
                "devices": "Devices", "emblems": "Emblems", "emotes": "Emotes",
                "mimetypes": "MimeTypes", "places": "Places", "status": "Status"}
    std = [("%dx%d/%s" % (n, n, c), n) for n in (16, 22, 24, 32, 48, 64, 96, 128, 256, 512)
           for c in sorted(contexts)]
    dirs_all = sorted(set(own) | set(std), key=lambda d: (d[1], d[0]))
    with open(os.path.join(hic, "index.theme"), "w") as f:
        f.write("[Icon Theme]\nName=Hicolor\nComment=Fallback icon theme\nHidden=true\nDirectories=%s\n\n"
                % ",".join(r for r, _ in dirs_all))
        for rel, size in dirs_all:
            ctx = contexts.get(rel.split("/")[-1], "Applications")
            f.write("[%s]\nSize=%d\nContext=%s\nType=Fixed\n\n" % (rel, size, ctx))
    print("mkicons: %d of %d names, %d directories%s" % (
        found, len(names), len(dirs), ("; not in Adwaita: " + " ".join(missing)) if missing else ""))
    if "image-missing" in missing:
        sys.exit("mkicons: image-missing is required (GTK aborts without it)")


if __name__ == "__main__":
    main()
