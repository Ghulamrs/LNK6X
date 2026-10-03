; q31: one .bss word, reached, linked --rom_model with the runtime. What lnk6x writes to zero
; it: 7.4.4 pulls __TI_zero_init and writes an 8-byte record, 8.2.2's kernels (ti74 maps)
; show an rle image of zeros instead - this probe reads the bytes of both. q30's first
; version had this word and lnk6x 8.2.2 pulled no handler for it (docs/known.md, 03-10).
	.global main
	.bss	v, 4, 4
	.text
main:
	MVKL	v, A0
	MVKH	v, A0
	LDW	*A0, A4
	BNOP	B3, 5
