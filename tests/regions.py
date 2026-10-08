#!/usr/bin/env python3
"""regions.py - how many bytes two TI ELF32 images differ by, region by region.

    python3 tests/regions.py ours.out ref.out             every region that differs, and its count
    python3 tests/regions.py ours.out ref.out PINS...     held against pins: exit 0 within them,
                                                          1 over one or unpinned, 3 within but lower

A region is a section, by name, or one of elf-header, phdrs and shdrs. A section's count is its
differing bytes plus the difference in length; a section in one image only counts its size. A pin
is region=count, the region a glob (.debug_*=250000); a region no pin names must not differ at all,
so a change to the unwind index or to .text can never hide inside the symbol table's count."""
import fnmatch, struct, sys


def load(path):
    d = open(path, 'rb').read()
    if d[:4] != b'\x7fELF':
        raise SystemExit(path + ': not ELF')
    phoff, shoff = struct.unpack('<II', d[28:36])
    phes, phnum, shes, shnum, shstr = struct.unpack('<HHHHH', d[42:52])
    secs = [struct.unpack('<IIIIIIIIII', d[shoff + i * shes:shoff + (i + 1) * shes]) for i in range(shnum)]
    names = d[secs[shstr][4]:secs[shstr][4] + secs[shstr][5]] if shnum else b''
    out = {}
    for s in secs:
        n = names[s[0]:].split(b'\0', 1)[0].decode('latin1') or '(null)'
        out[n] = d[s[4]:s[4] + s[5]] if s[1] not in (0, 8) else b'\0' * 0
    return dict(hdr=d[:52], ph=d[phoff:phoff + phes * phnum], sh=d[shoff:shoff + shes * shnum], secs=out)


def count(x, y):
    m = min(len(x), len(y))
    return sum(1 for i in range(m) if x[i] != y[i]) + abs(len(x) - len(y))


def regions(pa, pb):
    a, b = load(pa), load(pb)
    r = {'elf-header': count(a['hdr'], b['hdr']), 'phdrs': count(a['ph'], b['ph']),
         'shdrs': count(a['sh'], b['sh'])}
    for n in list(a['secs']) + [n for n in b['secs'] if n not in a['secs']]:
        r[n] = count(a['secs'].get(n, b''), b['secs'].get(n, b''))
    return {k: v for k, v in r.items() if v}


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    r = regions(sys.argv[1], sys.argv[2])
    pins = [p.split('=', 1) for p in sys.argv[3:]]
    if not pins:
        print(' '.join('%s=%d' % kv for kv in r.items()))
        return 0
    got = {}
    over, lower = [], []
    for k, v in r.items():
        key = next((g for g, _ in pins if fnmatch.fnmatchcase(k, g)), None)
        if key is None:
            over.append('%s=%d unpinned' % (k, v))
        else:
            got[key] = got.get(key, 0) + v
    for g, c in pins:
        n = got.get(g, 0)
        if n > int(c):
            over.append('%s=%d over %s' % (g, n, c))
        elif n < int(c):
            lower.append('%s=%d under %s' % (g, n, c))
    print('; '.join(over or lower) or 'every region at its pin')
    return 1 if over else 3 if lower else 0


sys.exit(main())
