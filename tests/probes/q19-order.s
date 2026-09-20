; q19: six sections of six clearly different sizes, every one of them reached, so the order
; lnk6x allocates them in can be read off the addresses. q07 came out .stack, .text, .sysmem,
; .const, .c6xabi.extab, .fardata, .switch, .cinit, .c6xabi.exidx - which is neither the
; SECTIONS order of the command file nor anything else the bed has explained, though the first
; four of them are in descending size. order.cmd names these six in one order and their sizes
; are in another, so the two readings cannot both survive.
	.global _c_int00
	.sect ".const"                      ; 0x40 bytes
c:	.space 0x40
	.sect ".rodata"                     ; 0x10
r:	.space 0x10
	.sect ".switch"                     ; 0x80
s:	.space 0x80
	.sect ".fardata"                    ; 0x20
f:	.space 0x20
	.sect ".neardata"                   ; 0x100
n:	.space 0x100
	.text
_c_int00:
	MVKL c, A0
	MVKH c, A0
	MVKL r, A1
	MVKH r, A1
	MVKL s, A2
	MVKH s, A2
	MVKL f, A3
	MVKH f, A3
	MVKL n, A4
	MVKH n, A4
	B B3
	NOP 5
