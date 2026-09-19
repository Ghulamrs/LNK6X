#!/bin/sh
# The oracle probes: ship tests/ to the Windows box, have CCS 7.4's cl6x, lnk6x, ofd6x, dis6x
# and nm6x do their work there, and bring everything back - the objects, the images, the maps,
# the XML dumps and the listings. This runs nothing of this project's own; it records what
# lnk6x does, so the linker can be written against what it produced rather than against prose.
#
#   sh tests/probes.sh          (ssh alias `windows`; CCS 7.4 at C:\ti\ccsv7 on the box)
set -u
cd "$(dirname "$0")/.." || exit 1
BOX=${BOX:-windows}
ROOT=${ROOT:-C:/lnk6x-probes}
T=${T:-build/probe}
mkdir -p "$T" || exit 1
find . -name "* [0-9].*" -delete
COPYFILE_DISABLE=1 tar -C . --no-xattrs -czf "$T/tree.tgz" tests || exit 1
W=$(echo "$ROOT" | sed 's|/|\\|g')        # the same place in cmd's spelling
ssh -n -o BatchMode=yes "$BOX" "if not exist $W mkdir $W" > /dev/null || exit 1
scp -q "$T/tree.tgz" "$BOX:$ROOT/tree.tgz" || exit 1
ssh -n -o BatchMode=yes "$BOX" "cd /d $W & tar xzf tree.tgz & $W\\tests\\windows\\probe.cmd $W"
rc=$?
scp -q "$BOX:$ROOT/build/probe/*" "$T/" || exit 1
[ $rc = 0 ] || echo "probes.sh: the box reported a failure - read the .asm and .lnk logs"
for f in "$T"/*.out; do
    [ -e "$f" ] || continue
    printf '%-20s %8d bytes\n' "$(basename "$f")" "$(wc -c < "$f")"
done
echo "probes.sh: results in $T"
exit $rc
