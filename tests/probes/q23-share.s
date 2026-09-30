; q23: which calls share a trampoline. .text:one calls faraway twice; .text:two calls it once and
; branches to faraway2 with a plain B (not a CALLP); faralias is a second name at faraway's
; address and .text:one calls it first. Read off the map's FAR CALL TRAMPOLINES table: one
; trampoline per callee address or per name, per calling section or per reach, and which name a
; shared one takes. This linker: one per address, reused by every later call in reach, named by
; the first call - so $Tramp$S$$faralias for faraway and faralias both.
	.global _c_int00, faraway, faralias, faraway2
	.sect ".text:one"
_c_int00:
	CALLP faralias, B3
	CALLP faraway, B3
	CALLP faraway, B3
	CALLP two, B3
	NOP
	NOP
	NOP
	B B3
	NOP 5
	.sect ".text:two"
two:
	CALLP faraway, B3
	B faraway2
	NOP 5
	.sect ".fartext"
faraway:
faralias:
	MVK 15, A4
	B B3
	NOP 5
faraway2:
	MVK 16, A4
	B B3
	NOP 5
