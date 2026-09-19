; q03: storage that is allocated but not written - .bss and a .usect of its own. The image must
; grow in memory and not on disk, and the two must be told apart in the program headers.
	.global _c_int00
	.bss	buf, 256, 8
count	.usect ".far", 16, 4
	.text
_c_int00:
	MVKL buf, A0
	MVKH buf, A0
	MVKL count, A1
	MVKH count, A1
	B B3
	NOP 5
