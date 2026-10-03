; q35: the second object, two entries (see q35-initarray-a).
	.global init_b1, init_b2
	.text
init_b1:
	BNOP	B3, 5
init_b2:
	BNOP	B3, 5
	.sect	".init_array"
	.align	4
	.word	init_b1, init_b2
