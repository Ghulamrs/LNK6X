; q35: a third object with three entries, named to sort before the other two, for the
; reversed link (q35-initarray-rev: b, a, 0) that tells input order from a sort.
	.global init_01, init_02, init_03
	.text
init_01:
	BNOP	B3, 5
init_02:
	BNOP	B3, 5
init_03:
	BNOP	B3, 5
	.sect	".init_array"
	.align	4
	.word	init_01, init_02, init_03
