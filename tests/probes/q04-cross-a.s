; q04 a: calls the other object. The first case where the linker must choose an order and then
; resolve a branch across it - the PCR_S21 that asm6x left for it.
	.global _c_int00, helper
	.text
_c_int00:
	CALLP helper, B3
	NOP
