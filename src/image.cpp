/*  image.cpp - writing the TI ELF32 executable.
 *
 *  Every constant here was read off tests/ref. Two of them are blobs this linker copies rather
 *  than composes: the build attributes lnk6x writes with its own name in them, and the
 *  .TI.section.flags record. Both are byte-identical in all seven images that link no runtime
 *  library, and differ only in the two that do - so they are constants until a probe says
 *  otherwise, and docs/known.md says exactly that.
 */
#include "lnk.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

static const u8 attributes[] = {
    0x41, 0x26, 0x00, 0x00, 0x00, 0x54, 0x49, 0x00, 0x01, 0x1F, 0x00, 0x00,
    0x00, 0x05, 0x4C, 0x69, 0x6E, 0x6B, 0x65, 0x72, 0x00, 0x08, 0x09, 0x0A,
    0x03, 0x0C, 0x03, 0x80, 0x02, 0x08, 0x82, 0x02, 0x02, 0x84, 0x02, 0x03,
    0x8E, 0x02, 0x02, 0x12, 0x00, 0x00, 0x00, 0x63, 0x36, 0x78, 0x61, 0x62,
    0x69, 0x00, 0x01, 0x07, 0x00, 0x00, 0x00, 0x04, 0x08
};
static const u8 ti_section_flags[26] = { 0x01 };

/*  The six names lnk6x defines whether or not anything wants them, all absolute and all
 *  0xFFFFFFFF: the startup code tests them rather than calls them. */
static const char *invented[] = {
    "binit", "__binit__", "__c_args__",
    "__TI_pprof_out_hndl", "__TI_prof_data_start", "__TI_prof_data_size"
};
static const int ninvented = 6;

namespace {

struct Strtab {
    std::string s;
    Strtab() { s.push_back('\0'); }
    u32 add(const std::string &t) {
        if (t.empty()) return 0;
        size_t at = s.find(std::string("\0", 1) + t + std::string("\0", 1));
        if (at != std::string::npos) return (u32)(at + 1);
        u32 r = (u32)s.size();
        s += t; s.push_back('\0');
        return r;
    }
};

struct OutSym { u32 name, value, size; u8 info, other; u16 shndx; };

struct ByAddr {
    const std::vector<OutSec> *o;
    ByAddr(const std::vector<OutSec> *x) : o(x) {}
    bool operator()(int a, int b) const { return (*o)[a].addr < (*o)[b].addr; }
};

} /* namespace */

