/* runload: .fast is loaded into flash and run from L2 RAM. The load address and the run address
   part company, which the program headers must show and which the startup code must act on -
   and which the Windows linker has no equivalent of at all. */
--stack_size=0x4000
--heap_size=0x1000
MEMORY
{
    FLASH : origin = 0x60000000, length = 0x00200000
    L2RAM : origin = 0x11800000, length = 0x00040000
    DDR   : origin = 0xC0000000, length = 0x04000000
}
SECTIONS
{
    .fast         : load = FLASH, run = L2RAM
    .text         > L2RAM
    .const        > L2RAM
    .neardata     > L2RAM
    .bss          > L2RAM
    .stack        > L2RAM
    .data         > DDR
    .far          > DDR
    .cinit        > FLASH
    .init_array   > DDR
    .switch       > DDR
    .sysmem       > DDR
    .cio          > DDR
    .c6xabi.exidx > DDR
    .c6xabi.extab > DDR
}
