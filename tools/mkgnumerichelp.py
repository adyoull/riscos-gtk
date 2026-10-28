#!/usr/bin/env python3
"""Gnumeric's manual (DocBook, doc/C in its source) as HTML for !Gnumeric:
one file per section, named by its id, its pictures reduced to 128
colours, and a small page for each id the program's Help buttons ask for
that has no file of its own (it goes on to the section's file and place).

  mkgnumerichelp.py GNUMERIC_SRC OUTDIR

Needs xsltproc and the DocBook XSL stylesheets (docbook-xsl), and Pillow.
The program opens OUTDIR/<id>.html (patches/goffice-0.10.57-riscos.diff,
go_help_display) and OUTDIR/index.html (Help > Contents).
MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import os
import re
import shutil
import subprocess
import sys

from PIL import Image

XSL = "/usr/share/xml/docbook/stylesheet/docbook-xsl/html/chunk.xsl"


def main():
    src, out = sys.argv[1], os.path.abspath(sys.argv[2])
    doc = os.path.join(src, "doc", "C")
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    subprocess.run(["xsltproc", "--nonet", "--xinclude",
                    "--stringparam", "base.dir", out + "/",
                    "--stringparam", "use.id.as.filename", "1",
                    "--stringparam", "chunk.section.depth", "1",
                    "--stringparam", "html.ext", ".html",
                    "--stringparam", "chunker.output.encoding", "UTF-8",
                    XSL, "gnumeric.xml"], cwd=doc, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # the pictures, smaller
    figs = os.path.join(out, "figures")
    os.makedirs(figs)
    for f in sorted(os.listdir(os.path.join(doc, "figures"))):
        p = os.path.join(doc, "figures", f)
        if f.lower().endswith(".png"):
            im = Image.open(p).convert("RGBA")
            im.quantize(colors=128, method=Image.Quantize.FASTOCTREE).save(
                os.path.join(figs, f), optimize=True)
        else:
            shutil.copy(p, figs)
    # where each id is
    where = {}
    for f in os.listdir(out):
        if f.endswith(".html"):
            with open(os.path.join(out, f), encoding="utf-8", errors="replace") as h:
                for m in re.finditer(r'\b(?:id|name)="([^"]+)"', h.read()):
                    where.setdefault(m.group(1), f)
    # the ids the program asks for (src/help.h and the dialogues)
    asked = set()
    for root, _, files in os.walk(os.path.join(src, "src")):
        for f in files:
            if f.endswith((".h", ".c")):
                with open(os.path.join(root, f), encoding="utf-8", errors="replace") as h:
                    for m in re.finditer(r'GNUMERIC_HELP_LINK_\w+\s+"([^"]+)"', h.read()):
                        asked.add(m.group(1))
    made = missing = 0
    for i in sorted(asked):
        if os.path.exists(os.path.join(out, i + ".html")):
            continue
        target = where.get(i)
        if target is None:
            missing += 1
            target, frag = "index.html", ""
        else:
            frag = "#" + i
        with open(os.path.join(out, i + ".html"), "w", encoding="utf-8") as h:
            h.write('<!DOCTYPE html><html><head><meta charset="utf-8">'
                    '<meta http-equiv="refresh" content="0; url=%s%s">'
                    '<title>Gnumeric</title></head><body><p><a href="%s%s">'
                    'Gnumeric Manual</a></p></body></html>\n' % (target, frag, target, frag))
        made += 1
    n = len([f for f in os.listdir(out) if f.endswith(".html")])
    print("mkgnumerichelp: %d pages (%d to go on to a section; %d ids not in the manual: its contents)"
          % (n, made, missing))


if __name__ == "__main__":
    main()
