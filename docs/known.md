# What this linker does not do, and what it does differently

The bed compares images byte for byte, so anything deliberate or unexplained has to be written
down rather than left for a later reader to rediscover. This is that list, and it is the
sibling of `LINK/docs/known.md`.

## Constants standing in for rules

**`.c6xabi.attributes` and `.TI.section.flags` are copied, not composed.** Both are identical
in all eight images that link no runtime library, and different in the two that do. So this
linker writes the eight-image value and would be wrong the moment a link brings in a library or
an object built for another core. What they actually encode - the attribute subsections are TI's
and c6xabi's, and the section-flags record is 26 bytes of which only the first is non-zero - is
readable, but nothing in the bed forces a particular composition yet. A probe that links two
objects built for different `-mv` values would.

**The six invented symbols are a fixed list.** `binit`, `__binit__`, `__c_args__`,
`__TI_pprof_out_hndl`, `__TI_prof_data_start`, `__TI_prof_data_size`, always `SHN_ABS` and
always `0xFFFFFFFF`. In a link that brings in the runtime, some of them stop being absolute -
`binit` becomes the address of the boot table - and this linker has no idea yet what decides it.

## Rules read rather than understood

**`.bss` is allocated before everything else.** q03 forces it and `__TI_STATIC_BASE` explains
it, but only one probe shows it. A command file that gives another section an explicit address
below `.bss`, or one with two `.bss`-like sections, would say whether the rule is about `.bss`
by name or about the near region in general.

**PCR_S21 is relative to the fetch packet.** Every branch in the bed sits at offset 0 of its
32-byte packet, so the bed cannot tell `target - (P & ~31)` from `target - P`. This linker
clears the low five bits, which is what the C6000 ABI says; a probe with a branch anywhere but
the first slot of a packet would confirm it here.

**An empty initialised section takes its range's origin.** Not the high-water mark - q06 shows
`.fast` at `0xC0000000` after `.far` has already taken those four bytes. One image says so.

## Not implemented

Each is a refusal, not a silent wrong answer: the linker says so and stops.

  * Archives. `-l` takes a file and links it whole; no symbol index is read and no member is
    chosen. q07 and q10 are the probes for it, and q10's archive has not been built on the box
    yet.
  * `--rom_model` and the cinit table. links.txt already records why the bed could not link a
    rom-model image without the runtime: every cinit record names its decompressor, and those
    live in the library.
  * `--stack_size` and `--heap_size` are parsed and ignored; nothing in the bed allocates
    `.stack` or `.sysmem`, because nothing references them.
  * Input-section lists inside a `SECTIONS` entry (`.text { a.obj(.text) }`), subsection
    specifications (`.text:_func`), `START`, `END`, `SIZE`, `LOAD_START` and the other address
    operators, `GROUP`, `UNION`, `PAGE`, `type = COPY|DSECT|NOLOAD`, and expression assignments.
    q09 is the subsection probe and has no reference image yet.
  * Load-against-run placement is implemented to the extent the command file can ask for it,
    and is untested: q08 was written for it, but its `.fast` has no reference and was eliminated
    before it could be placed, so the run/load probe currently proves nothing. A probe whose
    run-placed section is actually referenced would fix that.
  * `--retain`, `--unused_section_elimination=off`, and `.clink` - elimination is always on.
  * Weak symbols, COMMON symbols, `.TI.symbol.alias`, symbol versioning.
  * No `.map` file is written. `-m` is accepted and ignored.

## Things that are this linker's own

Nothing. There is no equivalent here of LINK's `/timestamp:`, because a TI image carries no
time of day to pin.
