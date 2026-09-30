; q25: a far call to a name that is not global, 0x20 bytes into .fartext. The relocation may name
; the section rather than the label; what the trampoline is called then, and how the map names
; its callee, is the question.
	.global _c_int00
	.text
_c_int00:
	CALLP localfar, B3
	B B3
	NOP 5
	.sect ".fartext"
first:
	B B3
	NOP 5
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
localfar:
	MVK 15, A4
	B B3
	NOP 5
