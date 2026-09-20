/* far: two ranges 16 MB apart, so that a call from one to the other is out of PCR_S21's reach
   and the linker has to insert a trampoline (or refuse). */
--stack_size=0x4000
--heap_size=0x1000
MEMORY
{
    NEAR : origin = 0xC0000000, length = 0x00100000
    FAR  : origin = 0xC1000000, length = 0x00100000
}
SECTIONS
{
    .text         > NEAR
    .fartext      > FAR
    .const        > NEAR
    .data         > NEAR
    .bss          > NEAR
    .far          > NEAR
    .fardata      > NEAR
    .neardata     > NEAR
    .rodata       > NEAR
    .cinit        > NEAR
    .init_array   > NEAR
    .switch       > NEAR
    .cio          > NEAR
    .stack        > NEAR
    .sysmem       > NEAR
    .c6xabi.exidx > NEAR
    .c6xabi.extab > NEAR
}
