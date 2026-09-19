; q02: sections of its own beside the ordinary ones, linked once flat and once split, so the
; order lnk6x lands them in - and what it does with a section the command file never names -
; can be read off rather than assumed.
	.global _c_int00
	.text
_c_int00:
	NOP
	B B3
	NOP 5
	.sect ".fast"
	.word 0xAAAAAAAA
	.sect ".slow"
	.word 0xBBBBBBBB
	.sect ".unnamed_by_the_cmd"
	.word 0xCCCCCCCC
