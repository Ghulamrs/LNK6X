; q13: bytes in .const, .rodata and .switch, all reached. Every image so far had these empty,
; so their flags (0 for .const, 2 for .rodata) were read off empty sections and might be the
; flags of an empty section rather than of the section.
	.global _c_int00
	.sect ".const"
k	.word 0x11111111
	.sect ".rodata"
r	.word 0x22222222
	.sect ".switch"
s	.word 0x33333333
	.text
_c_int00:
	MVKL k, A0
	MVKH k, A0
	MVKL r, A1
	MVKH r, A1
	MVKL s, A2
	MVKH s, A2
	B B3
	NOP 5
