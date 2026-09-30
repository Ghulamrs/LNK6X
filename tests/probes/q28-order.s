; q28: the order of two trampolines in one section. .text calls faraway at 0x0 and 0x8 and
; faraway2 at 0x4. By a callee's highest call (faraway's 0x8) its trampoline comes first; by its
; lowest (faraway2's 0x4 against faraway's 0x0) faraway2's does. q23, q26 and q27 fit both;
; this linker takes the highest.
	.global _c_int00, faraway, faraway2
	.text
_c_int00:
	CALLP faraway, B3
	CALLP faraway2, B3
	CALLP faraway, B3
	B B3
	NOP 5
	.sect ".fartext"
faraway:
	MVK 15, A4
	B B3
	NOP 5
faraway2:
	MVK 16, A4
	B B3
	NOP 5