bool Link::write_image()
{
    const u32 nout = (u32)outs.size();

    /*  File offsets. Allocated sections are laid down in address order, not in the order the
     *  section table lists them - q03's .bss is at a lower address than its .text and takes a
     *  lower offset, though the command file names .text first. A section with no address of
     *  its own lands at the end of them, and one that is not allocated has no offset at all. */
    std::vector<int> placed, empty;
    for (u32 i = 0; i < nout; i++) {
        if (!(outs[i].flags & SHF_ALLOC)) continue;
        if (outs[i].addr) placed.push_back((int)i); else empty.push_back((int)i);
    }
    std::stable_sort(placed.begin(), placed.end(), ByAddr(&outs));

    u32 pos = 52;
    for (size_t k = 0; k < placed.size(); k++) {
        OutSec &o = outs[placed[k]];
        pos = align_up(pos, o.align);
        o.offset = pos;
        if (o.type == SHT_PROGBITS) pos += o.size;
    }
    for (size_t k = 0; k < empty.size(); k++) outs[empty[k]].offset = pos;

    /*  Nothing with bytes may be left without an offset. Before N3 was found, a section whose
     *  flags said it was not allocated kept offset 0 and its bytes were copied over the ELF
     *  header: a file that was not an ELF file at all, written without a word. */
    for (u32 i = 0; i < nout; i++)
        if (outs[i].type == SHT_PROGBITS && outs[i].size && !outs[i].offset) {
            err = outs[i].name + ": laid out at no file offset (flags " +
                  std::string(1, (char)('0' + (outs[i].flags & 7))) + ")";
            return false;
        }

    u32 attr_off = pos;                       pos += (u32)sizeof attributes;
    u32 sym_off  = align_up(pos, 4);          /* the symbol table is the one that is aligned */

    /* ------------------------------------------------------ the symbol table */
    Strtab str;
    std::vector<OutSym> syms;
    OutSym z; memset(&z, 0, sizeof z);
    syms.push_back(z);

    for (u32 i = 0; i < nout; i++) {
        OutSym y;
        y.name = str.add(outs[i].name);
        y.value = outs[i].addr; y.size = 0;
        y.info = (STB_LOCAL << 4) | STT_SECTION; y.other = 0; y.shndx = (u16)(i + 1);
        syms.push_back(y);
    }

    for (size_t mi = 0; mi < mods.size(); mi++) {
        Module &m = mods[mi];
        bool any = false;
        for (size_t si = 1; si < m.secs.size(); si++) if (m.secs[si].live && m.secs[si].out >= 0) any = true;
        if (!any) continue;
        for (size_t k = 1; k < m.syms.size(); k++) {
            Sym &y = m.syms[k];
            if ((y.info >> 4) != STB_LOCAL) continue;
            u32 type = y.info & 0xF;
            OutSym o;
            o.size = y.size; o.info = y.info; o.other = y.other;
            if (type == STT_FILE) {
                /* the object's own FILE symbol names the source it was assembled from, and
                   that is the name lnk6x carries into the image - not the object's */
                o.name = str.add(y.name); o.value = 0; o.shndx = SHN_ABS;
            } else if (y.shndx == SHN_ABS) {
                o.name = str.add(y.name); o.value = y.value; o.shndx = SHN_ABS;
            } else {
                if (y.shndx >= m.secs.size()) continue;
                InSec &c = m.secs[y.shndx];
                if (!c.live || c.out < 0) continue;
                o.name = str.add(type == STT_SECTION ? outs[c.out].name : y.name);
                o.value = c.addr + y.value;
                o.shndx = (u16)(c.out + 1);
            }
            y.out = (int)syms.size();
            syms.push_back(o);
        }
    }

    u32 first_global = (u32)syms.size();
    for (int i = 0; i < ninvented; i++) {
        OutSym o;
        o.name = str.add(invented[i]);
        o.value = 0xFFFFFFFFu; o.size = 0;
        o.info = (STB_GLOBAL << 4) | STT_NOTYPE; o.other = 2; o.shndx = SHN_ABS;
        syms.push_back(o);
    }
    for (size_t mi = 0; mi < mods.size(); mi++) {
        Module &m = mods[mi];
        for (size_t k = 1; k < m.syms.size(); k++) {
            Sym &y = m.syms[k];
            if ((y.info >> 4) == STB_LOCAL || y.name.empty() || y.shndx == SHN_UNDEF) continue;
            OutSym o;
            o.name = str.add(y.name); o.size = y.size; o.info = y.info; o.other = 2;
            if (y.shndx == SHN_ABS) { o.value = y.value; o.shndx = SHN_ABS; }
            else {
                if (y.shndx >= m.secs.size()) continue;
                InSec &c = m.secs[y.shndx];
                if (!c.live || c.out < 0) continue;
                o.value = c.addr + y.value;
                o.shndx = (u16)(c.out + 1);
            }
            syms.push_back(o);
        }
    }
    {
        int sb = out_index(".bss");
        OutSym o;
        o.name = str.add("__TI_STATIC_BASE");
        o.value = static_base; o.size = 0;
        o.info = (STB_GLOBAL << 4) | STT_NOTYPE; o.other = 2;
        o.shndx = (u16)(sb >= 0 ? sb + 1 : (int)SHN_ABS);
        syms.push_back(o);
    }

    pos = sym_off + (u32)syms.size() * 16;
    u32 tif_off = pos;  pos += (u32)sizeof ti_section_flags;
    u32 str_off = pos;  pos += (u32)str.s.size();

    /* ------------------------------------------------------ the section names */
    Strtab shstr;
    std::vector<u32> shname(nout);
    for (u32 i = 0; i < nout; i++) shname[i] = shstr.add(outs[i].name);
    u32 n_attr = shstr.add(".c6xabi.attributes");
    u32 n_sym  = shstr.add(".symtab");
    u32 n_tif  = shstr.add(".TI.section.flags");
    u32 n_str  = shstr.add(".strtab");
    u32 n_shs  = shstr.add(".shstrtab");

    u32 shstr_off = pos; pos += (u32)shstr.s.size();

    /* ------------------------------------------------------- the segments */
    segs.clear();
    for (size_t k = 0; k < placed.size(); k++) {
        OutSec &o = outs[placed[k]];
        if (!o.size) continue;
        if (!segs.empty()) {
            Seg &g = segs.back();
            u32 flags = g.flags | o.pflags;
            bool mixes = (flags & PF_W) && (flags & PF_X);
            if (!mixes && g.vaddr + g.memsz == o.addr) {
                g.memsz += o.size;
                if (o.type == SHT_PROGBITS) g.filesz = (o.offset + o.size) - g.offset;
                g.flags = flags;
                if (o.align > g.align) g.align = o.align;
                continue;
            }
        }
        Seg g;
        g.offset = o.offset; g.vaddr = o.addr; g.paddr = o.addr;
        g.filesz = (o.type == SHT_PROGBITS) ? o.size : 0;
        g.memsz = o.size; g.flags = o.pflags; g.align = o.align;
        segs.push_back(g);
    }

    u32 ph_off = align_up(pos, 4);
    u32 sh_off = ph_off + (u32)segs.size() * 32;
    u32 total  = sh_off + (nout + 6) * 40;

    /* ---------------------------------------------------------- the bytes */
    std::vector<u8> f(total, 0);
    memcpy(&f[0], "\177ELF\1\1\1", 7);
    wr16(&f[16], 2);                 /* ET_EXEC */
    wr16(&f[18], 140);               /* EM_TI_C6000 */
    wr32(&f[20], 1);
    wr32(&f[24], entry_addr);
    wr32(&f[28], ph_off);
    wr32(&f[32], sh_off);
    wr32(&f[36], 0);
    wr16(&f[40], 52);
    wr16(&f[42], 32); wr16(&f[44], (u16)segs.size());
    wr16(&f[46], 40); wr16(&f[48], (u16)(nout + 6));
    wr16(&f[50], (u16)(nout + 5));

    for (size_t i = 0; i < all.size(); i++) {
        InSec *c = all[i];
        if (c->data.empty() || c->out < 0) continue;
        u32 at = outs[c->out].offset + (c->addr - outs[c->out].addr);
        if (at + c->data.size() > f.size()) { err = "a section lands past the end of the file"; return false; }
        memcpy(&f[at], &c->data[0], c->data.size());
    }
    memcpy(&f[attr_off], attributes, sizeof attributes);
    memcpy(&f[tif_off], ti_section_flags, sizeof ti_section_flags);
    memcpy(&f[str_off], str.s.data(), str.s.size());
    memcpy(&f[shstr_off], shstr.s.data(), shstr.s.size());
    for (size_t i = 0; i < syms.size(); i++) {
        u8 *p = &f[sym_off + i * 16];
        wr32(p, syms[i].name); wr32(p + 4, syms[i].value); wr32(p + 8, syms[i].size);
        p[12] = syms[i].info; p[13] = syms[i].other; wr16(p + 14, syms[i].shndx);
    }
    for (size_t i = 0; i < segs.size(); i++) {
        u8 *p = &f[ph_off + i * 32];
        wr32(p, 1); wr32(p + 4, segs[i].offset); wr32(p + 8, segs[i].vaddr);
        wr32(p + 12, segs[i].paddr); wr32(p + 16, segs[i].filesz); wr32(p + 20, segs[i].memsz);
        wr32(p + 24, segs[i].flags); wr32(p + 28, segs[i].align);
    }

    u8 *sh = &f[sh_off];
    memset(sh, 0, 40);
    for (u32 i = 0; i < nout; i++) {
        u8 *p = sh + (size_t)(i + 1) * 40;
        wr32(p, shname[i]); wr32(p + 4, outs[i].type); wr32(p + 8, outs[i].flags);
        wr32(p + 12, outs[i].addr); wr32(p + 16, outs[i].offset); wr32(p + 20, outs[i].size);
        wr32(p + 24, 0); wr32(p + 28, 0);
        wr32(p + 32, outs[i].parts.empty() ? 1u : outs[i].align);
        wr32(p + 36, outs[i].entsize);
    }
    u8 *p = sh + (size_t)(nout + 1) * 40;
    wr32(p, n_attr); wr32(p + 4, SHT_C6000_ATTRIBUTES); wr32(p + 16, attr_off);
    wr32(p + 20, (u32)sizeof attributes);
    p += 40;
    wr32(p, n_sym); wr32(p + 4, SHT_SYMTAB); wr32(p + 16, sym_off);
    wr32(p + 20, (u32)syms.size() * 16); wr32(p + 24, nout + 4); wr32(p + 28, first_global);
    wr32(p + 36, 16);
    p += 40;
    wr32(p, n_tif); wr32(p + 4, SHT_TI_SECTION_FLAGS); wr32(p + 16, tif_off);
    wr32(p + 20, (u32)sizeof ti_section_flags);
    p += 40;
    wr32(p, n_str); wr32(p + 4, SHT_STRTAB); wr32(p + 8, 0x20); wr32(p + 16, str_off);
    wr32(p + 20, (u32)str.s.size()); wr32(p + 36, 1);
    p += 40;
    wr32(p, n_shs); wr32(p + 4, SHT_STRTAB); wr32(p + 8, 0x20); wr32(p + 16, shstr_off);
    wr32(p + 20, (u32)shstr.s.size()); wr32(p + 36, 1);

    FILE *o = fopen(opt.out.c_str(), "wb");
    if (!o) { err = opt.out + ": cannot create"; return false; }
    if (fwrite(&f[0], 1, f.size(), o) != f.size()) { fclose(o); err = opt.out + ": short write"; return false; }
    fclose(o);
    return true;
}
