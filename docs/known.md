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

**`.stack` and `.sysmem` are sized only when their names are asked for.** q07's are 0x4000 and
0x1000 - `--stack_size` and `--heap_size` exactly - and no input section contributes to either;
every probe that links no runtime leaves both empty although the same command file sets both
options. So the trigger taken here is a reference to `__TI_STACK_END`/`__TI_STACK_SIZE` or to
`__TI_SYSMEM_SIZE`. That fits all twenty images and is not the same as knowing the rule.

**The six invented symbols are a fixed list.** `binit`, `__binit__`, `__c_args__`,
`__TI_pprof_out_hndl`, `__TI_prof_data_start`, `__TI_prof_data_size`, always `SHN_ABS` and
always `0xFFFFFFFF`. In a link that brings in the runtime, some of them stop being absolute -
`binit` becomes the address of the boot table - and this linker has no idea yet what decides it.

## Rules read rather than understood

**`.bss` is allocated before everything else.** q03 forces it and `__TI_STATIC_BASE` explains
it, but only one probe shows it. A command file that gives another section an explicit address
below `.bss`, or one with two `.bss`-like sections, would say whether the rule is about `.bss`
by name or about the near region in general.

**PCR_S21 is relative to the fetch packet - settled.** q11 puts a `CALLP` at offset 8 of its
packet and this linker matches lnk6x byte for byte, so `target - (P & ~31)` is the rule and
`target - P` is not. The open question the earlier text left is closed.

**PCR_L16 and PCR_H16 are not applied, and the bed says why not.** The only place they appear
is `tdeh_uwentry_c6000.obj` in the runtime, three pairs of them, and q07-lib.out has the
relocated words. Held against it (the section runs at 0xC00066E0):

| at | symbol | S | A | P | L16 field | H16 field |
|----|--------|---|---|---|-----------|-----------|
| +000 | __TI_Unwind_RaiseException | C0007940 | -8 | C00066E0 | 1260 | 0000 |
| +040 | __TI_Unwind_Resume | C0006E00 | +56 | C0006720 | 0720 | 0000 |
| +0B8 | __TI_cxa_end_cleanup | C0007DE0 | -20 | C0006798 | 1660 | 0000 |

`S + A - P` gives 1258, 0718, 1634 and `S + A - (P & ~31)` gives 1258, 0718, 164C, so neither
is it; the implied targets are S, S+0x40 and S+0x18, which is not a constant relation to the
addend either. The same object's ABS_L16 and ABS_H16 in the same table read correctly with the
bits-7..22 field, so the field is being read right. Until a rule fits all three, the linker
refuses the relocation by name rather than writing a word it cannot justify. PREL31 (25) and
EHTYPE (28) are refused for the same reason and have not been measured at all - PREL31 is in
27 members of the runtime and in every C++ program's unwind tables.

**The `-l`/archive pull rule for weak names is assumed, not read.** An undefined *weak* name
does not pull a member out of an archive here, which is what ELF means by weak and why the
runtime's optional hooks do not drag their implementations in. Nothing in the bed forces it: a
probe with a weak reference to a name an archive member defines would.

**An empty initialised section takes its range's origin.** Not the high-water mark - q06 shows
`.fast` at `0xC0000000` after `.far` has already taken those four bytes. One image says so.

## What the library probes still differ in

q05-model-rom, q07-lib and q16-ride link now and none of the three is byte-identical. Each
difference below is a rule read off the oracle's map or image and not yet implemented, and
they compound, so the three are not useful as regression tests until the first few are in.

  * **No `.cinit` table.** Under `--rom_model` lnk6x composes the compressed load images, a
    handler table of one pointer per decompressor, and a cinit table of `{load, run}` records,
    and brackets the two with `__TI_CINIT_Base/Limit` and `__TI_Handler_Table_Base/Limit`.
    q05-model-rom shows the shape at 0xC00002E8. This linker says on stderr that it composes
    none of it and leaves the bounds empty, rather than writing a ram-model image quietly.
  * **The allocation order with a library is not the SECTIONS order.** q07's addresses come out
    `.stack`, `.text`, `.sysmem`, `.const`, `.c6xabi.extab`, `.fardata`, `.switch`, `.cinit`,
    `.c6xabi.exidx` - the first four in descending size, the rest not - where flat.cmd names
    them `.text`, `.const`, ..., `.stack`, `.sysmem`. What decides it is unread.
  * **The input sections inside `.text` are not in input order either.** q07's are in
    descending size for the whole of the run. One image says so.
  * **`.c6xabi.exidx` is not sorted.** lnk6x sorts the index by function address and brackets
    it with `__TI_UNWIND_TABLE_START/END`, which this linker defines but does not sort behind.
  * **The attributes blob is still a constant** - see above - and q07's is the merge of five
    distinct blobs the runtime's members carry.
  * **No trampolines.** q15 is the probe: a call 16 MB away gets a 32-byte `$Tramp$S$$name`
    appended to `.text` and listed in the map. This linker writes the call as it stands, and
    q15 differs by 906 bytes.

## Not implemented

Each is a refusal, not a silent wrong answer: the linker says so and stops.

  * PREL31, EHTYPE, PCR_L16 and PCR_H16 - see above.
  * `START`, `END`, `SIZE`, `LOAD_START` and the other address operators, `GROUP`, `UNION`,
    `PAGE`, `type = COPY|DSECT|NOLOAD`, and expression assignments in a `SECTIONS` entry. The
    input-section list and subsection form are read now; the rest are not.
  * Load-against-run placement is implemented to the extent the command file can ask for it,
    and is untested: q08 was written for it, but its `.fast` has no reference and was eliminated
    before it could be placed, so the run/load probe currently proves nothing. A probe whose
    run-placed section is actually referenced would fix that.
  * `--retain`, `--unused_section_elimination=off`, and `.clink` - elimination is always on.
  * `.TI.symbol.alias`, symbol versioning.
  * No `.map` file is written. `-m` is accepted and ignored - and the oracle's map is the
    readable evidence this whole bed is built on, so this is the next thing worth having.

## Things that are this linker's own

Nothing. There is no equivalent here of LINK's `/timestamp:`, because a TI image carries no
time of day to pin.
