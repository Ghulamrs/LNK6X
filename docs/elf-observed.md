# What lnk6x does

Read off the probe bed, not out of the manual. Every number here came from the images in
`tests/ref` and the `.map` files beside them. Tools: CCS 7.4, C6000 CGT 8.2.2 (`cl6x`, `lnk6x`,
`ofd6x`) on the Windows box, 2026-09-20, `-mv6740 --abi=eabi`.

The output is an ELF32 executable, little-endian, `EM_TI_C6000` (140), `e_flags` zero. There is
no time stamp and no build path anywhere in it - the only strings are the objects' own - so
unlike a PE image it can be compared byte for byte without pinning anything first.

## The thing that shapes everything else: unused sections go

lnk6x eliminates unreferenced sections by default for EABI, and the bed says so plainly.
q02-place.obj holds `.fast`, `.slow` and `.unnamed_by_the_cmd`, four bytes each, written with
`.sect`. In `q02-place-flat.out` and `q02-place-split.out` none of their bytes appear, the map
does not list them, the MODULE SUMMARY counts them as zero, and `.fast` - which `split.cmd`
names explicitly - is present in the section table at length zero. q06 is the same story one
section at a time: `.neardata` and `.far` are reached from `.text` through MVKL/MVKH and
survive with their data; `.const`, holding `k`, is reached by nothing and is gone, and so is
its local symbol.

So the rule is: start at the section that defines the entry point, follow every relocation to
the section its symbol lands in, and keep what that reaches. Anything else is not in the image.

## The output sections

The list is the `SECTIONS` block in its own order, and then whichever of the sixteen standard
sections the command file did not name, in *their* order:

    .text .const .data .bss .far .fardata .neardata .rodata
    .cinit .init_array .switch .cio .stack .sysmem .c6xabi.exidx .c6xabi.extab

`runload.cmd` names fifteen sections and leaves out `.fardata` and `.rodata`; q08's section
table has them appended after `.c6xabi.extab`, in that order, which is what settles it. Then
come the linker's own five, always last and always in this order: `.c6xabi.attributes`,
`.symtab`, `.TI.section.flags`, `.strtab`, `.shstrtab`.

### Flags, type and alignment

An output section's `sh_flags` does not come from its input. It comes from the name:

| section | flags | | section | flags |
|---|---|---|---|---|
| `.text` | 6 | | `.rodata` | 2 |
| `.const` | 0 | | `.cinit` | 2 |
| `.data` | 3 | | `.init_array` | 3 |
| `.bss` | 3 | | `.switch` | 0 |
| `.far` | 3 | | `.cio` | 0 |
| `.fardata` | 3 | | `.stack` | 0 |
| `.neardata` | 3 | | `.sysmem` | 0 |
| `.c6xabi.exidx` | 0x82, `sh_entsize` 8 | | `.c6xabi.extab` | 2 |

A name not on that list gets zero - `.fast` and `.slow` do, in the two command files that name
them. `.const` getting 0 while `.rodata` gets 2 is not a mistake in the reading; both are empty
in every image the bed produced, and no probe has yet put bytes in either.

`sh_type` is `SHT_PROGBITS` when the section has bytes, `SHT_NOBITS` when it has none - and a
`fill = ` in the command file counts as having them, which is why `.fast` is PROGBITS in
`split.cmd` and NOBITS in `runload.cmd`. `sh_addralign` is the largest alignment among the
input sections that joined it, or 1 when none did.

### Addresses

An empty section that is NOBITS has address zero. An empty section that is PROGBITS gets the
**origin** of its memory range, not the range's high-water mark: in q06, `.far` has taken
`0xC0000000..0xC0000003` out of DDR by the time the empty `.fast` is placed, and `.fast` is
still given `0xC0000000`.

## Allocation

Sections are cut from the memory range their `SECTIONS` entry names, from the origin upwards,
each aligned to its own alignment and to any `align =` the entry asks for. The order is the
`SECTIONS` order **with one exception: `.bss` is allocated first.** q03 is the proof -
`flat.cmd` lists `.text` first and `.bss` fourth, and the image has `.bss` at `0xC0000000`,
`.text` at `0xC0000100` and `.far` at `0xC0000120`. The reason is visible in the same image:
`__TI_STATIC_BASE` is the address of `.bss`, so the near region has to begin where the range
does. q06, which has no `.bss` at all, allocates strictly in `SECTIONS` order and sets the
static base to zero.

