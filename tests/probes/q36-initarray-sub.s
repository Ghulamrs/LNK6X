; q36: an .init_array and a subsection of it, .init_array:late, in one object, --rom_model
; with the runtime: whether lnk6x folds the subsection into .init_array, where it puts it,
; and whether __TI_INITARRAY_Base and _Limit bracket both.
	.global main, init_main, init_late
	.text
init_main:
	BNOP	B3, 5
init_late:
	BNOP	B3, 5
main:
	ZERO	A4
	BNOP	B3, 5
	.sect	".init_array"
	.align	4
	.word	init_main
	.sect	".init_array:late"
	.align	4
	.word	init_late
