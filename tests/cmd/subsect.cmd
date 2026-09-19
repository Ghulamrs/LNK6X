/* subsect: one subsection named on its own, the rest left to the wildcard. If lnk6x honours the
   naming the way link.exe lifts .idata$5 to the front of .rdata, .text:early lands before the
   others whatever its place in the source; if it does not, the source order or the alphabet
   decides. The answer is the merge rule this linker has to implement. */
--stack_size=0x4000
--heap_size=0x1000
MEMORY
{
    RAM : origin = 0xC0000000, length = 0x04000000
}
SECTIONS
{
    .text :
    {
        *(.text:early)
        *(.text)
        *(.text:*)
    } > RAM
    .data         > RAM
    .const        > RAM
    .bss          > RAM
    .far          > RAM
    .neardata     > RAM
    .cinit        > RAM
    .stack        > RAM
    .sysmem       > RAM
    .switch       > RAM
    .cio          > RAM
    .init_array   > RAM
    .c6xabi.exidx > RAM
    .c6xabi.extab > RAM
}
