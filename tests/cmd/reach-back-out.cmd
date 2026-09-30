/* q27: .text at 0xC0400000, .fartext at 0xBFFFFFE0 - faraway one word short of the reach */
MEMORY
{
    FAR  : origin = 0xBFFFFFE0, length = 0x00100000
    NEAR : origin = 0xC0400000, length = 0x00100000
}
SECTIONS
{
    .text         > NEAR
    .fartext      > FAR
}
