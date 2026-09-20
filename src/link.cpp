/*  link.cpp - what is in the image, and where.
 *
 *  The order of a link, in the order this file does it:
 *
 *    read        every object named on the command line
 *    resolve     the global names, so a reference can be followed across objects
 *    eliminate   from the entry point, follow every relocation and keep what it reaches.
 *                This is not an optimisation to be switched on: lnk6x does it by default for
 *                EABI, and q02 is the proof - three sections of data the command file names
 *                and nothing refers to are not in the image at all
 *    sections    the output list is the SECTIONS block in its own order, and then whichever
 *                of the sixteen standard sections it did not name, in theirs
 *    allocate    .bss first, because it is what the static base points at, then the rest in
 *                SECTIONS order, each from the memory range its entry asks for
 *    offsets     file offsets in address order, not in section-table order
 */
#include "lnk.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

/*  The sixteen sections lnk6x writes whether or not anything asked for them, in the order it
 *  appends the ones the command file left out - q08 leaves out .fardata and .rodata and gets
 *  them back in exactly this order - with the flags it gives each when it is empty. */
static const struct { const char *name; u32 flags; u32 entsize; } standard[] = {
    { ".text",          SHF_ALLOC | SHF_EXECINSTR,  0 },
    { ".const",         0,                          0 },
    { ".data",          SHF_ALLOC | SHF_WRITE,      0 },
    { ".bss",           SHF_ALLOC | SHF_WRITE,      0 },
    { ".far",           SHF_ALLOC | SHF_WRITE,      0 },
    { ".fardata",       SHF_ALLOC | SHF_WRITE,      0 },
    { ".neardata",      SHF_ALLOC | SHF_WRITE,      0 },
    { ".rodata",        SHF_ALLOC,                  0 },
    { ".cinit",         SHF_ALLOC,                  0 },
    { ".init_array",    SHF_ALLOC | SHF_WRITE,      0 },
    { ".switch",        0,                          0 },
    { ".cio",           0,                          0 },
    { ".stack",         0,                          0 },
    { ".sysmem",        0,                          0 },
    { ".c6xabi.exidx",  SHF_ALLOC | 0x80,           8 },
    { ".c6xabi.extab",  SHF_ALLOC,                  0 }
};
static const int nstandard = (int)(sizeof standard / sizeof standard[0]);

static bool standard_flags(const std::string &n, u32 &flags, u32 &entsize)
{
    for (int i = 0; i < nstandard; i++)
        if (n == standard[i].name) { flags = standard[i].flags; entsize = standard[i].entsize; return true; }
    flags = 0; entsize = 0;
    return false;
}

static bool slurp(const std::string &path, std::vector<u8> &out, std::string &err)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) { err = path + ": cannot open"; return false; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize((size_t)(n > 0 ? n : 0));
    if (n > 0 && fread(&out[0], 1, (size_t)n, f) != (size_t)n) { fclose(f); err = path + ": short read"; return false; }
    fclose(f);
    return true;
}

/* the name the map and the symbol table use: the file's own, without its directory */
static std::string basename_of(const std::string &p)
{
    size_t s = p.find_last_of("/\\");
    return (s == std::string::npos) ? p : p.substr(s + 1);
}

bool Link::read_inputs()
{
    for (size_t i = 0; i < opt.inputs.size(); i++) {
        std::vector<u8> b;
        if (!slurp(opt.inputs[i], b, err)) return false;
        Module m;
        if (!elf_read(b.empty() ? (const u8 *)"" : &b[0], b.size(), basename_of(opt.inputs[i]), m, err)) return false;
        mods.push_back(m);
    }
    for (size_t mi = 0; mi < mods.size(); mi++) {
        for (size_t si = 0; si < mods[mi].secs.size(); si++) mods[mi].secs[si].module = (int)mi;
        for (size_t k = 0; k < mods[mi].syms.size(); k++) {
            const Sym &y = mods[mi].syms[k];
            if ((y.info >> 4) == STB_LOCAL || y.name.empty()) continue;
            if (y.shndx == SHN_UNDEF) continue;
            if (defined.find(y.name) == defined.end())
                defined[y.name] = std::make_pair((int)mi, (int)k);
        }
    }
    return true;
}

