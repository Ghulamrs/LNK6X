; q33: a .bss of 0x20000 bytes, past what a 16-bit run length holds: whether 8.2.2's zero
; image takes the rle stream's 24-bit form (docs/known.md has the decompressor's reading of
; it), and what 7.4.4 writes for the same.
	.global main
	.bss	big, 0x20000, 8
	.text
main:
	MVKL	big, A0
	MVKH	big, A0
	LDW	*A0, A4
	BNOP	B3, 5
