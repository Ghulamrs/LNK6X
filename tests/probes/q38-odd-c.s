; q38: two more contributions, word- and halfword-aligned, to leave holes behind the odd one.
	.global wc, hc
	.sect ".const"
	.align 4
wc:	.word 0x11223344, 0x55667788
	.sect ".const:h"
	.align 2
hc:	.half 0x99AA
	.byte 0xBB
