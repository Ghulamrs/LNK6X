/* order: the six sections q19 fills, named in an order that is neither their size order nor
   the one flat.cmd uses, so that the addresses lnk6x gives them say which of the three it
   followed. One range, no alignment asked for, nothing else to hide behind. */
--stack_size=0x4000
--heap_size=0x1000
MEMORY
{
    RAM : origin = 0xC0000000, length = 0x04000000
}
SECTIONS
{
    .rodata       > RAM
    .switch       > RAM
    .text         > RAM
    .neardata     > RAM
    .const        > RAM
    .fardata      > RAM
    .data         > RAM
    .bss          > RAM
    .far          > RAM
    .cinit        > RAM
    .init_array   > RAM
    .cio          > RAM
    .stack        > RAM
    .sysmem       > RAM
    .c6xabi.exidx > RAM
    .c6xabi.extab > RAM
}
