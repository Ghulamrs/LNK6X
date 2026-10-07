; q38: local strings in .const, at an odd address once q38-odd-a's .const precedes them,
; reached by MVKL/MVKH from .text and by words in .fardata.
	.global _c_int00
	.ref odd_a, wc, hc
	.sect ".const"
s1:	.byte 0x68, 0x65, 0x6C, 0x6C, 0x6F, 0
s2:	.byte 0x77, 0x6F, 0x72, 0x6C, 0x64, 0
s3:	.byte 1, 2, 3
	.sect ".fardata"
	.align 4
pp:	.word s1
	.word s2
	.word s3
	.text
_c_int00:
	MVKL s1, A0
	MVKH s1, A0
	MVKL s2, A1
	MVKH s2, A1
	MVKL s3, A2
	MVKH s3, A2
	MVKL pp, A3
	MVKH pp, A3
	MVKL odd_a, A4
	MVKH odd_a, A4
	MVKL wc, A5
	MVKH wc, A5
	MVKL hc, A6
	MVKH hc, A6
	B B3
	NOP 5
