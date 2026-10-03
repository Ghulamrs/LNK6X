; q37: the second object (see q37-tie-input), linked after it though its name sorts first.
	.global aa, mm, yy2, pzr, sbare2, sw
	.sect ".const:aa"
aa	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
	.sect ".const:mm"
mm	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
	.sect ".const:yy"
yy2	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
	.sect ".const:pz:r"
pzr	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17
	.sect ".const:.string"
sbare2	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19
	.sect ".const:.string:w"
sw	.byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
