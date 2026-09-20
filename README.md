# LNK6X

A linker for the TMS320C6000 — the C674x core of the C6747 — in C-style ISO C++14. It takes the
TI ELF objects [ASM6x](../ASM6x) writes (and those `cl6x` writes), places them as a linker
command file directs, and produces the executable `lnk6x` produces. It is the C6000 counterpart
of [LINK](../LINK), the Windows PE linker, and lives beside it rather than inside it — as ASM6x
lives beside MASM.

TI's `lnk6x` (CCS 7.4, C6000 CGT 8.2.2, on the Windows box) is the oracle. Nothing here is
designed before its output has been read.

## Where things are

    src/lnk.h           what the passes say to each other
    src/elf.cpp         one TI ELF32 object, read
    src/cmd.cpp         the linker command file: MEMORY, SECTIONS, load and run
    src/link.cpp        elimination, output sections, allocation, relocation
    src/reloc.cpp       the C6000 fixups
    src/image.cpp       the ELF32 executable, its segments and its symbol table
    src/main.cpp        the command line, in lnk6x's spelling
    tests/ref/          the objects, the images and the maps lnk6x made: the bed's input
    tests/run.sh        links every probe again and compares the image byte for byte
    tests/bad.sh        what the linker says when it will not make an image at all
    tests/probes/       the probes: the smallest input that forces one linker decision each
    tests/cmd/          linker command files - TI's kind, not cmd.exe's
    tests/windows/      what has to run on the box (cl6x, lnk6x, ofd6x, dis6x, nm6x)
    tests/probes.sh     ships the probes to the box, runs them, brings the results back
    docs/elf-observed.md what lnk6x does, read off the bed
    docs/known.md       what this linker does differently, or not at all

## Building and testing

    make                                    -> build/lnk6x.exe
    make test                               -> tests/run.sh, then tests/bad.sh

The test is the comparison: each probe is linked again from the objects in `tests/ref` and the
image is held against lnk6x's, byte for byte. A TI image carries no time stamp and no build
path, so nothing has to be pinned first - the two files either agree or they do not.

Eight of them agree today: q01, both halves of q02, q03, q04, q05-model-ram, q06 and q08. Two
link TI's runtime, which is not ours to check in, and four were added to links.txt after the
box last ran and have no reference image yet. One run of

    sh tests/probes.sh

settles both: it files everything the box returns into `tests/ref`, the runtime library
included.

Both kinds of file are called `.cmd`, which is unfortunate but is what each tool wants:
`tests/cmd/*.cmd` are read by `lnk6x`, `tests/windows/*.cmd` by `cmd.exe`.

## The probes

    q01-bare        one code section: the floor of what lnk6x will produce
    q02-place       named sections, linked flat and then split: where each lands, and why
    q03-bss         .bss and .usect: allocated, but with no bytes in the file
    q04-cross       two objects: a cross-object call and the PCR_S21 that resolves it
    q05-model       one object linked --ram_model and --rom_model: what differs, cinit included
    q06-near        .neardata, .const, .far: near placement and the static base
    q07-lib         a reference into rts6740_elf_eh.lib: which members come, and what they drag
    q08-runload     load address against run address, TI's own trick and not the PE linker's

The command files vary one thing each: `flat.cmd` one memory range and no opinions, `split.cmd`
L2RAM against DDR with an alignment and a fill, `runload.cmd` a section loaded in one place and
run in another.

Where a command-file spelling here turns out not to be the one `lnk6x` accepts, its `.lnk` log
says so and the spelling is corrected — that is what the bed is for, and cheaper than reading it
out of the manual and believing it.

## Running them

The Windows box does the work; the Mac ships the tree and reads what comes back.

    sh tests/probes.sh          # needs the ssh alias `windows`

Everything lands in `build/probe`: the objects, the images, the `.map` files, an `ofd6x -x` XML
dump and a `dis6x` listing of each, and `nm6x` for the symbols.
