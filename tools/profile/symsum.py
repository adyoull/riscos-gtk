#!/usr/bin/env python3
"""symsum.py [--base PROFILE0 --per K] [--objs LABEL=OBJ,OBJ...]... BINARY PROFILE [N] [MAPS]:
--base subtracts another run's profile (the same program doing the thing
0 times) and --per divides by how many times this run did it, giving the
cost of doing it once. --objs names the functions defined in object files
(e.g. riscos-gtk's backend, the fake RISC OS) by their file's label. tools/profile/insprof.c's per-block
instruction counts summed per function (from nm), the top N, and per
library (from the symbol's prefix: cairo, pixman, pango, harfbuzz,
FreeType, fontconfig, GLib, GTK, GDK, riscos-gtk's backend, the fake
RISC OS). Code in shared libraries (glibc: memcpy, malloc...; on RISC OS
these are UnixLib's) is named from MAPS, the program's /proc/self/maps
(tools/profile/mapsdump.c), else counted as one line.
Instruction counts, not times. MIT licence."""
import bisect, subprocess, sys, collections, re

def symbols(binary):
    out = subprocess.run(['nm', '-n', '--defined-only', binary], capture_output=True, text=True).stdout
    addrs, names = [], []
    for l in out.splitlines():
        p = l.split()
        # code symbols, without ARM's mapping symbols ($a, $t, $d)
        if len(p) == 3 and p[1] in 'tTwW' and not p[2].startswith('$'):
            addrs.append(int(p[0], 16)); names.append(p[2])
    # the program's code: .text (and .plt), so glibc and ld.so, mapped
    # elsewhere, aren't put on the last symbol
    lo, hi = None, None
    sec = subprocess.run(['readelf', '-SW', binary], capture_output=True, text=True).stdout
    for l in sec.splitlines():
        m = re.search(r'\]\s+(\.text|\.plt|\.init|\.fini)\s+\S+\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)', l)
        if m:
            a, sz = int(m.group(2), 16), int(m.group(3), 16)
            lo = a if lo is None else min(lo, a)
            hi = a + sz if hi is None else max(hi, a + sz)
    return addrs, names, lo, hi

LIBS = [('fake RISC OS (test harness)', r'^(fake_|_kernel_|_swix|os_|xos_|wimp_)'),
        ('riscos-gtk backend', r'^(_?gdk_riscos|_gdk_riscos|riscos_|gdk_riscos)'),
        ('pixman', r'^(pixman|_pixman|fast_|combine_|neon_)'),
        ('cairo', r'^(_?cairo)'),
        ('harfbuzz', r'^(hb_|_hb|OT::|AAT::|CFF::)'),
        ('pango', r'^(_?pango)'),
        ('FreeType', r'^(FT_|ft_|tt_|cff_|af_|ps_|t1_|sfnt_|gray_|TT_)'),
        ('fontconfig', r'^(Fc|_Fc)'),
        ('GTK', r'^(_?gtk)'),
        ('GDK', r'^(_?gdk)'),
        ('GLib/GObject/GIO', r'^(_?g_|g_|_g_|gst|gio)'),
        ('libpng/zlib', r'^(png_|inflate|deflate|adler|crc32|zcalloc)'),
        ('expat/libxml', r'^(XML_|xml)'),
       ]

def lib_symbols(maps):
    """(start, end, base, [(addr, name)]) for each shared library's code"""
    libs = []
    seen = {}
    for l in open(maps):
        p = l.split()
        if len(p) < 6 or not p[5].startswith('/') or '.so' not in p[5]:
            continue
        a, b = [int(x, 16) for x in p[0].split('-')]
        if p[5] not in seen and int(p[2], 16) == 0:
            seen[p[5]] = a                 # load base: the mapping at offset 0
        if 'x' not in p[1]:
            continue
        base = seen.get(p[5], a)
        out = subprocess.run(['nm', '-D', '-n', '--defined-only', p[5]], capture_output=True, text=True).stdout
        syms = []
        for s in out.splitlines():
            q = s.split()
            if len(q) == 3 and q[1] in 'tTwWiI':
                syms.append((base + int(q[0], 16), q[2].split('@')[0]))
        short = p[5].rsplit('/', 1)[-1].split('.so')[0]
        libs.append((a, b, short, sorted(syms)))
    return libs

def lib_of(name):
    if name in OBJ_LABELS:
        return OBJ_LABELS[name]
    for lib, rx in LIBS:
        if re.match(rx, name):
            return lib
    return 'other (program, libc helpers)'

def tally(prof, addrs, names, lo, hi, libs):
    per = collections.Counter()
    for l in open(prof):
        if l.startswith('caller'):
            continue
        pc, c = l.split()
        pc, c = int(pc, 16), int(c)
        i = bisect.bisect_right(addrs, pc) - 1
        if i < 0 or lo is None or not (lo <= pc < hi):
            name = '[outside the program: glibc, ld.so]'
            for a, b, short, syms in libs:
                if a <= pc < b:
                    j = bisect.bisect_right([x[0] for x in syms], pc) - 1
                    name = '[%s] %s' % (short, syms[j][1] if j >= 0 else '?')
                    break
            per[name] += c
        else:
            per[names[i]] += c
    return per

OBJ_LABELS = {}

def obj_names(spec):
    label, files = spec.split('=', 1)
    for f in files.split(','):
        out = subprocess.run(['nm', '--defined-only', f], capture_output=True, text=True).stdout
        for l in out.splitlines():
            p = l.split()
            if len(p) == 3 and p[1] in 'tT' and not p[2].startswith('$'):
                OBJ_LABELS.setdefault(p[2], label)

def main():
    args = sys.argv[1:]
    base, per_k = None, 1.0
    while args and args[0].startswith('--'):
        if args[0] == '--base':
            base = args[1]; args = args[2:]
        elif args[0] == '--per':
            per_k = float(args[1]); args = args[2:]
        elif args[0] == '--objs':
            obj_names(args[1]); args = args[2:]
        else:
            sys.exit('symsum.py: unknown option ' + args[0])
    binary, prof = args[0], args[1]
    n = int(args[2]) if len(args) > 2 else 40
    addrs, names, lo, hi = symbols(binary)
    libs = lib_symbols(args[3]) if len(args) > 3 else []
    per = tally(prof, addrs, names, lo, hi, libs)
    if base:
        per.subtract(tally(base, addrs, names, lo, hi, libs))
    per = collections.Counter({k: v / per_k for k, v in per.items() if v / per_k > 0})
    total = sum(per.values())
    print(f'total {total/1e6:.2f} M instructions' + (' each' if base else ''))
    bylib = collections.Counter()
    for k, v in per.items():
        bylib['shared libraries (glibc; UnixLib on RISC OS)' if k.startswith('[') else lib_of(k)] += v
    print('\nper library:')
    for k, v in bylib.most_common():
        print(f'  {v/1e6:10.1f} M {100*v/total:5.1f}%  {k}')
    print(f'\ntop {n} functions (not the fake RISC OS):')
    shown = 0
    for k, v in per.most_common():
        lab = 'shared' if k.startswith('[') else lib_of(k)
        if lab.startswith('fake'):
            continue
        print(f'  {v/1e6:10.2f} M {100*v/total:5.1f}%  {k}  ({lab})')
        shown += 1
        if shown >= n:
            break

main()
