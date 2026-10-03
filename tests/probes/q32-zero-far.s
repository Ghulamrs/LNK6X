; q32: a .far of 0x148 bytes - the runtime's own .far is that size in every kernel - reached,
; --rom_model with the runtime: the zero-fill piece for a .far, both linkers.
	.global main
w	.usect	".far", 0x148, 8
	.text
main:
	MVKL	w, A0
	MVKH	w, A0
	LDW	*A0, A4
	BNOP	B3, 5
