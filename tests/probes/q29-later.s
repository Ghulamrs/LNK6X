; q29: q20 had lnk6x make a trampoline for a call in reach, to a section the command file does
; not name - laid after .text, so with no address when .text was laid. Here the callee's section
; is named (later.cmd), 0x20 bytes against .text's 0x80, so laid after .text by size alone. A
; trampoline here says the rule is the order of allocation (this linker's reading); none says
; it is about sections the file never names.
	.global _c_int00, later
	.text
_c_int00:
	CALLP later, B3
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	B B3
	NOP 5
	.sect ".later"
later:
	MVK 15, A4
	B B3
	NOP 5
