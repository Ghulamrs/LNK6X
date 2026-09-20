; q18: initialised data with two runs of very different shape, linked --rom_model with the
; runtime, so that the cinit table lnk6x builds can be read rather than described. The first
; run is sixty-four identical words - what a run-length coder is for - and the second is
; sixty-four words that never repeat, which it cannot help. Between them the compressed load
; image, the handler index in front of it, the handler table and the cinit records all become
; readable, and __TI_CINIT_Base/Limit and __TI_Handler_Table_Base/Limit have something real to
; bracket. Nothing in the bed has any of this today (the review's N9).
	.global main, same, varied
	.sect ".data"
same:	.word 0x5A5A5A5A, 0x5A5A5A5A, 0x5A5A5A5A, 0x5A5A5A5A
	.word 0x5A5A5A5A, 0x5A5A5A5A, 0x5A5A5A5A, 0x5A5A5A5A
	.word 0x5A5A5A5A, 0x5A5A5A5A, 0x5A5A5A5A, 0x5A5A5A5A
	.word 0x5A5A5A5A, 0x5A5A5A5A, 0x5A5A5A5A, 0x5A5A5A5A
varied:	.word 0x00010203, 0x04050607, 0x08090A0B, 0x0C0D0E0F
	.word 0x10111213, 0x14151617, 0x18191A1B, 0x1C1D1E1F
	.word 0x20212223, 0x24252627, 0x28292A2B, 0x2C2D2E2F
	.word 0x30313233, 0x34353637, 0x38393A3B, 0x3C3D3E3F
	.text
main:
	MVKL same, A0
	MVKH same, A0
	MVKL varied, A1
	MVKH varied, A1
	B B3
	NOP 5
