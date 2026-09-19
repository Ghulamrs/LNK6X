; q05: one object, linked twice - --ram_model and --rom_model. The difference is the whole of
; what the linker adds for C startup: the cinit table, its base and limit symbols, and whatever
; else it decides an image needs to be booted rather than merely loaded.
	.global _c_int00
	.data
val	.word 0x11223344
	.text
_c_int00:
	MVKL val, A0
	MVKH val, A0
	LDW .D1T1 *A0, A1
	NOP 4
	B B3
	NOP 5
