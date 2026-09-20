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

# --rom_model has no .cinit table behind it yet, and has to say so rather than write a
# ram-model image under a rom-model name (the review's N9).
"$LNK" "$CMD/flat.cmd" --rom_model -o "$OUT/rom.out" "$REF/q01-bare.obj" \
    > "$OUT/bad-rommodel.log" 2>&1
if grep -q 'no .cinit table' "$OUT/bad-rommodel.log"; then
    printf '%-22s ok\n' rom-model-says-so
else
    printf '%-22s FAIL  said nothing about .cinit\n' rom-model-says-so; fail=$((fail+1))
fi

echo "---"
[ "$fail" -eq 0 ] && { echo "bad.sh: every case said what it should"; exit 0; }
echo "bad.sh: $fail case(s) did not"
exit 1
