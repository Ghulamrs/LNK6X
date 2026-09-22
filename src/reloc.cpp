/*  reloc.cpp - the C6000 fixups, as the bed spells them.
 *
 *  Three of them carry every reference in tests/ref, and each was checked against the
 *  reference images word by word (docs/elf-observed.md shows the arithmetic):
 *
 *    ABS_L16   MVKL: the low sixteen bits of the address, in bits 7..22
 *    ABS_H16   MVKH: the high sixteen, the same field - no rounding, the pair is MVKL/MVKH
 *    PCR_S21   a branch: (target - the fetch packet the branch is in) >> 2, in bits 7..27
 *
 *  The fetch packet is the instruction's address with its low five bits cleared. Every branch
 *  in the bed happens to sit at the start of one, so the bed cannot tell that rule from
 *  "relative to the instruction"; docs/known.md says so, and a probe would settle it.
 */
#include "lnk.h"
#include <cstdio>

static void field(u8 *p, u32 value, int lsb, int bits)
{
    u32 mask = (bits >= 32) ? 0xFFFFFFFFu : (((1u << bits) - 1u) << lsb);
    u32 w = rd32(p);
    wr32(p, (w & ~mask) | ((value << lsb) & mask));
}

bool apply_reloc(u32 type, u8 *p, u32 P, u32 S, i32 A, std::string &err)
{
    u32 V = S + (u32)A;
    switch (type) {
    case R_C6000_NONE:    break;
    case R_C6000_ABS32:   wr32(p, V); break;
    case R_C6000_ABS16:   wr16(p, (u16)V); break;
    case R_C6000_ABS8:    *p = (u8)V; break;
    case R_C6000_ABS_L16: field(p, V & 0xFFFFu, 7, 16); break;
    case R_C6000_ABS_H16: field(p, (V >> 16) & 0xFFFFu, 7, 16); break;
    case R_C6000_ABS_S16: field(p, V & 0xFFFFu, 7, 16); break;
    case R_C6000_PCR_S21: field(p, (V - (P & ~0x1Fu)) >> 2, 7, 21); break;
    case R_C6000_PCR_S12: field(p, (V - (P & ~0x1Fu)) >> 2, 16, 12); break;
    case R_C6000_PCR_S10: field(p, (V - (P & ~0x1Fu)) >> 2, 13, 10); break;
    case R_C6000_PCR_S7:  field(p, (V - (P & ~0x1Fu)) >> 2, 16, 7); break;
    /*  **The MVKL/MVKH pair of `$PCR_OFFSET(dest, base)`**, and the addend is what says
     *  which base. TI's assembler writes `A = (P & ~31) - base`, so the base label comes
     *  back as `(P & ~31) - A`, and the value is measured from *that label's* fetch
     *  packet - not from the instruction's. `r_addend` is therefore not added to S.
     *
     *  Read off the three sites in tdeh_uwentry_c6000.obj against q07-lib.out, where
     *  base_pcr (+0x08) serves two and cxa_base_pcr (+0xB4) the third: the wanted fields
     *  are 1260, 0720 and 1660, and this gives all three. Both `S + A - P` and
     *  `S + A - (P & ~31)` give none of them - see docs/known.md. */
    case R_C6000_PCR_L16:
        field(p, (S - (((P & ~0x1Fu) - (u32)A) & ~0x1Fu)) & 0xFFFFu, 7, 16); break;
    case R_C6000_PCR_H16:
        field(p, ((S - (((P & ~0x1Fu) - (u32)A) & ~0x1Fu)) >> 16) & 0xFFFFu, 7, 16); break;
    default: {
        /*  Name it. `relocation type 30 is not handled` sent a reader to the ABI to find out
         *  which one that was; these four are the ones the runtime and the C++ unwind tables
         *  bring, and what each computes is still unread (docs/known.md). */
        const char *nm = 0;
        switch (type) {
        case R_C6000_PREL31:  nm = "PREL31";  break;
        case R_C6000_EHTYPE:  nm = "EHTYPE";  break;

        default: break;
        }
        char b[96];
        if (nm) snprintf(b, sizeof b, "relocation R_C6000_%s (%u) is not handled", nm, type);
        else    snprintf(b, sizeof b, "relocation type %u is not handled", type);
        err = b; return false;
    }
    }
    return true;
}
