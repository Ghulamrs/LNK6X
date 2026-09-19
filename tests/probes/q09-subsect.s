; q09: subsections - .text:name and .data:name, TI's spelling of what MS COFF spells .text$mn.
; The PE side turned out not to merge those in the order their names suggest (.idata$5 is lifted
; to the front of .rdata), so the same question has to be put here rather than assumed: into
; which output section does a subsection go, in what order among its siblings, and does naming
; one of them in the command file pull it out of that order.
	.global _c_int00
	.text
_c_int00:
	NOP
	B B3
	NOP 5
	.sect ".text:late"
	.word 0x1A1A1A1A
	.sect ".text:early"
	.word 0x1B1B1B1B
	.sect ".data:zzz"
	.word 0x2A2A2A2A
	.sect ".data:aaa"
	.word 0x2B2B2B2B
