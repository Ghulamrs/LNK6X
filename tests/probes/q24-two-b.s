; q24: the second object - see q24-two-a.s.
	.global faraway, bfunc
	.text
bfunc:
	CALLP faraway, B3
	B B3
	NOP 5
	.sect ".fartext"
faraway:
	MVK 15, A4
	B B3
	NOP 5
