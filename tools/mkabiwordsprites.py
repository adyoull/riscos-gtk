#!/usr/bin/env python3
"""mkabiwordsprites.py - riscos-gtk: !AbiWord's sprites.

    mkabiwordsprites.py FONTDIR APPDIR [SHEET.png]

Draws AbiWord's application icon in the RISC OS style (tools/
riscosicons_apps.py app_logo: a page of writing with a blue capital and a
gold pen nib; an original drawing, the same as the icon theme's
"abiword") at the RISC OS icon sizes (PRMs, Icon Sets): 34 x 34 and
17 x 17 at 90 dpi (APPDIR/!Sprites,ff9: !abiword, sm!abiword) and 68 x 68
and 34 x 34 at 180 dpi (!Sprites11,ff9). FONTDIR has DejaVu's TTFs.
Needs Pillow. MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import riscosicons_apps as apps  # noqa: E402


def main():
    fontdir, appdir = sys.argv[1], sys.argv[2]
    apps.setup(lambda n: os.path.join(fontdir, n), {})
    pics = {"!abiword": apps.app_icon(34), "sm!abiword": apps.app_icon(17),
            "!abiword11": apps.app_icon(34, 2), "sm!abiword11": apps.app_icon(34)}
    with tempfile.TemporaryDirectory() as t:
        for n, im in pics.items():
            im.save(os.path.join(t, n + ".png"))
        p = lambda n: os.path.join(t, n + ".png")
        subprocess.check_call([sys.executable, os.path.join(HERE, "png2sprite.py"),
                               os.path.join(appdir, "!Sprites,ff9"),
                               "!abiword=" + p("!abiword"), "sm!abiword=" + p("sm!abiword")])
        subprocess.check_call([sys.executable, os.path.join(HERE, "png2sprite.py"), "--dpi", "180",
                               os.path.join(appdir, "!Sprites11,ff9"),
                               "!abiword=" + p("!abiword11"), "sm!abiword=" + p("sm!abiword11")])
    if len(sys.argv) > 3:
        from PIL import Image
        z = 4
        sheet = Image.new("RGBA", ((34 + 17 + 68 + 34) * z + 40, 68 * z + 8), (221, 221, 221, 255))
        x = 4
        for n in ("!abiword", "sm!abiword", "!abiword11", "sm!abiword11"):
            im = pics[n]
            sheet.alpha_composite(im.resize((im.width * z, im.height * z), Image.NEAREST), (x, 4))
            x += im.width * z + 8
        sheet.save(sys.argv[3])
    print("mkabiwordsprites: !Sprites,ff9 and !Sprites11,ff9")


if __name__ == "__main__":
    main()
