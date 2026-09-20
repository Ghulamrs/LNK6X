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
    if ! "$LNK" "tests/cmd/$cmdf" $args $fl -o "$OUT/$name.out" > "$OUT/$name.log" 2>&1; then
        printf '%-20s FAIL  %s\n' "$name" "$(head -1 "$OUT/$name.log")"; fail=$((fail+1)); return
    fi
    if cmp -s "$OUT/$name.out" "$REF/$name.out"; then
        printf '%-20s ok    %s bytes\n' "$name" "$(wc -c < "$REF/$name.out" | tr -d ' ')"
        pass=$((pass+1))
    else
        n=$(cmp -l "$OUT/$name.out" "$REF/$name.out" 2>/dev/null | wc -l | tr -d ' ')
        printf '%-20s DIFF  %s bytes differ (first: %s)\n' "$name" "$n" \
               "$(cmp "$OUT/$name.out" "$REF/$name.out" 2>&1 | sed 's/.*differ: //')"
        fail=$((fail+1))
    fi
}

# links.txt is the oracle's own list, so the bed and the probes cannot drift apart
sed 's/;.*//' tests/probes/links.txt | grep '|' > "$OUT/links"
# q10 is not in links.txt: probe.cmd builds its archive first, so it names its own link
echo "q10-ar|flat.cmd|--ram_model q10.lib|q10-ar-main" >> "$OUT/links"

while IFS='|' read -r n c f o; do
    [ -n "$n" ] && one "$n" "$c" "$f" "$o"
done < "$OUT/links"

echo "---"
echo "run.sh: $pass matched, $fail differed, $skip skipped"
[ "$fail" -gt 0 ] && exit 1
[ "$skip" -gt 0 ] && exit 2
exit 0
