; q34: contributions of one size and different alignments. The tie rule read off the kernels
; (the name after the colon, ascending) was read where every tie had one alignment; the cbs
; C++ image has two 15-byte .const:.string pieces, aligned 8 and 1, and lnk6x 7.4.4 lays the
; aligned one first against the name order. Each pair here names the more aligned piece
; second, so the name rule and an alignment rule give opposite answers; the first pair also
; tells "larger alignment first" from "size rounded up to the alignment": b1 is 9 bytes
; aligned 8 (16 rounded) against a1's 12 aligned 1. a4/b4 is the control, where the two
; rules agree. Pairs 5 and 6 are about the comparison itself, after the cbs image put
; .const:.string:_ZTSSt10bad_typeid in front of a .const:.string of the same size: pq against
; pq:r (one key a prefix of the other, split at a colon) and s against s_ (a prefix with no
; colon) - strcmp puts the shorter first, lnk6x may not. --ram_model, no runtime, flat.cmd.
	.global _c_int00
	.sect ".const:a1"
pa1	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
	.sect ".const:b1"
	.align 8
pb1	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9
	.sect ".const:a2"
pa2	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	.sect ".const:b2"
	.align 8
pb2	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	.sect ".const:a3"
	.align 4
pa3	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
	.sect ".const:b3"
	.align 8
pb3	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
	.sect ".const:pq"
ppq	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
	.sect ".const:pq:r"
ppqr	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
	.sect ".const:s"
ps	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
	.sect ".const:s_"
ps_	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
	.sect ".const:a4"
	.align 2
pa4	.byte 1, 2, 3, 4, 5, 6, 7
	.sect ".const:b4"
pb4	.byte 1, 2, 3, 4, 5, 6, 7
	.text
_c_int00:
	MVKL	pa1, A0
	MVKH	pa1, A0
	MVKL	pb1, A1
	MVKH	pb1, A1
	MVKL	pa2, A2
	MVKH	pa2, A2
	MVKL	pb2, A3
	MVKH	pb2, A3
	MVKL	pa3, A4
	MVKH	pa3, A4
	MVKL	pb3, A5
	MVKH	pb3, A5
	MVKL	pa4, A6
	MVKH	pa4, A6
	MVKL	pb4, A7
	MVKH	pb4, A7
	MVKL	ppq, A8
	MVKH	ppq, A8
	MVKL	ppqr, A9
	MVKH	ppqr, A9
	MVKL	ps, B0
	MVKH	ps, B0
	MVKL	ps_, B1
	MVKH	ps_, B1
	BNOP	B3, 5
