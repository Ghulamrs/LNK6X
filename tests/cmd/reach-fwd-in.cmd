/* q27: .fartext at 0xC03FFFE0 - faraway at 0xC03FFFFC, the last word a CALLP at 0xC0000000 reaches */
MEMORY
{
    NEAR : origin = 0xC0000000, length = 0x00100000
    FAR  : origin = 0xC03FFFE0, length = 0x00100000
}
SECTIONS
{
    .text         > NEAR
    .fartext      > FAR
}
