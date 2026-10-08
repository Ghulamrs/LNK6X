#!/bin/sh
# The bed. Every link in tests/probes/links.txt, made again by this linker from the objects in
# tests/ref, and the image compared with lnk6x's byte for byte. Nothing here needs the Windows
# box: the objects and the reference images are checked in, which is the whole point of keeping
# them there rather than in build/probe, where the .gitignore would lose them.
#
#   sh tests/run.sh             (or `make test`, which builds first)
#
# Exit: 0 all compared and matched, 1 something differed, 2 nothing differed but a probe had to
# be skipped. Two probes link TI's runtime, which is not ours to check in, and three were added
# to links.txt after the last run of the box and have no reference image yet; `sh tests/probes.sh`
# settles both.

cd "$(dirname "$0")/.." || exit 1
LNK=${LNK:-build/lnk6x.exe}
REF=tests/ref
OUT=${OUT:-build/test}

[ -x "$LNK" ] || { echo "run.sh: no linker at $LNK - run make first"; exit 1; }
mkdir -p "$OUT" || exit 1

fail=0; skip=0; pass=0

# A probe listed in tests/known-differ.txt may differ region by region by up to the bytes pinned
# there (tests/regions.py): a difference docs/known.md explains. More bytes, a region no pin names,
# or an unlisted probe still fails the bed.
known_bytes() { sed 's/;.*//' tests/known-differ.txt 2>/dev/null | awk -v n="$1" '$1 == n { $1 = ""; print substr($0, 2) }'; }
known=0
PY=${PYTHON:-}
[ -z "$PY" ] && for p in python3 python; do "$p" -c 1 >/dev/null 2>&1 && { PY=$p; break; }; done

one() {
    name=$1; cmdf=$2; flags=$3; objs=$4
    args=""; missing=""
    for o in $objs; do
        [ -f "$REF/$o.obj" ] || missing="$missing $o.obj"
        args="$args $REF/$o.obj"
    done
    fl=""
    for t in $flags; do
        case $t in
        *.lib)  [ -f "$REF/$t" ] || missing="$missing $t"
                args="$args $REF/$t" ;;
        -l)     ;;
        *)      fl="$fl $t" ;;
        esac
    done
    if [ ! -f "$REF/$name.out" ]; then
        printf '%-20s SKIP  no reference image - the box has not run this probe yet\n' "$name"
        skip=$((skip+1)); return
    fi
    if [ -n "$missing" ]; then
        printf '%-20s SKIP  missing:%s\n' "$name" "$missing"; skip=$((skip+1)); return
    fi
    # `refused` in known-differ.txt: lnk6x makes this image and this linker must say it cannot
    # (q15, the far call, until trampolines land). Linking it anyway is a failure.
    if [ "$(known_bytes "$name")" = refused ]; then
        if "$LNK" "tests/cmd/$cmdf" $args $fl -o "$OUT/$name.out" > "$OUT/$name.log" 2>&1; then
            printf '%-20s FAIL  linked, and is listed as refused - take it off tests/known-differ.txt\n' "$name"
            fail=$((fail+1))
        else
            printf '%-20s KNOWN refused: %s\n' "$name" "$(head -c 100 "$OUT/$name.log")"; known=$((known+1))
        fi
        return
    fi
    if ! "$LNK" "tests/cmd/$cmdf" $args $fl -o "$OUT/$name.out" > "$OUT/$name.log" 2>&1; then
        printf '%-20s FAIL  %s\n' "$name" "$(head -1 "$OUT/$name.log")"; fail=$((fail+1)); return
    fi
    if cmp -s "$OUT/$name.out" "$REF/$name.out"; then
        printf '%-20s ok    %s bytes\n' "$name" "$(wc -c < "$REF/$name.out" | tr -d ' ')"
        pass=$((pass+1))
        if [ -n "$(known_bytes "$name")" ]; then
            printf '%-20s       matches now - take it off tests/known-differ.txt\n' "$name"; fail=$((fail+1))
        fi
    else
        k=$(known_bytes "$name")
        if [ -z "$k" ]; then
            printf '%-20s DIFF  %s\n' "$name" "$(${PY:-false} tests/regions.py "$OUT/$name.out" "$REF/$name.out" 2>/dev/null ||
                cmp "$OUT/$name.out" "$REF/$name.out" 2>&1)"
            fail=$((fail+1)); return
        fi
        [ -n "$PY" ] || { printf '%-20s FAIL  pinned by region, and no python to count them\n' "$name"; fail=$((fail+1)); return; }
        # shellcheck disable=SC2086 - the pins are words, one region each
        verdict=$("$PY" tests/regions.py "$OUT/$name.out" "$REF/$name.out" $k); rc=$?
        case $rc in
        0) printf '%-20s KNOWN %s\n' "$name" "$verdict"; known=$((known+1)) ;;
        3) printf '%-20s KNOWN within its pins, some lower now - lower them: %s\n' "$name" "$verdict"; known=$((known+1)) ;;
        *) printf '%-20s DIFF  %s\n' "$name" "$verdict"; fail=$((fail+1)) ;;
        esac
    fi
}

# links.txt is the oracle's own list, so the bed and the probes cannot drift apart
sed 's/;.*//' tests/probes/links.txt | grep '|' > "$OUT/links"
# q10 is not in links.txt: probe.cmd builds its archive first, so it names its own link
echo "q10-ar|flat.cmd|--ram_model q10.lib|q10-ar-main" >> "$OUT/links"
# links744.txt: the far-call probes as CCS 5.5's lnk6x 7.4.4 linked them (<name>-744), held to
# this linker's --cgt=7.4.4
sed 's/;.*//' tests/probes/links744.txt | grep '|' |
    awk -F'|' '{ printf "%s-744|%s|%s --cgt=7.4.4|%s\n", $1, $2, $3, $4 }' >> "$OUT/links"

while IFS='|' read -r n c f o; do
    [ -n "$n" ] && one "$n" "$c" "$f" "$o"
done < "$OUT/links"

echo "---"
echo "run.sh: $pass matched, $known known, $fail differed, $skip skipped"
[ "$fail" -gt 0 ] && exit 1
[ "$skip" -gt 0 ] && exit 2
exit 0
