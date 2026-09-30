; q26: two callers too far from each other to share: .text in NEAR and .midtext in MID, 8 MB
; apart, both calling faraway in FAR, 8 MB above MID. Two trampolines to one callee - what the
; second is called (the same name twice, or a suffix) and where each goes.
	.global _c_int00, faraway, midfunc
	.text
_c_int00:
	CALLP faraway, B3
	CALLP midfunc, B3
	B B3
	NOP 5
	.sect ".midtext"
midfunc:
	CALLP faraway, B3
	B B3
	NOP 5
	.sect ".fartext"
faraway:
	MVK 15, A4
	B B3
	NOP 5