/* ------------------------------------------------- unused section elimination */

bool Link::eliminate()
{
    std::map<std::string, std::pair<int, int> >::iterator e = defined.find(opt.entry);
    if (e == defined.end()) { err = "entry point not found: " + opt.entry; return false; }

    std::vector<std::pair<int, int> > work;      /* module, section */
    Module &em = mods[e->second.first];
    Sym &es = em.syms[e->second.second];
    if (es.shndx >= em.secs.size()) { err = opt.entry + ": defined in no section"; return false; }
    em.secs[es.shndx].live = true;
    work.push_back(std::make_pair(e->second.first, (int)es.shndx));

    while (!work.empty()) {
        std::pair<int, int> at = work.back(); work.pop_back();
        InSec &c = mods[at.first].secs[at.second];
        for (size_t r = 0; r < c.relocs.size(); r++) {
            const Rel &rl = c.relocs[r];
            if (rl.sym >= mods[at.first].syms.size()) { err = c.name + ": a relocation names no symbol"; return false; }
            const Sym &y = mods[at.first].syms[rl.sym];
            int tm = at.first; u16 tx = y.shndx;
            if (y.shndx == SHN_UNDEF) {
                std::map<std::string, std::pair<int, int> >::iterator d = defined.find(y.name);
                if (d == defined.end()) { err = "unresolved symbol: " + y.name; return false; }
                tm = d->second.first;
                tx = mods[tm].syms[d->second.second].shndx;
            }
            if (tx == SHN_ABS || tx == SHN_UNDEF || tx >= mods[tm].secs.size()) continue;
            InSec &t = mods[tm].secs[tx];
            if (t.live) continue;
            t.live = true;
            work.push_back(std::make_pair(tm, (int)tx));
        }
    }
    return true;
}

/* --------------------------------------------------------- output sections */

int Link::out_index(const std::string &name) const
{
    for (size_t i = 0; i < outs.size(); i++) if (outs[i].name == name) return (int)i;
    return -1;
}

bool Link::build_sections()
{
    for (size_t i = 0; i < cmd.secs.size(); i++) {
        OutSec o;
        o.name = cmd.secs[i].name;
        o.load = cmd.secs[i].load;
        o.run  = cmd.secs[i].run.empty() ? cmd.secs[i].load : cmd.secs[i].run;
        o.progbits = cmd.secs[i].has_fill;
        o.type = SHT_NOBITS; o.addr = 0; o.size = 0; o.align = 1; o.offset = 0; o.pflags = 0;
        standard_flags(o.name, o.flags, o.entsize);
        outs.push_back(o);
    }
    for (int i = 0; i < nstandard; i++) {
        if (out_index(standard[i].name) >= 0) continue;
        OutSec o;
        o.name = standard[i].name;
        o.flags = standard[i].flags; o.entsize = standard[i].entsize;
        o.type = SHT_NOBITS; o.addr = 0; o.size = 0; o.align = 1; o.offset = 0; o.pflags = 0;
        o.progbits = false;
        outs.push_back(o);
    }

    for (size_t mi = 0; mi < mods.size(); mi++) {
        for (size_t si = 1; si < mods[mi].secs.size(); si++) {
            InSec &c = mods[mi].secs[si];
            if (!c.live || !(c.flags & SHF_ALLOC)) continue;
            int oi = out_index(c.name);
            if (oi < 0) {
                OutSec o;
                o.name = c.name; o.flags = 0; o.entsize = 0;
                o.type = SHT_NOBITS; o.addr = 0; o.size = 0; o.align = 1; o.offset = 0;
                o.pflags = 0; o.progbits = false;
                outs.push_back(o);
                oi = (int)outs.size() - 1;
            }
            c.out = oi;
            outs[oi].parts.push_back((int)all.size());
            all.push_back(&c);
        }
    }

    for (size_t i = 0; i < outs.size(); i++) {
        OutSec &o = outs[i];
        for (size_t k = 0; k < o.parts.size(); k++) {
            InSec *c = all[o.parts[k]];
            if (c->align > o.align) o.align = c->align;
            if (c->type == SHT_PROGBITS) o.progbits = true;
            o.pflags |= PF_R;
            if (c->flags & SHF_WRITE) o.pflags |= PF_W;
            if (c->flags & SHF_EXECINSTR) o.pflags |= PF_X;
        }
        o.type = o.progbits ? SHT_PROGBITS : SHT_NOBITS;
    }
    return true;
}

