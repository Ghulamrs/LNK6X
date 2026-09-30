; q24: two objects, each calling faraway (defined in the second). One trampoline between them or
; one each, and where each trampoline's symbol goes in the image's symbol table - after its own
; object's locals, as q15's did, or somewhere else. q24-two-b.s is the other half.
	.global _c_int00, faraway, bfunc
	.text
_c_int00:
	CALLP faraway, B3
	CALLP bfunc, B3
	B B3
	NOP 5
