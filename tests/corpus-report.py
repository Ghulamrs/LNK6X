#!/usr/bin/env python3
"""corpus-report.py - the ratification table, from what tests/corpus.sh brought back.

    python3 tests/corpus-report.py build/corpus [-v]

For every program: how each leg fared - the assembler (cl6x / asm6x), TI's lnk6x on the two
sets of objects, this linker on the two sets - and whether the image equals the oracle's byte
for byte (or how it differs, via elfdiff.py). The oracle is cl6x + lnk6x. Nothing is run:
vm6747 reads assembly, not an ELF executable. Refusals are grouped by their first line at the
end, each with the count of programs it stops."""
import os, re, sys, collections, io, contextlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import elfdiff

LEGS = ['ti-lnk', 'my-lnk', 'ti-ours', 'my-ours']

def read(p):
    try:
        return open(p, 'rb').read().decode('latin1').replace('\r', '')
    except IOError:
        return None

def first_error(text):
    if not text:
        return ''
    lines = [l.strip() for l in text.splitlines() if l.strip()]
    for i, l in enumerate(lines):
        if 'error' in l.lower() or l.startswith('lnk6x:'):
            # TI's messages put the symbol on the next line
            if l.startswith('error #') or l.startswith('error:'):
                return l
            m = re.search(r'(error #\d+.*|lnk6x: .*|.*error.*)', l)
            return m.group(1) if m else l
    return lines[0] if lines else ''

def ti_error(text):
    """TI's first error, with the line under it when it names the symbol"""
    if not text:
        return ''
    lines = text.splitlines()
    for i, l in enumerate(lines):
        if 'error' in l.lower():
            l = l.strip()
            nxt = lines[i + 1].strip() if i + 1 < len(lines) else ''
            m = re.search(r'error #\d+: (.*)', l)
            return (m.group(1) if m else l) + ((' ' + nxt) if nxt and not nxt.startswith('>>') else '')
    return first_error(text)

def main():
    root = sys.argv[1] if len(sys.argv) > 1 else 'build/corpus'
    verbose = '-v' in sys.argv
    progs = sorted(d for d in os.listdir(root) if re.match(r'\d\d-', d) and os.path.isfile(os.path.join(root, d, 'link.txt')))
    rows = []
    refusals = collections.defaultdict(list)
    asmref = collections.defaultdict(list)
    totals = collections.defaultdict(collections.Counter)
    for p in progs:
        d = os.path.join(root, p)
        name, objs = open(os.path.join(d, 'link.txt')).read().strip().split('|')
        objs = objs.split()
        ti_ok = all(os.path.exists(os.path.join(d, 'ti', o + '.obj')) for o in objs)
        my_ok = all(os.path.exists(os.path.join(d, 'my', o + '.obj')) for o in objs)
        for o in objs:
            if not os.path.exists(os.path.join(d, 'ti', o + '.obj')):
                asmref['cl6x: ' + first_error(read(os.path.join(d, 'ti', o + '.log')))].append(p)
            if not os.path.exists(os.path.join(d, 'my', o + '.obj')):
                asmref['asm6x: ' + first_error(read(os.path.join(d, 'my', o + '.log')))].append(p)
        oracle = 'ti-lnk'
        oracle_out = os.path.join(d, oracle + '.out')
        cells = {}
        for leg in LEGS:
            img = os.path.join(d, leg + '.out')
            asm_ok = ti_ok if leg.startswith('ti') else my_ok
            if not asm_ok:
                cells[leg] = 'n/a (assembler)'; totals[leg]['n/a'] += 1
                continue
            if not os.path.exists(img):
                log = read(os.path.join(d, leg + '.log'))
                msg = ti_error(log) if leg.endswith('lnk') else first_error(log)
                cells[leg] = 'refused'; totals[leg]['refused'] += 1
                refusals[(leg.split('-')[1], msg)].append(p)
                continue
            if leg == oracle:
                cells[leg] = 'oracle, %d bytes' % os.path.getsize(img); totals[leg]['oracle'] += 1
                continue
            if not os.path.exists(oracle_out):
                cells[leg] = 'linked, no oracle'; totals[leg]['linked'] += 1
                continue
            if open(img, 'rb').read() == open(oracle_out, 'rb').read():
                cells[leg] = 'identical'; totals[leg]['identical'] += 1
            else:
                buf = io.StringIO()
                with contextlib.redirect_stdout(buf):
                    elfdiff.report(img, oracle_out, quiet=True)
                cells[leg] = buf.getvalue().strip(); totals[leg]['differ'] += 1
        rows.append((p, len(objs), cells))

    print('| # | program | modules | cl6x+lnk6x | asm6x+lnk6x | cl6x+LNK6x | asm6x+LNK6x |')
    print('|---|---|---|---|---|---|---|')
    for p, n, cells in rows:
        num, nm = p.split('-', 1)
        print('| %s | %s | %d | %s |' % (num, nm, n, ' | '.join(cells[l] for l in LEGS)))
    print()
    print('Totals over %d programs:' % len(rows))
    for leg in LEGS:
        t = totals[leg]
        print('  %-8s %s' % (leg, ', '.join('%s %d' % (k, v) for k, v in sorted(t.items()))))
    print()
    if asmref:
        print('Assembler refusals (the leg is then not applicable):')
        for msg, ps in sorted(asmref.items(), key=lambda x: -len(x[1])):
            print('  %3d  %s' % (len(ps), msg))
    print('Linker refusals, by first message:')
    for (who, msg), ps in sorted(refusals.items(), key=lambda x: -len(x[1])):
        print('  %3d  %-5s %s' % (len(ps), who, msg))
        if verbose:
            print('       ' + ' '.join(ps))

if __name__ == '__main__':
    main()