/* ------------------------------------------------------------- allocation */

bool Link::allocate()
{
    /*  .bss goes first. It is not first in any command file the bed uses, and it still comes
     *  out at the origin in q03 - what decides it is that __TI_STATIC_BASE points at .bss, so
     *  the near region has to start where the range does. */
    std::vector<int> order;
    int bi = out_index(".bss");
    if (bi >= 0 && !outs[bi].parts.empty()) order.push_back(bi);
    for (size_t i = 0; i < outs.size(); i++) if ((int)i != bi) order.push_back((int)i);

    for (size_t k = 0; k < order.size(); k++) {
        OutSec &o = outs[order[k]];
        Range *r = o.run.empty() ? 0 : cmd.range(o.run);
        if (o.parts.empty()) {
            /*  An empty section still has an address when it holds bytes - a fill makes it
             *  initialised - and none at all when it does not. */
            o.addr = (o.progbits && r) ? r->origin : 0;
            continue;
        }
        if (!r) { err = o.name + ": no memory range for it"; return false; }
        u32 at = r->origin + r->used;
        at = align_up(at, o.align);
        for (size_t i = 0; i < cmd.secs.size(); i++)
            if (cmd.secs[i].name == o.name && cmd.secs[i].has_align) at = align_up(at, cmd.secs[i].align);
        o.addr = at;
        for (size_t p = 0; p < o.parts.size(); p++) {
            InSec *c = all[o.parts[p]];
            at = align_up(at, c->align);
            c->addr = at;
            c->load = at;
            at += c->size;
        }
        o.size = at - o.addr;
        if (at - r->origin > r->length) { err = o.name + ": does not fit in " + o.run; return false; }
        r->used = at - r->origin;
    }

    int sb = out_index(".bss");
    static_base = (sb >= 0) ? outs[sb].addr : 0;

    std::map<std::string, std::pair<int, int> >::iterator e = defined.find(opt.entry);
    if (!sym_addr(e->second.first, e->second.second, entry_addr)) return false;
    return true;
}

bool Link::sym_addr(int mod, int sym, u32 &a)
{
    const Sym &y = mods[mod].syms[sym];
    if (y.shndx == SHN_ABS) { a = y.value; return true; }
    if (y.shndx == SHN_UNDEF) {
        std::map<std::string, std::pair<int, int> >::iterator d = defined.find(y.name);
        if (d == defined.end()) { err = "unresolved symbol: " + y.name; return false; }
        return sym_addr(d->second.first, d->second.second, a);
    }
    if (y.shndx >= mods[mod].secs.size()) { err = y.name + ": names no section"; return false; }
    const InSec &c = mods[mod].secs[y.shndx];
    if (!c.live) { err = y.name + ": in a section that was eliminated"; return false; }
    a = c.addr + y.value;
    return true;
}

bool Link::fix_up()
{
    for (size_t i = 0; i < all.size(); i++) {
        InSec *c = all[i];
        if (c->data.empty()) continue;
        for (size_t r = 0; r < c->relocs.size(); r++) {
            const Rel &rl = c->relocs[r];
            if (rl.offset + 4 > c->data.size()) { err = c->name + ": a relocation falls past its section"; return false; }
            u32 S;
            if (!sym_addr(c->module, rl.sym, S)) return false;
            std::string e2;
            if (!apply_reloc(rl.type, &c->data[rl.offset], c->addr + rl.offset, S, rl.addend, e2))
                { err = c->name + ": " + e2; return false; }
        }
    }
    return true;
}
