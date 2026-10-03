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

**`.bss` is not allocated first; it is allocated by size like the rest.** This linker said the
opposite until 2026-09-29, on q03's evidence - and q03's `.bss` is 0x100 against a 0x20 `.text`,
q12's 0x40 against 0x20, so the size rule alone lays both first. The kernels settled it: sieve's
0x4E21 `.bss` follows its 0x9180 `.text` in lnk6x 7.4.4's image, and laid first here it moved
every section and the entry point with it.

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

    **The layout, off q05-model-rom and q18-cinit, corrected 2026-09-30 off 7.4.4's
    isort.** `.cinit` holds the load images, the handler table (4-aligned, one 32-bit pointer
    per decompressor, which `__TI_Handler_Table_Base/Limit` bracket) and the zero-fill
    records (4-aligned), **all in descending size together**, then the record table, two
    words each `{load, run}`, which `__TI_CINIT_Base/Limit` bracket, in the order the images
    and zero records were laid. q05 and q18 never told this from "images first", since
    their handler table is the smallest piece; isort's `.neardata` image (0xA) is smaller than
    its handler table (0xC), and 7.4.4 lays the table between the two images - which is why
    this linker's isort `.cinit` was 0xB8 against 0xBC and everything after it moved by 4. The
    decoded images were identical before and after; only the addresses moved. Every 7.4.4
    kernel map, the harness (0x412, 0x25, 0x21, 0xC, 8) and 8.2.2's isort (0x37, 0xB, 0xB,
    0xA, 8) read the same way. The record table and `.cinit` are 8-aligned by 8.2.2 (q05,
    q18) and 4-aligned by 7.4.4 (isort's table at +0x9C): the default is 8.2.2, the bed's
    oracle, and `--cgt=7.4.4` asks for 7.4.4's. The initialised section
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
  * ~~The allocation order with a library is not the SECTIONS order~~ - **read and
    implemented 2026-09-22.** It is **descending size**, with `.bss` first (q03:
    `__TI_STATIC_BASE` points at it), `.cinit` and `.c6xabi.exidx` held to the end whatever
    their size, and a section the file never names after all of those (q12's `.mybss`). A
    tie goes to the name, ascending: q19 has `.const` and `.text` both 0x40 and lnk6x puts
    `.const` first - and it cannot be the file's order, because q19 is linked twice from
    command files that name the six sections differently and lnk6x lays them out
    identically both times. Why those two are held back is not read, but both describe the
    rest of the image - the load images carry run addresses, the index is sorted by
    function address - so placing them among the others would decide their contents from
    their own position. q19 went from 926 and 114 bytes differing to **5**, and to none on 2026-09-27: a segment
    starts at the largest alignment of the sections it holds, so its .neardata, aligned 1,
    sits at 0x40 because the .text behind it is aligned 32.
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
  * **Out-of-range fields are refused, not truncated (2026-09-30, the review's L-A5) - and a far
    PCR_S21 branch gets lnk6x's trampoline instead (2026-09-30).** Every bounded relocation is
    checked before its field is written, by the C6000 ELF ABI's rule for it: PCR_S21, S12, S10
    and S7 are signed word counts from the fetch packet, so the byte distance must be a multiple
    of four and fit 21, 12, 10 or 7 bits; ABS_S16 (MVK) is signed 16; ABS16 and ABS8 are data and
    take a signed or an unsigned value of their width. The L16/H16 halves, ABS32, EHTYPE and
    PREL31 cannot overflow. Until then `field()` masked, and q15 linked rc 0 with an empty log
    and a branch to somewhere else. Every one still out of range is named before the link stops,
    exit 1 and no image - bad.sh holds a PCR_S12 16 MB long, made from q15's object by one byte.

    **A PCR_S21 (B, CALLP) that cannot reach its target goes through a trampoline**, and q15's
    image is byte-identical to lnk6x's (8.2.2). What was read off q15-far.out and .map and is
    implemented: a 32-byte input section `$Tramp$S$$faraway` in the caller's own module, laid
    at the end of the caller's output section (0xC0000020, `.text` grows 0x20 to 0x40), the branch's PCR_S21 retargeted to it (field 8 words); its symbol a local
    STT_FUNC with `other` 0, value its address, after its module's own locals (symbol 21 of
    q15, straight after .fartext's section symbol); and the map's `FAR CALL TRAMPOLINES` table,
    the callee named by its input section and offset (`$.fartext:q15-far.obj$0x0`). Its eight
    words (`tramp_code` in reloc.cpp):

        053c54f7   STW   .D2T2  B10,*B15--[2]       ; p-bit set: in parallel with
        0500002a   MVKL  .S2    faraway,B10         ; 0x0000 - the low half of the callee
        0560806a   MVKH  .S2    faraway,B10         ; 0xC100 - the high half
        00280362   B     .S2    B10
        053c52e6   LDW   .D2T2  *++B15[2],B10       ; B10 restored in the branch's delay slots
        00006000   NOP   4
        00000000   NOP
        00000000   NOP

    B3 is the CALLP's own return address and is not touched, so a plain `B` works the same way.
    **The layout runs to a fixed point** (`Link::layout`): allocate, find the calls that want a
    trampoline and reach none to their callee, make one per callee per pass, allocate again - a
    trampoline moves what is behind it, and the next call may reach the one just made. None is
    removed, and a caller whose own is still out of reach (an input section past 4 MB) is
    refused by name rather than given another, so it ends.

    **What probes q20-q27 settled (lnk6x 8.2.2 and 7.4.4, run 2026-09-30), all implemented:**

      * **Placement (q21, q24)**: at the *end* of the caller's output section, after every input
        section of it - q21's follows .text:small, not the .text:caller that made it, and q24's
        follows the second object's .text. Never in a gap. It belongs to the module of the
        section it follows in the map (q24: q24-two-b.obj) - or of the highest call; q24 cannot
        tell the two apart.
      * **Two forms, one size (q20, q27)**: where the trampoline itself reaches the callee it is
        `B callee; NOP 5` and six zero words (00000010 with the PCR_S21 field, 00008000); the
        B10 form only where it does not. Both are 32 bytes.
      * **Sharing (q23, q24)**: one per callee *place* - an input section and an offset - for
        every call in reach of it, across sections and objects; faraway and faralias at one
        address share one. Two only where a caller cannot reach the first (q26).
      * **Naming (q23, q25, q26)**: `$Tramp$S$$` and the symbol of the lowest-addressed call to
        that place - q23's is `$Tramp$S$$faralias`, the name only its call at 0x0 used; a local
        label's name (q25, whose relocation does name the label); the same name twice in q26.
      * **Order (q23, q26, q27, q28)**: several in one section go in descending order of each
        callee's *lowest* call - q28's faraway2 (called at 0x4) before faraway (0x0 and 0x8).
      * **Reach (q27)**: the field's exact reach, no margin - nothing at 0xC03F0000 or on the last
        word either way, one each a word or a fetch packet past it.
      * **When a call gets one (q20)**: also when the callee's output section is laid *after*
        the caller's - q20's .text calls .mycode, which no file names and so is laid last, and
        lnk6x makes `$Tramp$S$$near` (the short form) for a callee 0x38 bytes away - yet the
        CALLP itself goes straight to `near`: only a branch out of reach is sent through one.
        q29 settled it as the order of allocation: a callee in a *named* section laid after
        .text by size gets one too.

    **And four things the probes showed that are not about trampolines**, implemented:

      * a section whose name begins `.far` is writable, code or not (q15 and q26's .fartext are
        7; q26's .midtext and q20's .mycode are 6, q20's .myconst 2);
      * of the standard sections the command file does not name, only .fardata (beside a named
        .far) and .rodata (beside a named .const) are added - q26 and q27 name only code and get
        nothing else, so no .bss and __TI_STATIC_BASE absolute; and **7.4.4 adds an empty
        .cinit** as well. Every kernel command file names all sixteen, so nothing moves there;
      * sections the file never names are laid by size among themselves (q20's .mycode 0x20
        before .myconst 4), after every named one;
      * a map's trampoline table writes the callee and trampoline addresses on a callee's first
        call line only (q23).

    **Two differences the probes found, mended 2026-09-30 after measuring them against the 20
    kernel images lnk6x 7.4.4 made (~/c6747-review-2026-09-29/ti55)** - every loaded section
    identical to TI's before and after, and the symbol table nearer:

      * **A section symbol keeps its input section's name** - `.text:big`, `.text:_c_int00` -
        where this linker wrote the output section's `.text`. q21 and q23 match with it. In the
        kernels the symbols of ours not in TI's image fall from ~205 to 2 or 3 each, and those
        of TI's not in ours from ~1,660 to ~1,450 (the rest are DWARF and the index's). Its
        .strtab grows by ~6 kB, all names TI's image also has, so the bytes cmp counts in the
        runtime probes rose: tests/known-differ.txt says so.
      * **Under --cgt=7.4.4, an empty section is written without SHF_ALLOC** (.data 1, .rodata
        and .cinit 0, .c6xabi.exidx 0x80, and so at offset 0), **and the linker's attributes
        are 7.4.4's** (`08 08 0a 05 0c 05` for 8.2.2's `08 09 0a 03 0c 03`). Every 7.4.4 probe
        matches with it, and no kernel image has a section whose flags differ from TI's any more
        (3 to 5 each before). The kernels' attributes blob still differs - TI's is the merge of
        the runtime objects' own, which this linker does not compose.

## Not implemented

Each is a refusal, not a silent wrong answer: the linker says so and stops.

  * PREL31 and EHTYPE - see above. PCR_L16 and PCR_H16 are applied now.
  * A trampoline for anything but PCR_S21, and one for a caller whose own trampoline would be
    out of its reach (an input section past 4 MB): both are refused by name (above).
  * **A REL ABS16 or ABS8 keeps its addend in place, and it is not read.** `elf.cpp` reads the
    in-place addend of ABS32, EHTYPE and PREL31 only, so `.half sym+4` in a REL table would
    be written as `sym`. Nothing in the bed or the runtime has one; noticed while adding the
    range checks (2026-09-30), and left until a probe shows lnk6x's reading of the field.
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

## A real program against TI's runtime, 2026-09-28

Measured from C++Optimize (`tools/c6747-three`): cpp11's `hello` object, assembled by ASM6x and
linked against `rts6740_elf_eh.lib` (7.4.4's mklib) with TI's own `C6747.cmd`. **It runs now**
on TI's C6747 cycle-accurate simulator - `Hello World!`, 26,242 cycles, where lnk6x's image of
the same object takes 26,122 - and q07's image went from 9 sections matching the oracle's to 15
of 16. What it took, each read off the oracle's map or bytes:

  * **The command file and the line.** A MEMORY entry needs no colon and may carry
    `(RWX)`; `-heap`/`-stack` in the file, `--heap_size`/`--stack_size` on the line, and
    lnk6x's 1 kB defaults.
  * **An unwind index entry lives while its code does** (`sh_link` names it) - which is what
    pulls the personality routine and the unwinder in; q07's .text went 3,488 -> 16,320 bytes.
  * **PREL31 counts halfwords on the C6000**: `(S + A - P) >> 1`, where ARM's is bytes.
  * **REL relocations keep their addend in the place**: a type_info's vptr is `vtable + 8`.
    Read at load, because fix_up runs twice under --rom_model.
  * **.sysmem is --heap_size whenever memory.obj brings one**, the reservation a floor.
  * **Holes are filled first-fit** within an output section, lowest gap first.
  * **`__TI_UNWIND_TABLE_START/END`** were compared against 18 characters of a 17-character
    prefix and so pointed at .cinit.
  * **A segment is a run of one kind** - file bytes or none - never mixing write and execute,
    and .c6xabi.exidx (SHT_C6000_UNWIND) is file bytes, PROGBITS in the output: zero-filled
    before, it took .switch and .cinit with it and _c_int00 ran off a blank table.
  * **Zero-fill records**: an uninitialised `.far`/`.bss` gets `__TI_zero_init` at handler
    index 0 (rle24 moves to 1) and an 8-byte record, the handler pulled only when needed - a
    pulled member's symbols reach the image even when its code is eliminated.
  * **`binit`/`__binit__` stay 0xFFFFFFFF**, and the four `.cinit` table names are set before
    fix_up, not after it: `_auto_init_elf` had been patched with .cinit's start for both bases.

**Still different from lnk6x, and why q07 is 15 of 16:** the index has no linker-made
entries - lnk6x adds an EXIDX_CANTUNWIND for kept code with no entry of its own and merges
adjacent ones (42 entries against 37 here). It changes nothing for a program that does not
throw through such code. `tests/known-differ.txt` was re-pinned on 2026-09-29 (below).

## The six kernels against lnk6x 7.4.4, 2026-09-29

The C6747 benchmark kernels of C++Optimize (`tools/c6747-levels`: fib, hash, isort, matmul,
sieve, virt, cpp11 -O1 and -O2, assembled by ASM6x) linked by this linker and by lnk6x 7.4.4
from the same object, `C6747.cmd`, `--heap_size=0x800 --stack_size=0x800` and 7.4.4's
`rts6740_elf_eh.lib`. All twelve link, print their `.expected` on TI's cycle-accurate simulator
and stop at `C$$EXIT`; after the four rules below every output section of every image has the
oracle's address and size, but for isort's `.cinit` and the index behind it, 8 bytes up (below). Each rule was read off the two maps, section by section:

  * **`.TI.symbol.alias` is honoured.** A u32 version 1, a u16 count, `TI\0`, then pairs of
    symbol indices, alias first: remove.obj's is (`remove`, `unlink`), typeinfo_.obj's are
    the C2 and D2 constructors and destructors standing for C1 and D1. lnk6x resolves the
    alias to its target - `remove` and `unlink` share one address in every TI map, and no
    `.text:remove` is laid - so the alias's own section is kept only if something else names
    it. 491 of the runtime's members carry the section; this linker read none of it, laid the
    0x20-byte stub, and every address after it in `.text` and every data section was 0x20 off.
  * **A tie between contributions goes to the name after the colon, and a bare section is
    keyed by its object's name.** fib.obj's `.text` precedes memory.obj's `.text:malloc` at
    0x180 and tdeh_uwentry_c6000.obj's `.text` follows fseek.obj's `.text:fseek` at 0x120 -
    116 of 116 tie groups across six maps of 7.4.4 and 8.2.2 agree. The old reading, "a bare
    `.text` after every subsection", explained the second and not the first.
  * **`.bss` takes its place by size** (above).
  * **A section the linker alone sizes is written WA, 8-aligned** - q07's `.stack`, which this
    linker wrote with flags 0 and alignment 1 while placing it right.

**What still differs, all measured and none of it running code:**

  * two words of `.text`, `_Z16find_et_setup_pr` and `__TI_ut_entry_cmp` loading the unwind
    table's end, which moves with the linker-made EXIDX_CANTUNWIND entries this linker does
    not write - 0x1D0 against 0x170 bytes of index in fib;
  * ~~isort's `.cinit` 4 bytes short~~ - **mended 2026-09-30**: the handler table goes among
    the load images by size (above), and `--cgt=7.4.4` gives `.cinit` and its record table
    7.4.4's 4-alignment. With it, all twenty kernel images of the review (arith, fib, floats,
    hash, hello, isort, matmul, sieve, structs, virt at -O1 and -O2) have every loaded
    section byte-identical to 7.4.4's; without it `.cinit` keeps 8.2.2's alignment of 8;
  * 7.4.4 writes an *empty* `.bss`, `.data`, `.init_array` or `.neardata` with flags 1 (W) and
    an empty `.rodata` with 0, dropping ALLOC; 8.2.2 keeps 3 and 2, as the bed's images show
    and this linker writes;
  * the attributes blob, the symbol table and its strings, as in every runtime probe.

The two kernels' cycle counts against lnk6x's images are in C++Optimize's CLAUDE.md.

## .init_array, mended 2026-10-03

**A C++ program's static constructors never ran.** RIDE's common-back-end study linked one
cpp11 object (`static std::string tag = "shapes";`) with this linker and with lnk6x 7.4.4: on
TI's cycle-accurate simulator lnk6x's image printed `shapes big 42 13.0` and this one printed
` big 42 13.0`. Two rules were missing, and q07 could show neither, having no `.init_array`:

  * **`.init_array` is a root.** Nothing names a constructor's entry but `_c_int00`'s walk from
    `__TI_INITARRAY_Base` to `_Limit`, so elimination dropped every one; lnk6x keeps them.
  * **`__TI_INITARRAY_Base` and `_Limit` are defined at the section's ends** where an object
    brings an `.init_array`, and left weak undefined where none does, as q07 shows lnk6x
    leaving them. And under `--rom_model` `.init_array` stays PROGBITS, loaded as it is rather
    than through `.cinit`, as both lnk6x versions lay it.

With both, the study's C++ image has lnk6x 7.4.4's `.init_array`, both names at 8000da8c and
8000da90, and prints `shapes big 42 13.0` (101,207 cycles against lnk6x's 99,851 - the unwind
index and `.const` differ as they did before); the C image, with no `.init_array`, is
byte-identical to the one before the change. **q30** holds it in the bed against lnk6x 8.2.2:
the same differences as q07 and nothing more, where the linker before this change also
dropped `.init_array` and missed by 9 bytes of `.text`.

**Completed the same day** (begun by Fable 5.1's probes q30-q37, under both linkers):

  * **`.init_array` in what order.** 8.2.2 keeps the entries in the objects' order on the line
    (q35: a, b; reversed with a third, b, a, 0); 7.4.4 lays them by size like any section
    (0, b, a both times). That order is the order the constructors run in.
  * **Only `.init_array` itself is a root**: a `.init_array:late` subsection is dropped by both
    linkers like any section nothing names (q36). Under --ram_model the names are the same (q30-ram).
  * **The zero-fill rule was 7.4.4's alone.** 8.2.2 never pulls `__TI_zero_init`: it zeroes an
    uninitialised `.bss` or `.far` through an rle24 image of zeros (`00 01 01 04 00 01 00 00 00`
    for a word, q31-q33). 7.4.4 pulls the handler with an 8-byte record - the rule hello's
    `.far` showed - and its handler table is in the command file's order of the first section
    wanting each handler (zero_init at 0 when `.far` is named before `.fardata`; rle24, none,
    zero_init when a `.data` comes first). This was the 32 bytes q30's first version met.
  * **Archive members are taken in one pass in archive order**, a member taken the moment the
    walk reaches it with a name still undefined - read off the STT_FILE runs of lnk6x's image.
    It decides which of two equal-sized contributions loads first.
  * **A size tie between objects goes by the object's name** (q37), not by the line's order.
  * **An input index section is one unit**, kept in its own order, units by their code's
    address: cpp11's one `.c6xabi.exidx:.text` per file comes whole, where sorting every
    entry by function moved four of them. The linker-made EXIDX_CANTUNWIND entries already
    matched; the sentence above saying the index has none is out of date.

Measured: the cbs C++ image linked `--cgt=7.4.4` now takes **99,851 cycles on the CCS 5.5
simulator, lnk6x 7.4.4's own count** (101,207 after the first half of this round, a blank
string before it); against CCS 7.4's runtime it runs in 99,571 and the C program in 29,822, the
new zero-fill path included. The six kernels at -O1 and -O2 keep every loaded section as before.
Every new probe matches lnk6x in every loaded section, and - mended the same evening - in the
segment table too: **a segment is a run of sections lnk6x would lay as one**, and the file offsets
are laid out by the same runs. A run is one kind (file bytes or none), never mixes write with
execute or a writable section with a read-only one, takes a stricter alignment only where its
start already has it (q32's `.switch` at 4 mod 8 ends before an 8-aligned `.cinit`), and under
8.2.2 alone crosses a small gap between two sections of the same execute permission (q30's
`.const` and `.extab`, 2 bytes; 7.4.4 makes them two segments). All 78 images of the bed and the
six kernels at -O1 and -O2, linked again by lnk6x 7.4.4 on the box, have lnk6x's segment table.
Still different: 30 bytes of the cbs image's `.const` and the 2 bytes of `.text` that load one
of its strings. **One tie of 774, and its rule is not found.** Fable's tie tester over 89 lnk6x
maps (the bed, the cbs C and C++ images, the twelve kernels linked by lnk6x 7.4.4 on the box)
finds the name after the colon right in every tie group but this one and the trampolines:
typeinfo_.obj's `.const:.string:_ZTSSt10bad_typeid` (COMDAT, aligned 8) before newhandler.obj's
`.const:.string` (aligned 1), both 15 bytes, where the name puts the bare one first. A C++ pair
compiled by cl6x 7.4.4 reproduces it with the runtime. Probes built to isolate it on the box -
the same pair as assembly, the zz piece grouped and not, both pieces in an archive in either
order, the bare string reached only through a local label - are all laid by name by both
linkers, so what decides it is something of those two runtime members not yet seen. Keying a
bare `.string` by its object, or putting a grouped or a more aligned piece first, mends the cbs
image and breaks the C image, the hash kernel or q34, and is not taken. It costs nothing that
runs: the cbs image takes lnk6x 7.4.4's own 99,851 cycles, and a C++ case with static
constructors and destructors (C++Optimize tools/c6747/ctor, through tools/c6747-levels) prints
clang's output on TI's simulator linked by both linkers, in identical cycles.

## Things that are this linker's own

Nothing. There is no equivalent here of LINK's `/timestamp:`, because a TI image carries no
time of day to pin.
