; q08: a section meant to be loaded in one place and run in another - TI's trick, which the PE
; linker has no equivalent of. The load address and the run address part company in the program
; headers, and the copy the startup code must do is described by the tables the linker builds.
	.global _c_int00
	.text
_c_int00:
	NOP
	B B3
	NOP 5
	.sect ".fast"
	.word 0xF0F0F0F0
	.word 0x0F0F0F0F
