/* flat: one memory range and no opinions beyond it, so what lnk6x decides on its own - the
   order sections land in, the alignment it keeps, the symbols it invents, what it does with a
   section this file never names - is not hidden by anything said here.
   The model (--ram_model or --rom_model) is passed on the command line, not set here, so the
   same file can be used for both halves of the q05 pair. */
--stack_size=0x4000
--heap_size=0x1000
MEMORY
{
    RAM : origin = 0xC0000000, length = 0x04000000
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
