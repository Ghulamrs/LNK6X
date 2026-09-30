; q27: the reach itself, linked five times against five command files. fartop is at .fartext+0
; and faraway at +0x1C, called from a CALLP at the start of .text. A PCR_S21 holds
; -0x100000..0xFFFFF words from the caller's fetch packet, so:
;   reach-fwd-in    .fartext at 0xC03FFFE0: both in reach, faraway on the last word of it
;   reach-fwd-out   .fartext at 0xC0400000: both one fetch packet past it
;   reach-margin    .fartext at 0xC03F0000: 64 kB short of it - a linker with a safety margin
;                   would make trampolines here anyway
;   reach-back-in   .text at 0xC0400000, .fartext at 0xC0000000: fartop on the lowest word
;   reach-back-out  .text at 0xC0400000, .fartext at 0xBFFFFFE0: faraway one word short
	.global _c_int00, faraway, fartop
	.text
_c_int00:
	CALLP fartop, B3
	CALLP faraway, B3
	B B3
	NOP 5
	.sect ".fartext"
fartop:
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
faraway:
	MVK 15, A4
	B B3
	NOP 5
