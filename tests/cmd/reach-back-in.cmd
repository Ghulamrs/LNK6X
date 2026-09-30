/* q27: .text at 0xC0400000, .fartext at 0xC0000000 - fartop on the lowest word it reaches */
MEMORY
{
    FAR  : origin = 0xC0000000, length = 0x00100000
    NEAR : origin = 0xC0400000, length = 0x00100000
}
SECTIONS
{
    .text         > NEAR
    .fartext      > FAR
}