## File offsets

Not in section-table order - in **address** order, over the allocated sections only:

    pos = 52                                  (the ELF header)
    for each allocated section with an address, lowest address first:
        pos = align(pos, sh_addralign);  sh_offset = pos
        if PROGBITS: pos += size          (NOBITS takes an offset and no room)
    every allocated section with no address:  sh_offset = pos
    every section that is not allocated:      sh_offset = 0

q03 shows all three cases at once: `.bss` takes offset 0x38 - below `.text`'s 0x40, because its
address is lower and its alignment is 8 - `.text` 0x40, `.far` 0x60, every other allocated
section 0x60, and `.const`, `.switch`, `.cio`, `.stack`, `.sysmem` zero.

After them: `.c6xabi.attributes` at the running position, `.symtab` rounded up to 4,
`.TI.section.flags`, `.strtab` and `.shstrtab` each straight after the last, the program header
table rounded up to 4, and the section header table straight after that. The file ends there,
which is how q01 comes to be exactly 1980 bytes.

## Segments

One `PT_LOAD` per run of allocated sections that have length, taken in address order and merged
while two conditions hold: the next section starts exactly where the last one ended, and the
union of their attributes does not contain both write and execute. The attributes are the
*input* sections' - read always, write from `SHF_WRITE`, execute from `SHF_EXECINSTR` - and not
the output section's flags from the table above.

q06 merges `.text` (r-x) and `.neardata` (r--, because the input section is ALLOC and not
WRITE) into one r-x segment of 0x24 bytes. q04 does not merge `.text` (r-x) with `.data`
(rw-, the input really is writable) even though they are contiguous. q03 gets three segments
for three sections for the same reason.

## The symbol table

In this order, and `sh_info` is the index of the first global:

1. one `STT_SECTION` symbol per output section, in section order, `st_value` its address,
   `st_other` 0;
2. per object that has anything left in the image, in command-line order: its `STT_FILE`
   symbol - which carries the name of the **source file**, `q01-bare.s`, not of the object -
   then its surviving local symbols in the object's own order, with `st_other` 2;
3. the six names lnk6x defines whether or not anything wants them, all `SHN_ABS` and all
   `0xFFFFFFFF`: `binit`, `__binit__`, `__c_args__`, `__TI_pprof_out_hndl`,
   `__TI_prof_data_start`, `__TI_prof_data_size`;
4. the globals the objects defined, in the order they were defined;
5. `__TI_STATIC_BASE`, whose value is the address of `.bss` and whose `st_shndx` is `.bss`.

`.strtab` holds those names in that order with duplicates shared.

## Relocation

Three types carry the whole bed, and each was checked word by word against the images:

    ABS_L16 (9)   MVKL: value & 0xFFFF     into bits 7..22
    ABS_H16 (10)  MVKH: value >> 16        into bits 7..22, with no rounding
    PCR_S21 (4)   branch: (target - fetch packet) >> 2 into bits 7..27

q03: `MVKH buf` turns `0x00000068` into `0x00600068`, and 0xC000 << 7 is 0x600000.
q04: `CALLP helper, B3` turns `0x10000012` into `0x10000412`, and (0xC0000020 - 0xC0000000) >> 2
is 8, and 8 << 7 is 0x400. Both objects spell their relocations differently - q04's is a
`SHT_REL` with the addend in the field, q06's a `SHT_RELA` - and a reader has to take either.

## Two blobs carried rather than composed

`.c6xabi.attributes` (57 bytes, with the string `Linker` in it where the object says `cl6x`)
and `.TI.section.flags` (26 bytes, a 1 and then zeros) are byte-identical in all eight images
that link no runtime library. They differ only in `q05-model-rom` and `q07-lib`, which do. This
linker writes them as constants and `docs/known.md` records that as a constant standing in for
a rule nobody has read yet.
