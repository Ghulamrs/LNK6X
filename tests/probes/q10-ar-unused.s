; q10, the member that is not - it carries a marker in data, so its absence is visible in the
; image itself and not only in the symbol table
	.global ar_unused
	.data
ar_mark	.word 0xCCCCCCCC
	.text
ar_unused:
	MVKL ar_mark, A0
	MVKH ar_mark, A0
	B B3
	NOP 5
