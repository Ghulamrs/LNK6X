; q10 main: calls one of the two members of an archive built here with ar6x. The rts probe (q07)
; answers this for TI's own library, but not in isolation - this one has exactly two members and
; a marker in the unused one, as the PE side's p07 did, so its absence from the image is proof
; rather than inference.
	.global _c_int00, ar_used
	.text
_c_int00:
	CALLP ar_used, B3
	NOP
