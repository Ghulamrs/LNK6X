#!/bin/sh
# The ratification corpus: every program under tests/corpus, taken through the two chains and
# compared. The Mac compiles (cc1i, cxx1i, shci write the C6000 assembly here, since they are
# Mac binaries; a Shalimar program brings the runtime's six .s from RIDE's lib/shmrt-tms6747
# with it); the Windows box assembles each module twice - CCS 7.4's cl6x, and this project's
# asm6x - and links each set twice - TI's lnk6x on RIDE's exact line and command file, and this
# linker built there by cl from src/. tests/windows/corpus.cmd is the box's half; this half
# ships the tree, brings the results back into build/corpus, and tests/corpus-report.py turns
# them into the table the review reads. Nothing runs the images: vm6747 reads assembly, not an
# ELF executable, so the four .out files of a program are compared with each other instead.
#
#   sh tests/corpus.sh              compile, ship, run on the box, report
#   sh tests/corpus.sh compile      compile only (no box)
#   sh tests/corpus.sh ship         ship what compile left and run the box's half
#   sh tests/corpus.sh report       report from what build/corpus already holds
#
# Each tests/corpus/NN-name/manifest names its modules in link order; the extension says which
# compiler: .c cc1i, .cpp cxx1i, .shl/.shm shci (the first only - shci compiles the files beside
# it into the same assembly), .s as written.
set -u
cd "$(dirname "$0")/.." || exit 1
BIN=$(cd "${BIN:-../RIDE/bin}" && pwd)
CC1I=${CC1I:-$BIN/cc1i.exe}; CXX1I=${CXX1I:-$BIN/cxx1i.exe}; SHCI=${SHCI:-$BIN/shci.exe}
SHMRT=${SHMRT:-$BIN/lib/shmrt-tms6747}
BOX=${BOX:-windows}
ROOT=${ROOT:-C:/lnk6x-probes/corpus}
T=${T:-build/corpus}
what=${1:-all}

compile() {
    rm -rf "$T"; mkdir -p "$T" || exit 1
    n=0; refused=0
    for d in tests/corpus/*/; do
        p=$(basename "$d"); out="$T/$p"; mkdir -p "$out"
        modules=$(sed -n 's/^modules=//p' "$d/manifest")
        shm=0; objs=""
        for m in $modules; do
            b=${m%.*}
            case $m in
            *.c)   "$CC1I" -S -arch tms6747 -I "$d" "$d/$m" -o "$out/$b.s" 2> "$out/$b.cc.err" < /dev/null || { echo "REFUSED $p/$m: $(grep -v '©' "$out/$b.cc.err" | head -1)"; refused=$((refused+1)); } ;;
            *.cpp) "$CXX1I" -arch tms6747 -S "$d/$m" -o "$out/$b.s" 2> "$out/$b.cc.err" < /dev/null || { echo "REFUSED $p/$m: $(grep -v '©' "$out/$b.cc.err" | head -1)"; refused=$((refused+1)); } ;;
            *.shl|*.shm)
                   [ $shm = 1 ] && continue
                   shm=1; ( cd "$d" && "$SHCI" --target=tms6747 -S "$m" -o "$OLDPWD/$out/$b.s" ) 2> "$out/$b.cc.err" < /dev/null || { echo "REFUSED $p/$m: $(grep -v '©' "$out/$b.cc.err" | head -1)"; refused=$((refused+1)); } ;;
            *.s)   cp "$d/$m" "$out/$b.s" ;;
            esac
            [ -f "$out/$b.s" ] && objs="$objs $b"
        done
        if [ $shm = 1 ]; then
            # the Shalimar runtime for the C6000 is assembly RIDE ships; it links after the program
            for r in "$SHMRT"/*.s; do
                rb=$(basename "$r" .s); cp "$r" "$out/shmrt-$rb.s"; objs="$objs shmrt-$rb"
            done
        fi
        echo "$p|$objs" > "$out/link.txt"
        n=$((n+1))
    done
    echo "corpus.sh: $n programs compiled, $refused modules refused"
}

ship() {
    find . -name "* [0-9].*" -delete
    COPYFILE_DISABLE=1 tar -C . --no-xattrs -czf "$T/tree.tgz" src tests/cmd/ride.cmd tests/windows/corpus.cmd "$T"/[0-9]* || exit 1
    W=$(echo "$ROOT" | sed 's|/|\\|g')
    perl -e 'alarm 1800; exec @ARGV' ssh -n -o BatchMode=yes "$BOX" "if not exist $W mkdir $W" > /dev/null || exit 1
    scp -q "$T/tree.tgz" "$BOX:$ROOT/tree.tgz" || exit 1
    perl -e 'alarm 1800; exec @ARGV' ssh -n -o BatchMode=yes "$BOX" "cd /d $W & tar xzf tree.tgz & $W\\tests\\windows\\corpus.cmd $W" | tr -d '\r'
    scp -q "$BOX:$ROOT/results.tgz" "$T/results.tgz" || exit 1
    tar xzf "$T/results.tgz" || exit 1      # the members are build/corpus/... already
}

case $what in
compile) compile ;;
ship)    ship ;;
report)  python3 tests/corpus-report.py "$T" ;;
all)     compile; ship; python3 tests/corpus-report.py "$T" ;;
*)       echo "corpus.sh: compile | ship | report | (nothing)"; exit 2 ;;
esac
