; q11: a branch that is not the first instruction of its fetch packet. Every branch in the bed
; so far sat at offset 0 of a 32-byte packet, so the bed could not tell "relative to the packet"
; from "relative to the instruction"; here the CALLP sits at offset 8 and the two readings give
; PCR_S21 fields that differ by 2.
	.global _c_int00, helper
	.text
_c_int00:
	NOP
	NOP
	CALLP helper, B3
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	CALLP helper, B3
	B B3
	NOP 5
helper:
	B B3
	NOP 5
