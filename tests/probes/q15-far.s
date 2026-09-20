; q15: a call whose target is 16 MB away - beyond PCR_S21's reach of 4 MB - so that the linker
; has to answer with a trampoline (lnk6x's --trampolines are on by default). far.cmd puts
; .fartext in a range 16 MB above .text. Whether a trampoline appears, where, and what it is
; spelled with is what this records.
	.global _c_int00, faraway
	.text
_c_int00:
	CALLP faraway, B3
	NOP
	B B3
	NOP 5
	.sect ".fartext"
faraway:
	MVK 15, A4
	B B3
	NOP 5
