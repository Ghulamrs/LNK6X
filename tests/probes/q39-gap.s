; q39: .const goes into the gap .text's 32-byte alignment leaves after .fardata (0xC4 bytes).
; In the order the object gives them its four parts take 26 bytes; laid by size, with the hole a
; 9-byte part leaves before a word-aligned one, they take 32, and the gap is 28. lnk6x lays it
; after .text; LNK6x laid it in the gap until 2026-10-07, its last word on _c_int00.
	.global _c_int00, kb8, kc4, ka9, ka5
	.text
_c_int00:
	MVKL kb8, A0
	MVKH kb8, A0
	MVKL kc4, A1
	MVKH kc4, A1
	MVKL ka9, A2
	MVKH ka9, A2
	MVKL ka5, A3
	MVKH ka5, A3
	MVKL dd, A4
	MVKH dd, A4
	B B3
	NOP 5
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	.sect ".fardata"
	.align 4
dd:
	.word 0xF0000000
	.word 0xF0000001
	.word 0xF0000002
	.word 0xF0000003
	.word 0xF0000004
	.word 0xF0000005
	.word 0xF0000006
	.word 0xF0000007
	.word 0xF0000008
	.word 0xF0000009
	.word 0xF000000A
	.word 0xF000000B
	.word 0xF000000C
	.word 0xF000000D
	.word 0xF000000E
	.word 0xF000000F
	.word 0xF0000010
	.word 0xF0000011
	.word 0xF0000012
	.word 0xF0000013
	.word 0xF0000014
	.word 0xF0000015
	.word 0xF0000016
	.word 0xF0000017
	.word 0xF0000018
	.word 0xF0000019
	.word 0xF000001A
	.word 0xF000001B
	.word 0xF000001C
	.word 0xF000001D
	.word 0xF000001E
	.word 0xF000001F
	.word 0xF0000020
	.word 0xF0000021
	.word 0xF0000022
	.word 0xF0000023
	.word 0xF0000024
	.word 0xF0000025
	.word 0xF0000026
	.word 0xF0000027
	.word 0xF0000028
	.word 0xF0000029
	.word 0xF000002A
	.word 0xF000002B
	.word 0xF000002C
	.word 0xF000002D
	.word 0xF000002E
	.word 0xF000002F
	.word 0xF0000030
	.sect ".const:b"
	.align 4
kb8:	.word 0xB0B0B0B0, 0xB1B1B1B1
	.sect ".const:c"
	.align 4
kc4:	.word 0xC0C0C0C0
	.sect ".const:a"
ka9:	.byte 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9
	.sect ".const:d"
ka5:	.byte 0xE1, 0xE2, 0xE3, 0xE4, 0xE5
