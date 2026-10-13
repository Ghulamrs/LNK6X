#!/bin/sh
# tests/debug-line.sh - .debug_line carried into the image (link.cpp, build_sections): tests/ref/dwline.obj is
# ASM6x's object of its tests/debug-line/rows.s - cl6x's own rows for t.c, asm6x's 86 bytes, two set_address rows relocated against .text; linked
# alone with flat.cmd the image carries that unit with its address final, in no segment, after the loaded
# sections; q01-bare.obj, which has no line table, is linked as it always was - no .debug_line at all.
set -u
cd "$(dirname "$0")/.." || exit 1
LNK=${LNK:-build/lnk6x.exe}
OUT=${OUT:-build/test}; mkdir -p "$OUT"
"$LNK" -mv6740 --abi=eabi tests/cmd/flat.cmd -e main tests/ref/dwline.obj -o "$OUT/dwline.out" > "$OUT/dwline.log" 2>&1 || { echo "debug-line: the link failed"; cat "$OUT/dwline.log"; exit 1; }
"$LNK" -mv6740 --abi=eabi tests/cmd/flat.cmd tests/ref/q01-bare.obj -o "$OUT/dwline-none.out" > "$OUT/dwline-none.log" 2>&1 || { echo "debug-line: the plain link failed"; cat "$OUT/dwline-none.log"; exit 1; }
python3 -I - "$OUT/dwline.out" "$OUT/dwline-none.out" <<'PY' || exit 1
import struct, sys
def sects(path):
    d=open(path,'rb').read(); shoff=struct.unpack_from('<I',d,0x20)[0]; shentsize,shnum,shstrndx=struct.unpack_from('<HHH',d,0x2e)
    hdr=[struct.unpack_from('<IIIIIIIIII',d,shoff+i*shentsize) for i in range(shnum)]; sto=hdr[shstrndx][4]
    phoff=struct.unpack_from('<I',d,0x1c)[0]; phnum=struct.unpack_from('<H',d,0x2c)[0]
    segs=[struct.unpack_from('<IIIIIIII',d,phoff+32*i) for i in range(phnum)]
    return d,{d[sto+h[0]:d.index(b'\0',sto+h[0])].decode():h for h in hdr},segs
d,s,segs=sects(sys.argv[1]); ok=True
dl=s.get('.debug_line'); text=s['.text']
if not dl: print('debug-line: FAILED - no .debug_line in the image'); sys.exit(1)
flags, addr, size = dl[2], dl[3], dl[5]
if flags != 0 or addr != 0 or size != 86: ok=False; print('debug-line: header', hex(flags), hex(addr), size)
if any(seg[1] <= dl[4] < seg[1]+seg[4] for seg in segs): ok=False; print('debug-line: inside a segment')
hl=struct.unpack_from('<I',d,dl[4]+6)[0]; prog=dl[4]+10+hl
first=struct.unpack_from('<I',d,prog+3)[0]
if d[prog]!=0 or d[prog+2]!=2 or first != text[3]: ok=False; print('debug-line: set_address %x, .text at %x' % (first, text[3]))
d2,s2,_=sects(sys.argv[2])
if '.debug_line' in s2: ok=False; print('debug-line: a plain link grew a .debug_line')
print('debug-line: %s (unit of %d bytes after the loaded sections, set_address = .text at 0x%x; the plain link has none)' % ('ok' if ok else 'FAILED', size, first))
sys.exit(0 if ok else 1)
PY
