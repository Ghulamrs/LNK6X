#!/bin/sh
# The other half of the bed. run.sh asks whether an image is right; this asks what the linker
# says when it will not make one - the refusals, and the things it must not do silently.
# Nothing here needs the Windows box: every input is made on the spot, out of tests/ref.
#
#   sh tests/bad.sh             (or `make test`, which runs run.sh first)
#
# Exit: 0 every case said what it should, 1 otherwise.

cd "$(dirname "$0")/.." || exit 1
LNK=${LNK:-build/lnk6x.exe}
OUT=${OUT:-build/test}
REF=tests/ref
CMD=tests/cmd

[ -x "$LNK" ] || { echo "bad.sh: no linker at $LNK - run make first"; exit 1; }
mkdir -p "$OUT" || exit 1

fail=0

# one <name> <expected substring> -- <the linker's arguments>
one() {
    name=$1; want=$2; shift 3
    "$LNK" "$@" > "$OUT/bad-$name.log" 2>&1
    rc=$?
    got=$(cat "$OUT/bad-$name.log")
    if [ "$rc" -eq 0 ]; then
        printf '%-22s FAIL  linked, and should not have\n' "$name"; fail=$((fail+1)); return
    fi
    case $got in
    *"$want"*) printf '%-22s ok\n' "$name" ;;
    *)         printf '%-22s FAIL  wanted "%s", said: %s\n' "$name" "$want" "$got"; fail=$((fail+1)) ;;
    esac
}

: > "$OUT/empty.obj"
one empty-file "not an ELF file" -- "$CMD/flat.cmd" --ram_model -o "$OUT/x.out" "$OUT/empty.obj"

# A MEMORY block the file ends inside. The refusal has to name the brace, not arrive later
# as `.text: no memory range for it`, which names the wrong thing (the review's N17).
printf '%s\n' '--stack_size=0x4000' 'MEMORY' '{' '    RAM : origin = 0xC0000000, length = 0x1000' \
    > "$OUT/unterminated.cmd"
one unterminated-memory "MEMORY block is not closed" -- "$OUT/unterminated.cmd" --ram_model \
    -o "$OUT/x.out" "$REF/q01-bare.obj"

# Two objects that both define _c_int00. lnk6x says "symbol redefined" and stops; this linker
# used to keep the first and say nothing (the review's N16, B7).
one duplicate-definition "symbol redefined" -- "$CMD/flat.cmd" --ram_model -o "$OUT/x.out" \
    "$REF/q01-bare.obj" "$REF/q01-bare.obj"

# A library that is nowhere to be found is named, not silently skipped.
one missing-library "cannot open" -- "$CMD/flat.cmd" --ram_model -o "$OUT/x.out" \
    "$REF/q01-bare.obj" -l no_such_library.lib

# -i is searched for a -l name given bare, which is how RIDE passes the runtime.
if "$LNK" "$CMD/flat.cmd" --ram_model -i "$REF" -o "$OUT/lp.out" \
        "$REF/q10-ar-main.obj" -l q10.lib > "$OUT/bad-libdir.log" 2>&1 && [ -f "$OUT/lp.out" ]; then
    printf '%-22s ok\n' libdir-search
else
    printf '%-22s FAIL  %s\n' libdir-search "$(head -1 "$OUT/bad-libdir.log")"; fail=$((fail+1))
fi

# --rom_model composes a .cinit table now, so what this asks has turned round: it used to
# check the linker *said* it composed none (the review's N9). q01-bare has no initialised
# data at all, so the right answer for it is a link that succeeds and says nothing - a
# table with no records is not an error, and neither is a warning nobody needs.
if "$LNK" "$CMD/flat.cmd" --rom_model -o "$OUT/rom.out" "$REF/q01-bare.obj" \
        > "$OUT/bad-rommodel.log" 2>&1 && [ ! -s "$OUT/bad-rommodel.log" ]; then
    printf '%-22s ok\n' rom-model-quiet
else
    printf '%-22s FAIL  %s\n' rom-model-quiet "$(head -1 "$OUT/bad-rommodel.log")"; fail=$((fail+1))
fi

# A branch out of PCR_S21's reach goes through a trampoline now, lnk6x's `$Tramp$S$$callee`
# (run.sh holds q15's image to lnk6x's byte for byte). What stays refused by name is every
# other bounded field out of range (the review's L-A5): q15's own relocation turned into a
# PCR_S12 - one byte of its .rel.text - is a 12-bit branch 16 MB long. q15's CALLP sits at
# 0xC0000000, so PCR_S21 holds -0x100000..0xFFFFF words: .fartext at 0xC03FFFE0 is in reach
# and needs none, at 0xC0400000 is one fetch packet past it and gets one. The boundary pair
# says the reach is neither late nor early.
far_at() {
    sed "s/origin = 0xC1000000/origin = $1/" "$CMD/far.cmd" > "$OUT/far-$1.cmd"
}
# linked <name> <map must say> <map must not say> -- <the linker's arguments>
linked() {
    name=$1; want=$2; not=$3; shift 4
    if "$LNK" "$@" -m "$OUT/$name.map" > "$OUT/bad-$name.log" 2>&1 && [ ! -s "$OUT/bad-$name.log" ] \
            && { [ -z "$want" ] || grep -qF "$want" "$OUT/$name.map"; } \
            && { [ -z "$not" ] || ! grep -qF "$not" "$OUT/$name.map"; }; then
        printf '%-22s ok\n' "$name"
    else
        printf '%-22s FAIL  %s\n' "$name" "$(head -1 "$OUT/bad-$name.log")"; fail=$((fail+1))
    fi
}
linked far-call '$Tramp$S$$faraway' '' -- "$CMD/far.cmd" --ram_model -o "$OUT/x.out" "$REF/q15-far.obj"
# --rom_model relocates twice, and the trampoline has to survive the second layout.
linked far-call-rom '$Tramp$S$$faraway' '' -- "$CMD/far.cmd" --rom_model -o "$OUT/x.out" "$REF/q15-far.obj"
far_at 0xC0400000
linked far-call-edge '$Tramp$S$$faraway' '' -- "$OUT/far-0xC0400000.cmd" --ram_model -o "$OUT/x.out" "$REF/q15-far.obj"
far_at 0xC03FFFE0
linked far-call-in-reach '' 'Tramp' -- "$OUT/far-0xC03FFFE0.cmd" --ram_model -o "$OUT/near.out" "$REF/q15-far.obj"
# q15's .rel.text is at file offset 0x11c; its one entry's type byte is at 0x120.
cp "$REF/q15-far.obj" "$OUT/q15-s12.obj"
printf '\005' | dd of="$OUT/q15-s12.obj" bs=1 seek=288 conv=notrunc 2>/dev/null
one far-branch-s12 "R_C6000_PCR_S12 to \"faraway\" (0xc1000000) at q15-s12.obj(.text)+0x0" -- \
    "$CMD/far.cmd" --ram_model -o "$OUT/x.out" "$OUT/q15-s12.obj"
one far-branch-s12-rom "does not fit a signed 12-bit field" -- \
    "$CMD/far.cmd" --rom_model -o "$OUT/x.out" "$OUT/q15-s12.obj"

echo "---"
[ "$fail" -eq 0 ] && { echo "bad.sh: every case said what it should"; exit 0; }
echo "bad.sh: $fail case(s) did not"
exit 1
