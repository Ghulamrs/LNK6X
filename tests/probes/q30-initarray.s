; q30: one constructor in .init_array, linked with TI's runtime under --rom_model. _c_int00
; walks __TI_INITARRAY_Base to _Limit before main, so lnk6x keeps the section, loads it as it
; is rather than through .cinit, and defines the two names at its ends - where q07, with no
; .init_array, leaves them weak undefined. A C++ static constructor is this shape (cbs, 03-10).
; No variable on purpose: a .bss here brings in __TI_zero_init, a question of its own.
	.global main, init
	.text
init:
	BNOP	B3, 5
main:
	ZERO	A4
	BNOP	B3, 5
	.sect	".init_array"
	.align	4
	.word	init
