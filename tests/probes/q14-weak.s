; q14: a weak definition and a strong one of the same name in another object (q14-strong), and
; a weak reference nothing defines. TI's runtime defines __TI_CINIT_Base and friends weak, so a
; linker that cannot tell weak from global cannot link it.
	.global _c_int00
	.weak wk
	.weak undefined_weak
	.text
wk:
	MVK 1, A4
	B B3
	NOP 5
_c_int00:
	CALLP wk, B3
	NOP
	MVKL undefined_weak, A5
	MVKH undefined_weak, A5
	B B3
	NOP 5
