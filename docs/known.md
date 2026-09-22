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

**PCR_L16 and PCR_H16 are applied, and the rule was read off the oracle's own bytes**
(2026-09-22). They occur in one place only - `tdeh_uwentry_c6000.obj`, three pairs, and a
scan of all 456 members of `rts6740_elf_eh.lib` says there is no fourth site. Held against
q07-lib.out (the contribution runs at 0xC00066E0):

| at | symbol | S | A | P | L16 field | H16 field |
|----|--------|---|---|---|-----------|-----------|
| +000 | __TI_Unwind_RaiseException | C0007940 | -8 | C00066E0 | 1260 | 0000 |
| +040 | __TI_Unwind_Resume | C0006E00 | +56 | C0006720 | 0720 | 0000 |
| +0B8 | __TI_cxa_end_cleanup | C0007DE0 | -20 | C0006798 | 1660 | 0000 |

`S + A - P` gives 1258, 0718, 1634 and `S + A - (P & ~31)` gives 1258, 0718, 164C, so
neither is it - and that is where this note stopped for a while. What settled it was the
object's own labels: `base_pcr` at +0x08 and `cxa_base_pcr` at +0xB4, which are the base
argument of TI's `$PCR_OFFSET(dest, base)`. The addend says which one:

    A = (P & ~31) - base           so   base = (P & ~31) - A

and the value is measured from *that label's* fetch packet, not the instruction's:

    V = S - ( ((P & ~31) - A) & ~31 )

1260, 0720, 1660 - all three. `r_addend` is therefore not added to S at all; it is how the
base is carried. The same value's high half goes in H16, which is why all three read 0000.

Two things made it findable. The object's *pre-link* words already hold the addend in the
field (FFF8, 0038, FFF4), which is what identified the addend as a section-relative
quantity rather than part of the target; and q17 links the same program from two ranges
0x1234000 apart, where all three fields come out identical - so the value is a difference
of two addresses that shift together, which rules out anything absolute.

