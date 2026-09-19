; q04 b: the other half, with data of its own so both merges can be read
	.global helper
	.data
bdata	.word 0x12345678
	.text
helper:
	MVKL bdata, A0
	MVKH bdata, A0
	B B3
	NOP 5
