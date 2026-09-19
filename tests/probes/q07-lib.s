; q07: a reference into TI's runtime, and no _c_int00 of its own - the boot member must come
; from the library, drag in what it needs, and leave the rest of the archive behind. This is the
; probe that says how lnk6x walks an archive and when it stops.
	.global main, memcpy
	.text
main:
	CALLP memcpy, B3
	NOP
