; q35: two objects each with an .init_array - one entry here, two in q35-initarray-b - linked
; a then b, --rom_model with the runtime: the order lnk6x gives the entries between
; __TI_INITARRAY_Base and _Limit, which is the order the constructors run in.
	.global main, init_a
	.text
init_a:
	BNOP	B3, 5
main:
	ZERO	A4
	BNOP	B3, 5
	.sect	".init_array"
	.align	4
	.word	init_a