Verified in four C++ programs of the corpus (06-smart, 07-vector3, 08-cxx1lab,
23-template-container): every site in this linker's own images satisfies the rule against
that image's own addresses, and the corpus has no linker refusal left.

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

  * ~~No `.cinit` table~~ - **composed since 2026-09-22**, and the notes below are what it
    was built from. What is still not identical to lnk6x's is the *order of contributions
    inside* an output section, which is the layout difference listed further down: q18's
    `.fardata` comes out with its words in a different order, so its compressed image
    differs even though it decodes to the right 32 bytes. q05, whose `.data` is one word,
    is byte-identical but for the two handler addresses, which move with the layout.
    Under `--rom_model` lnk6x composes the load images, a handler table of one pointer per
    decompressor, and a cinit table of `{load, run}` records, and brackets the two with
    `__TI_CINIT_Base/Limit` and `__TI_Handler_Table_Base/Limit`. This linker says on stderr
    that it composes none of it, rather than writing a ram-model image quietly.

    **The layout, off q05-model-rom and q18-cinit.** `.cinit` holds, in order: every load
    image; then the handler table, 4-aligned, one 32-bit pointer per decompressor, which
    `__TI_Handler_Table_Base/Limit` bracket; then padding; then the records, 8-aligned, two
    words each `{load, run}`, which `__TI_CINIT_Base/Limit` bracket. The initialised section
    itself becomes SHT_NOBITS at its run address - q05's `.data` is type 8 in the image - so
    its bytes live only in `.cinit`. The first byte of a load image is the handler's index
    into that table. Both samples list two handlers, `__TI_decompress_rle24` at 0 and
    `__TI_decompress_none` at 1, and both choose rle24 even for four bytes.

    **What `__TI_decompress_none` expects**, read from its own instructions in q05.out
    (`dis6x`, at 0xC0000280) rather than guessed:

        ADD 3,A4,A3 ; LDW *+A3[0],A6 ; ADD 7,A4,B5 ; MV B4,A4 ; MV B5,B4 ; B memcpy

    that is `memcpy(run, load + 7, *(u32 *)(load + 3))`. So an uncompressed image is one
    index byte, two of padding, a 32-bit length, then the bytes - and its load address must
    be **1 modulo 4**, or the length word is unaligned. That is a whole format, and it is
    enough to compose a correct table without implementing TI's compressor; what it will not
    give is an image byte-identical to lnk6x's, which chooses rle24.

    **And rle24's stream, read the same way** (`__TI_decompress_rle_core` at 0xC0000000).
    `__TI_decompress_rle24` tail-calls it with 1 in A6. The core takes the first byte of the
    stream as an **escape value** E, then reads bytes: one that is not E is stored literally
    and it goes round again; one that is E introduces a run - the next byte is the count,
    the one after it the value, and it calls `memset` and advances. A count of zero is the
    long form: two or three further bytes are shifted and or-ed into a wider length (A6 is
    what makes it 24-bit rather than 16), and a count below four means the run is of E
    itself, so an escape can be emitted without one. dst advances by the count each time.

    **The caller convention, from `_auto_init_elf` in q18.out (0xC0006200).** It takes the
    record count as `(__TI_CINIT_Limit - __TI_CINIT_Base) / 8`, walks the records two words
    at a time, and then:

        LDB *+A4[0],A3 ; ADD A4,1,A4 ; LDW *+A11[A3],A3 ; B A3

    - the index is `load[0]`, and **the handler is called with `load + 1`**. So `none` finds
    its length at `load + 4` and its bytes at `load + 8`, and an rle stream's escape byte is
    `load[1]`.

    **With that, both oracle images decode exactly**, which is the check that the reading is
    right: q05's `00 00 44 33 22 11 ...` is handler 0, escape 0x00, four literals
    `44 33 22 11` - its `.data` word - then escape+0 to end; q18's is handler 0, escape 0x40,
    a run of 64 x 0x5A, 64 literals, then escape+0. A count of zero is the terminator, not a
    long form; the long form is reached another way and neither sample needs it.

    **And lnk6x's escape byte is the smallest value absent from the section's data** - 0x00
    for q05, whose bytes are 44 33 22 11, and 0x40 for q18, whose bytes are 0x5A and 0x00
    through 0x3F. Both samples agree, which makes byte-identity reachable after all: what is
    left to settle is when it emits a run rather than literals (q18 runs 64 identical bytes
    and leaves 64 varied ones alone), and whether it ever chooses `none` - q05 says it does
    not, even for four bytes.
  * **The allocation order with a library is not the SECTIONS order.** q07's addresses come out
    `.stack`, `.text`, `.sysmem`, `.const`, `.c6xabi.extab`, `.fardata`, `.switch`, `.cinit`,
    `.c6xabi.exidx` - the first four in descending size, the rest not - where flat.cmd names
    them `.text`, `.const`, ..., `.stack`, `.sysmem`. What decides it is unread.
  * ~~The input sections inside `.text` are not in input order~~ - **they are placed in
    descending size since 2026-09-22**, which q07's map shows for the whole of its run
    (0x640, 0x580, 0x4C0, 0x440, ...) and which applies to every output section, not just
    `.text`. **A tie goes to the section's own name, ascending, with a bare `.text` after
    every `.text:something`** - and the same for `.fardata`. That was measured rather than
    assumed: over 511 ties in four reference maps, the archive's own order agrees with
    lnk6x 46% of the time (chance) and the module summary's no better, while this rule is
    **257 of 257** on every tie outside `.c6xabi.exidx`. The exidx is excluded because
    lnk6x sorts it by function address instead, which is the entry below this one.
  * **`.c6xabi.exidx` is not sorted.** lnk6x sorts the index by function address and brackets
    it with `__TI_UNWIND_TABLE_START/END`, which this linker defines but does not sort behind.
  * **The attributes blob is still a constant** - see above - and q07's is the merge of five
    distinct blobs the runtime's members carry.
  * **No trampolines.** q15 is the probe: a call 16 MB away gets a 32-byte `$Tramp$S$$name`
    appended to `.text` and listed in the map. This linker writes the call as it stands, and
    q15 differs by 906 bytes.

## Not implemented

Each is a refusal, not a silent wrong answer: the linker says so and stops.

  * PREL31 and EHTYPE - see above. PCR_L16 and PCR_H16 are applied now.
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
