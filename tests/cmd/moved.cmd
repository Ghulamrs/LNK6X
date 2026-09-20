/* moved: flat.cmd with the range somewhere else entirely. Linking the same program from both
   says which terms a relocation depends on: every symbol, every section base and every
   instruction address shifts by the same 0x1234000, so a field that moves with the target and
   one that moves with the reference can be told apart by arithmetic rather than by guesswork.
   This is what q17 is for - PCR_L16 and PCR_H16, which three sites in the runtime could not
   settle (docs/known.md). */
--stack_size=0x4000
--heap_size=0x1000
MEMORY
{
    RAM : origin = 0xC1234000, length = 0x04000000
}
SECTIONS
{
    .text         > RAM
    .const        > RAM
    .data         > RAM
    .bss          > RAM
    .far          > RAM
    .fardata      > RAM
    .neardata     > RAM
    .rodata       > RAM
    .cinit        > RAM
    .init_array   > RAM
    .switch       > RAM
    .cio          > RAM
    .stack        > RAM
    .sysmem       > RAM
    .c6xabi.exidx > RAM
    .c6xabi.extab > RAM
}
