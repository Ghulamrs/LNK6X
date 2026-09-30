/* q27: .fartext at 0xC03F0000 - 64 kB inside the reach */
MEMORY
{
    NEAR : origin = 0xC0000000, length = 0x00100000
    FAR  : origin = 0xC03F0000, length = 0x00100000
}
SECTIONS
{
    .text         > NEAR
    .fartext      > FAR
}
