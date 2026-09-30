; q20: two sections lnk6x has no standard name for, both in reach and in the one range of
; flat.cmd: `.mycode`, a function called from .text, and `.myconst`, read-only data. q15's
; .fartext came out of lnk6x with flags 7 - writable, though it holds only code - and this asks
; whether that is the name (both come out writable here) or something the far call did (neither).
	.global _c_int00
	.sect ".myconst"
k:	.word 0x11223344
	.sect ".mycode"
near:
	MVK 15, A4
	B B3
	NOP 5
	.text
_c_int00:
	MVKL k, A0
	MVKH k, A0
	CALLP near, B3
	B B3
	NOP 5
