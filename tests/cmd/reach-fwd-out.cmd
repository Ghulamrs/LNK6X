/* q27: .fartext at 0xC0400000 - one fetch packet past the reach */
MEMORY
{
    NEAR : origin = 0xC0000000, length = 0x00100000
    FAR  : origin = 0xC0400000, length = 0x00100000
}
SECTIONS
{
    .text         > NEAR
    .fartext      > FAR
}
