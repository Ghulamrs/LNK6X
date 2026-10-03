; q37: what breaks a size tie between contributions - the name after the colon, or the input
; order. q34's pairs were written in name order, so both rules gave one answer; here every
; pair is written with the later name first (b5 before a5, zz before yy), and q37-tie-aaa,
; whose name sorts before this object's, is linked after it: its aa ties with zz and yy, its
; mm with b5 and a5, and its yy with this yy by name alone. Three pairs are about the cbs
; C++ image, where lnk6x 7.4.4 laid .const:.string:_ZTSSt10bad_typeid before a .const:.string
; of the same size, against strcmp and against q34's pq before pq:r in one object: pz here
; against pz:r there (a prefix, across objects), .string:x here against the bare .string
; there, and the bare .string here against .string:w there. --ram_model, no runtime, flat.cmd.
	.global _c_int00, pb5, pa5, zz, yy, pz, sx, sbare
	.ref aa, mm, yy2, pzr, sbare2, sw
	.sect ".const:b5"
pb5	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
	.sect ".const:a5"
pa5	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
	.sect ".const:zz"
zz	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
	.sect ".const:yy"
yy	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
	.sect ".const:pz"
pz	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17
	.sect ".const:.string:x"
sx	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19
	.sect ".const:.string"
sbare	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
	.text
_c_int00:
	MVKL	pb5, A0
	MVKH	pb5, A0
	MVKL	pa5, A1
	MVKH	pa5, A1
	MVKL	zz, A2
	MVKH	zz, A2
	MVKL	yy, A3
	MVKH	yy, A3
	MVKL	aa, A4
	MVKH	aa, A4
	MVKL	mm, A5
	MVKH	mm, A5
	MVKL	yy2, A6
	MVKH	yy2, A6
	MVKL	pz, A7
	MVKH	pz, A7
	MVKL	pzr, A8
	MVKH	pzr, A8
	MVKL	sx, A9
	MVKH	sx, A9
	MVKL	sbare, B0
	MVKH	sbare, B0
	MVKL	sbare2, B1
	MVKH	sbare2, B1
	MVKL	sw, B2
	MVKH	sw, B2
	BNOP	B3, 5
