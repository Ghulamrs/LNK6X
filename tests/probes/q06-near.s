; q06: near data against far. Near data is addressed from B14, so where the linker puts it and
; what it sets the static base to are one decision, not two - and getting it wrong is the kind
; of fault that only shows at run time on the board.
	.global _c_int00
	.sect ".neardata"
n	.word 0x55555555
	.sect ".const"
k	.word 0x66666666
	.sect ".far"
f	.word 0x77777777
	.text
_c_int00:
	MVKL n, A0
	MVKH n, A0
	MVKL f, A1
	MVKH f, A1
	B B3
	NOP 5
