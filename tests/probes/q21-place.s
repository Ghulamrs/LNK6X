; q21: where a trampoline goes when its caller is one of several contributions to .text. The
; three are of three sizes - .text:big 0x80, .text:caller 0x40 with the far call, .text:small
; 0x20 - so lnk6x lays them big, caller, small by size. The trampoline (0x20) lands straight after
; .text:caller (as q15's does after its .text), among the parts by its size (tying .text:small),
; or at the end of .text; this linker takes the first.
	.global _c_int00, faraway
	.sect ".text:big"
big:
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	B B3
	NOP 5
	.sect ".text:small"
small:
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	B B3
	NOP 5
	.sect ".text:caller"
_c_int00:
	CALLP big, B3
	CALLP small, B3
	CALLP faraway, B3
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	B B3
	NOP 5
	.sect ".fartext"
faraway:
	MVK 15, A4
	B B3
	NOP 5
