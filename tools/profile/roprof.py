#!/usr/bin/env python3
"""roprof.py SYMS LIBMAP PROFILE [BASE_PROFILE] [N]: a profile made on
RISC OS by riscos-unixlib-web's sampler (UnixLib$Profile; lines
"pc lr cs samples up1 up2 up3"), named from SYMS (nm -n of the program's
ELF: static programs only, as the samples are absolute addresses) and
grouped by library with LIBMAP ("symbol library" lines, from nm of the
static archives). Each sample's weight is capped at 3 cs (a long SWI or
another task's turn lands on the one sample after it); samples waiting
in riscos-gtk's Wimp_PollIdle (or the main loop's poll) are left out.
With BASE_PROFILE, its totals are subtracted (the same program doing
less), giving what the extra work cost. Prints centiseconds. MIT licence."""
import bisect, sys, collections

def load_syms(p):
    a, n = [], []
    for l in open(p):
        f = l.split()
        if len(f) == 3:
            a.append(int(f[0], 16)); n.append(f[2])
    return a, n

def name_of(syms, x):
    """The function holding x, without GCC's .constprop/.isra/.part
    suffixes; '-' past the code (data, the stack: not a return address)"""
    a, n = syms
    i = bisect.bisect_right(a, x) - 1
    if i < 0 or not x or n[i] == '_fini':
        return '-'
    return n[i].split('.')[0]

WAIT = {'poll_once', 'early_poll', 'riscos_poll'}
# the program's own idle thread (it lets the ticker run, so samples are
# taken): its turns aren't the program's work
IDLE = {'__pthread_yield_return'}
SWIS = {'_kernel_swi', 'swi', '_swix', '_swi', 'swi_call'}

def read(p, syms):
    self_, incl, waits, total = collections.Counter(), collections.Counter(), 0.0, 0.0
    for l in open(p):
        f = l.split()
        if len(f) != 7 or f[0] in ('ref', 'total'):
            continue
        pc, lr, cs, cnt = int(f[0], 16), int(f[1], 16), int(f[2]), int(f[3])
        ups = [int(x, 16) for x in f[4:7]]
        w = min(cs, 3 * cnt)
        names = [name_of(syms, x) for x in [pc, lr] + ups if x]
        names = [x for x in names if x != '-'] or ['-']
        fn = names[0]
        if fn in IDLE:
            continue
        # waiting in the Wimp: the SWI's return, called from the poll
        callers = [x for x in names[1:] if x not in SWIS and x != fn]
        if fn in SWIS and callers and callers[0] in WAIT:
            waits += w
            continue
        total += w
        self_[fn] += w
        for x in set(names):
            incl[x] += w
    return self_, incl, total, waits

def main():
    syms = load_syms(sys.argv[1])
    lib = {}
    for l in open(sys.argv[2]):
        f = l.split()
        if len(f) == 2:
            lib.setdefault(f[0], f[1])
    args = sys.argv[3:]
    n = int(args.pop()) if args and args[-1].isdigit() else 30
    s, inc, tot, wt = read(args[0], syms)
    if len(args) > 1:
        bs, binc, btot, bwt = read(args[1], syms)
        s.subtract(bs); inc.subtract(binc); tot -= btot; wt -= bwt
    print('busy %.0f cs (each sample capped at 3 cs), waiting in the Wimp %.0f cs' % (tot, wt))
    per = collections.Counter()
    for k, v in s.items():
        per[lib.get(k, 'program/other')] += v
    print('\nby library (own time):')
    for k, v in per.most_common(16):
        print('  %7.0f cs %5.1f%%  %s' % (v, 100 * v / tot if tot else 0, k))
    print('\nfunctions, own time:')
    for k, v in s.most_common(n):
        print('  %7.0f cs %5.1f%%  %s  (%s)' % (v, 100 * v / tot if tot else 0, k, lib.get(k, '')))
    print('\nfunctions, with what they called (stack scan; rough):')
    for k, v in inc.most_common(n):
        print('  %7.0f cs %5.1f%%  %s  (%s)' % (v, 100 * v / tot if tot else 0, k, lib.get(k, '')))

try:
    main()
except BrokenPipeError:
    pass
