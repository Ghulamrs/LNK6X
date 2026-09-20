; q12: whether ".bss first" is about .bss by name or about the near region: a second
; uninitialised section (.usect ".mybss") and a .neardata beside .bss, all reached, and the
; command file naming .text first as flat.cmd does. Where each lands says what the rule is.
	.global _c_int00
	.bss	buf, 64, 8
other	.usect ".mybss", 32, 4
	.sect ".neardata"
near	.word 7
	.text
_c_int00:
	MVKL buf, A0
	MVKH buf, A0
	MVKL other, A1
	MVKH other, A1
	MVKL near, A2
	MVKH near, A2
	B B3
	NOP 5
