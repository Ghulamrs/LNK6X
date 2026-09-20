#!/usr/bin/env python3
"""elfdiff.py - where two TI ELF32 executables differ, section by section.

    python3 tests/elfdiff.py a.out b.out          per section: same, differs (n bytes), only in one
    python3 tests/elfdiff.py -q a.out b.out       one line

The headers are compared field by field (entry, segment and section counts), then every section
by name: its address, size, flags and bytes. The symbol table is compared as a set of
(name, value, shndx-name) so that a reordering and a real difference are told apart."""
import struct, sys

def cstr(b):
    i = b.find(b'\0')
    return (b if i < 0 else b[:i]).decode('latin1')

def load(path):
    d = open(path, 'rb').read()
    if d[:4] != b'\x7fELF':
        raise ValueError(path + ': not ELF')
    entry, phoff, shoff = struct.unpack('<III', d[24:36])
    phes, phnum, shes, shnum, shstr = struct.unpack('<HHHHH', d[42:52])
    secs = [struct.unpack('<IIIIIIIIII', d[shoff + i * shes:shoff + (i + 1) * shes]) for i in range(shnum)]
    names = d[secs[shstr][4]:secs[shstr][4] + secs[shstr][5]]
    out = {}
    order = []
    for s in secs:
        nm = cstr(names[s[0]:])
        body = d[s[4]:s[4] + s[5]] if s[1] not in (0, 8) else b''
        out[nm] = dict(type=s[1], flags=s[2], addr=s[3], size=s[5], align=s[8], body=body)
        order.append(nm)
    segs = [struct.unpack('<IIIIIIII', d[phoff + i * phes:phoff + (i + 1) * phes]) for i in range(phnum)]
    syms = set()
    for s in secs:
        if s[1] == 2:
            strs = d[secs[s[6]][4]:secs[s[6]][4] + secs[s[6]][5]]
            es = s[9] or 16
            for k in range(s[5] // es):
                e = d[s[4] + k * es:s[4] + (k + 1) * es]
                nm, val, sz, inf, oth, shn = struct.unpack('<IIIBBH', e)
                shname = {0: 'UNDEF', 0xFFF1: 'ABS', 0xFFF2: 'COMMON'}.get(shn, order[shn] if shn < len(order) else str(shn))
                syms.add((cstr(strs[nm:]), val, shname, inf))
    return dict(size=len(d), entry=entry, segs=segs, secs=out, order=order, syms=syms)

def report(pa, pb, quiet=False):
    a = load(pa); b = load(pb)
    ra = open(pa, 'rb').read(); rb = open(pb, 'rb').read()
    if ra == rb:
        print('%s == %s (%d bytes)' % (pa, pb, len(ra)))
        return 0
    lines = []
    short = []
    if a['entry'] != b['entry']:
        lines.append('entry %08X against %08X' % (a['entry'], b['entry'])); short.append('entry')
    if len(a['segs']) != len(b['segs']):
        lines.append('%d segments against %d' % (len(a['segs']), len(b['segs']))); short.append('segments %d/%d' % (len(a['segs']), len(b['segs'])))
    elif a['segs'] != b['segs']:
        lines.append('the %d segments differ in their fields' % len(a['segs'])); short.append('segment fields')
    onlya = [n for n in a['order'] if n not in b['secs']]
    onlyb = [n for n in b['order'] if n not in a['secs']]
    if onlya: lines.append('only in %s: %s' % (pa, ' '.join(onlya))); short.append('sections only in a: %d' % len(onlya))
    if onlyb: lines.append('only in %s: %s' % (pb, ' '.join(onlyb))); short.append('sections only in b: %d' % len(onlyb))
    if [n for n in a['order'] if n in b['secs']] != [n for n in b['order'] if n in a['secs']]:
        lines.append('section order differs'); short.append('section order')
    nbytes = 0
    for n in a['order']:
        if n not in b['secs']:
            continue
        x, y = a['secs'][n], b['secs'][n]
        what = []
        if x['addr'] != y['addr']: what.append('addr %X/%X' % (x['addr'], y['addr']))
        if x['size'] != y['size']: what.append('size %X/%X' % (x['size'], y['size']))
        if x['flags'] != y['flags']: what.append('flags %X/%X' % (x['flags'], y['flags']))
        if x['align'] != y['align']: what.append('align %d/%d' % (x['align'], y['align']))
        if x['body'] != y['body']:
            m = min(len(x['body']), len(y['body']))
            c = sum(1 for i in range(m) if x['body'][i] != y['body'][i]) + abs(len(x['body']) - len(y['body']))
            nbytes += c
            what.append('%d bytes' % c)
        if what:
            lines.append('  %-22s %s' % (n, ', '.join(what)))
            short.append('%s(%s)' % (n, ','.join(what)))
    sa, sb = a['syms'], b['syms']
    if sa == sb and (a['secs'].get('.symtab', {}).get('body') != b['secs'].get('.symtab', {}).get('body')):
        lines.append('symbols: the same %d, in another order or string packing' % len(sa)); short.append('symbols reordered')
    if sa != sb:
        lines.append('symbols: %d only in %s, %d only in %s (of %d / %d)' % (len(sa - sb), pa, len(sb - sa), pb, len(sa), len(sb)))
        for s in sorted(sa - sb)[:8]: lines.append('    a: %s = %08X in %s' % (s[0], s[1], s[2]))
        for s in sorted(sb - sa)[:8]: lines.append('    b: %s = %08X in %s' % (s[0], s[1], s[2]))
        short.append('symbols %d/%d' % (len(sa - sb), len(sb - sa)))
    total = sum(1 for i in range(min(len(ra), len(rb))) if ra[i] != rb[i]) + abs(len(ra) - len(rb))
    if quiet:
        print('%d bytes differ: %s' % (total, '; '.join(short) if short else 'headers only'))
        return total
    print('%s (%d bytes) against %s (%d bytes): %d bytes differ' % (pa, len(ra), pb, len(rb), total))
    for l in lines:
        print(l)
    return total

if __name__ == '__main__':
    args = [x for x in sys.argv[1:] if x != '-q']
    if len(args) != 2:
        print(__doc__); sys.exit(2)
    sys.exit(1 if report(args[0], args[1], '-q' in sys.argv) else 0)
