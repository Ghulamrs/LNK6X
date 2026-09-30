; q22: a caller that ends part-way through a fetch packet. .text is 9 words, 0x24 bytes, with
; the far call first: does the trampoline follow at 0x24 (aligned 4, its packet crossing into the
; next fetch packet), 0x28 (8), or 0x40 (32, one whole fetch packet)? This linker takes 32.
	.global _c_int00, faraway
	.text
_c_int00:
	CALLP faraway, B3
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	B B3
	NOP 5
	.sect ".fartext"
faraway:
	MVK 15, A4
	B B3
	NOP 5
