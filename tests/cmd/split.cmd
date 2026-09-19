/* split: the C6747 as it actually is - fast on-chip L2 against slow external DDR - with one
   alignment and one fill asked for, so three things can be read at once: where the linker puts
   what, whether it honours the alignment by padding the section or by moving it, and what it
   writes into the gaps a fill names. */
--stack_size=0x4000
--heap_size=0x1000
MEMORY
{
    L2RAM : origin = 0x11800000, length = 0x00040000
    DDR   : origin = 0xC0000000, length = 0x04000000
}
SECTIONS
{
    .text         : load = L2RAM, align = 32
    .const        > L2RAM
    .neardata     > L2RAM
    .bss          > L2RAM
    .stack        > L2RAM
    .switch       > L2RAM
    .cinit        > L2RAM
    .init_array   > L2RAM
    .data         > DDR
    .fardata      > DDR
    .far          > DDR
    .rodata       > DDR
    .sysmem       > DDR
    .cio          > DDR
    .c6xabi.exidx > DDR
    .c6xabi.extab > DDR
    .fast         : load = DDR, fill = 0xDEADBEEF
    .slow         > DDR
}
