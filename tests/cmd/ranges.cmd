/* q26: three ranges 8 MB apart - .text calls .fartext, and so does .midtext, and neither reaches the other */
MEMORY
{
    NEAR : origin = 0xC0000000, length = 0x00100000
    MID  : origin = 0xC0800000, length = 0x00100000
    FAR  : origin = 0xC1000000, length = 0x00100000
}
SECTIONS
{
    .text         > NEAR
    .midtext      > MID
    .fartext      > FAR
}
