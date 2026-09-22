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

/*  What one module brings: its sections get their module number, and its global definitions
 *  go into the table. A strong definition wins over a weak one wherever it is met - q14 links
 *  the weak object first and lnk6x still gives `wk` the strong object's address - and two
 *  weak definitions keep the first. */
/*  Whether a definition is one the image will carry: a symbol in a section that is not
 *  allocated - debug information, the attribute records - is not. */
bool Link::in_image(int mi, int sym) const
{
    const Sym &y = mods[mi].syms[sym];
    if (y.shndx == SHN_ABS || y.shndx == SHN_COMMON) return true;
    if (y.shndx >= mods[mi].secs.size()) return false;
    return (mods[mi].secs[y.shndx].flags & SHF_ALLOC) != 0;
}

bool Link::take_module(int mi)
{
    for (size_t si = 0; si < mods[mi].secs.size(); si++) mods[mi].secs[si].module = mi;
    /*  A section marked SHF_GROUP is C++'s vague linkage: the same definition compiled into
     *  every object that needed it, for the linker to keep once. The runtime's `_ZTSv` is in
     *  `.const:.typeinfo:_ZTSv`, flags 0x202, in both tdeh_cpp_abi.obj and typeinfo_.obj, and
     *  lnk6x links them without a word - so the first copy of a group section name is kept and
     *  every later one is dropped, with the symbols in it. */
    for (size_t si = 1; si < mods[mi].secs.size(); si++) {
        InSec &c = mods[mi].secs[si];
        if (!(c.flags & SHF_GROUP) || c.name.empty()) continue;
        if (groups[c.name]) c.dropped = true;
        else groups[c.name] = true;
    }
    for (size_t k = 0; k < mods[mi].syms.size(); k++) {
        const Sym &y = mods[mi].syms[k];
        if ((y.info >> 4) == STB_LOCAL || y.name.empty()) continue;
        if (y.shndx == SHN_UNDEF) continue;
        if (y.shndx < mods[mi].secs.size() && mods[mi].secs[y.shndx].dropped) continue;
        std::map<std::string, std::pair<int, int> >::iterator d = defined.find(y.name);
        if (d == defined.end()) { defined[y.name] = std::make_pair(mi, (int)k); continue; }
        const Sym &had = mods[d->second.first].syms[d->second.second];
        if ((had.info >> 4) == STB_WEAK && (y.info >> 4) == STB_GLOBAL) {
            d->second = std::make_pair(mi, (int)k);
            continue;
        }
        if ((had.info >> 4) == STB_GLOBAL && (y.info >> 4) == STB_GLOBAL &&
            in_image(d->second.first, d->second.second) && in_image(mi, (int)k)) {
            /*  Two strong definitions of one name, both of them in the image. lnk6x says
             *  "symbol redefined" and stops; this linker kept the first and said nothing, so
             *  two objects that both defined _c_int00 linked quietly (the review's N16).
             *
             *  Only definitions that are in the image count. The runtime's objects share
             *  debug-information symbols by design - boot.obj and args_main.obj both define
             *  __TI_DW.debug_info.$base_types.<hash>, in a section that is not allocated and
             *  never reaches an image - and lnk6x links them without a word. */
            err = "symbol redefined: " + y.name + ", in " +
                  mods[d->second.first].name + " and in " + mods[mi].name;
            return false;
        }
    }
    return true;
}

bool Link::read_inputs()
{
    /*  The command line's objects, in its order - that order is the layout order - and its
     *  archives kept aside. A `-l name` is looked for through the `-i` directories; a name
     *  given with a directory, or an object, is opened as it stands. */
    std::vector<Archive> libs;
    for (size_t i = 0; i < opt.inputs.size(); i++) {
        std::string path = opt.inputs[i];
        std::vector<u8> b;
        if (!slurp(path, b, err)) {
            std::string found = find_library(opt, opt.inputs[i]);
            if (found.empty()) { err = opt.inputs[i] + ": cannot open"; return false; }
            path = found;
            if (!slurp(path, b, err)) return false;
        }
        if (b.size() >= 8 && memcmp(&b[0], "!<arch>\n", 8) == 0) {
            Archive a;
            if (!a.load(path, err)) return false;
            libs.push_back(a);
            continue;
        }
        Module m;
        if (!elf_read(b.empty() ? (const u8 *)"" : &b[0], b.size(), basename_of(path), m, err)) return false;
        mods.push_back(m);
    }
    for (size_t mi = 0; mi < mods.size(); mi++) if (!take_module((int)mi)) return false;
    if (libs.empty()) { add_linker_symbols(); return true; }

    /*  Then the archives, in passes. One pass takes every member the current undefined set
     *  names; the members it take may ask for more, which the next pass answers. The entry
     *  point is asked for first, since with a runtime it is the library that has it. A weak
     *  undefined name does not pull a member - that is what weak means - which is why the
     *  runtime's optional hooks do not drag their implementations in. */
    std::map<std::string, bool> weak_only;
    for (;;) {
        std::vector<std::string> want;
        std::map<std::string, bool> asked;
        if (defined.find(opt.entry) == defined.end()) { want.push_back(opt.entry); asked[opt.entry] = true; }
        for (size_t mi = 0; mi < mods.size(); mi++)
            for (size_t k = 0; k < mods[mi].syms.size(); k++) {
                const Sym &y = mods[mi].syms[k];
                if (y.shndx != SHN_UNDEF || y.name.empty()) continue;
                if ((y.info >> 4) != STB_GLOBAL) { weak_only[y.name] = true; continue; }
                if (defined.find(y.name) != defined.end() || asked[y.name]) continue;
                asked[y.name] = true;
                want.push_back(y.name);
            }
        bool took = false;
        for (size_t a = 0; a < libs.size(); a++) {
            Archive &ar = libs[a];
            std::vector<u32> pull;
            for (size_t u = 0; u < want.size(); u++) {
                if (defined.find(want[u]) != defined.end()) continue;
                for (size_t k = 0; k < ar.index.size(); k++) {
                    if (ar.index[k].first != want[u]) continue;
                    u32 off = ar.index[k].second;
                    if (std::find(ar.taken.begin(), ar.taken.end(), off) != ar.taken.end()) break;
                    if (std::find(pull.begin(), pull.end(), off) == pull.end()) pull.push_back(off);
                    break;
                }
            }
            for (size_t k = 0; k < pull.size(); k++) {
                Module m;
                if (!ar.member(pull[k], m, err)) return false;
                ar.taken.push_back(pull[k]);
                mods.push_back(m);
                if (!take_module((int)mods.size() - 1)) return false;
                took = true;
            }
        }
        if (!took) break;
    }
    add_linker_symbols();
    return true;
}

/*  The names lnk6x defines itself. It defines each one only when something asks for it - a
 *  link with no runtime has none of them, and q09's map lists only the six it invents whatever
 *  happens - and it writes them against an output section rather than as absolutes, except the
 *  three that are sizes. All of this is read off q07's image and map (the review's N5).
 *
 *  __TI_INITARRAY_Base and _Limit are not here: the runtime declares them weak undefined and
 *  q07 shows lnk6x leaving them that way, which the weak rule already does.
 */
void Link::add_linker_symbols()
{
    static const struct { const char *name; const char *sec; int which; } table[] = {
        { "__TI_STACK_END",           ".stack",         1 },   /* the end of the section */
        { "__TI_STACK_SIZE",          "",               2 },   /* --stack_size, absolute */
        { "__TI_SYSMEM_SIZE",         "",               3 },   /* --heap_size, absolute */
        { "__TI_UNWIND_TABLE_START",  ".c6xabi.exidx",  0 },
        { "__TI_UNWIND_TABLE_END",    ".c6xabi.exidx",  1 },
        { "__TI_CINIT_Base",          ".cinit",         0 },
        { "__TI_CINIT_Limit",         ".cinit",         1 },
        { "__TI_Handler_Table_Base",  ".cinit",         0 },
        { "__TI_Handler_Table_Limit", ".cinit",         1 },
        { "__TI_STATIC_BASE",         ".bss",           0 },   /* last: lnk6x writes it last */
        { 0, 0, 0 }
    };
    /*  The six lnk6x invents whether or not anything wants them: they are absolute and
     *  0xFFFFFFFF, image.cpp writes them at the head of the globals, and they are registered
     *  here as well so that a runtime's reference to `__binit__` resolves. */
    static const char *const always[] = {
        "binit", "__binit__", "__c_args__",
        "__TI_pprof_out_hndl", "__TI_prof_data_start", "__TI_prof_data_size", 0
    };
    std::map<std::string, bool> wanted;
    wanted["__TI_STATIC_BASE"] = true;      /* this one is in every image the bed has */
    for (size_t mi = 0; mi < mods.size(); mi++)
        for (size_t k = 0; k < mods[mi].syms.size(); k++) {
            const Sym &y = mods[mi].syms[k];
            if (y.shndx == SHN_UNDEF && !y.name.empty() && defined.find(y.name) == defined.end())
                wanted[y.name] = true;
        }

    /*  COMMON: a symbol with no section and a size, which is what `st_shndx` 0xFFF2 means.
     *  The runtime has four - parmbuf, __TI_tmpnams, _ZSt16__dummy_typeinfo, __dso_handle -
     *  and sym_addr used to refuse them as "names no section". The largest size wins, the
     *  alignment is the symbol's value, and they are allocated together in .bss (N7). Where
     *  lnk6x puts that run inside .bss is not something the bed shows. */
    std::map<std::string, std::pair<u32, u32> > common;      /* name -> size, alignment */
    for (size_t mi = 0; mi < mods.size(); mi++)
        for (size_t k = 0; k < mods[mi].syms.size(); k++) {
            const Sym &y = mods[mi].syms[k];
            if (y.shndx != SHN_COMMON || y.name.empty()) continue;
            if (defined.find(y.name) != defined.end()) continue;
            std::map<std::string, std::pair<u32, u32> >::iterator it = common.find(y.name);
            u32 al = y.value ? y.value : 1;
            if (it == common.end()) common[y.name] = std::make_pair(y.size, al);
            else {
                if (y.size > it->second.first)  it->second.first = y.size;
                if (al > it->second.second) it->second.second = al;
            }
        }

    Module m;
    m.name = "<linker>";
    m.file_sym = -1;
    m.syms.push_back(Sym());                /* the null symbol every module's table starts with */
    {
        InSec null_sec;
        null_sec.name = ""; null_sec.type = SHT_NULL; null_sec.flags = 0; null_sec.size = 0;
        null_sec.align = 1; null_sec.entsize = 0; null_sec.module = -1; null_sec.index = 0;
        null_sec.live = false; null_sec.out = -1; null_sec.addr = 0; null_sec.load = 0;
        m.secs.push_back(null_sec);
        InSec bss;
        bss.name = ".bss"; bss.type = SHT_NOBITS; bss.flags = SHF_ALLOC | SHF_WRITE;
        bss.size = 0; bss.align = 1; bss.entsize = 0; bss.module = -1; bss.index = 1;
        bss.live = false; bss.out = -1; bss.addr = 0; bss.load = 0;
        for (std::map<std::string, std::pair<u32, u32> >::iterator it = common.begin();
             it != common.end(); ++it) {
            u32 al = it->second.second;
            if (al > bss.align) bss.align = al;
            bss.size = align_up(bss.size, al);
            Sym y;
            y.name = it->first; y.value = bss.size; y.size = it->second.first;
            y.info = (u8)((STB_GLOBAL << 4) | STT_OBJECT); y.other = 2; y.shndx = 1;
            m.syms.push_back(y);
            bss.size += it->second.first;
        }
        m.secs.push_back(bss);
    }
    for (int i = 0; always[i]; i++) {
        if (defined.find(always[i]) != defined.end()) continue;   /* an object got there first */
        Sym y;
        y.name = always[i]; y.value = 0xFFFFFFFFu;
        y.info = (u8)((STB_GLOBAL << 4) | STT_NOTYPE); y.other = 2; y.shndx = SHN_ABS;
        m.syms.push_back(y);
    }
    for (int i = 0; table[i].name; i++) {
        if (!wanted[table[i].name]) continue;
        Sym y;
        y.name = table[i].name;
        y.info = (u8)((STB_GLOBAL << 4) | STT_NOTYPE);
        y.other = 2;
        y.shndx = SHN_ABS;              /* an address while it is resolved; see lnk_out */
        y.lnk_out = -1;
        m.syms.push_back(y);
    }
    mods.push_back(m);
    lnk_mod = (int)mods.size() - 1;
    take_module(lnk_mod);      /* its names were all absent, so this cannot find a duplicate */
}

/*  Their values, once the sections have addresses. `.stack` and `.sysmem` are the linker's
 *  own too: q07's are 0x4000 and 0x1000, which are --stack_size and --heap_size, and no input
 *  section contributes to either. A link that asks for neither leaves both at zero, which is
 *  what q01 through q14 show (the review's N10). */
void Link::set_linker_symbols()
{
    if (lnk_mod < 0) return;
    Module &m = mods[lnk_mod];
    for (size_t k = 0; k < m.syms.size(); k++) {
        Sym &y = m.syms[k];
        if (y.name == "__TI_STACK_SIZE")  { y.value = cmd.stack_size; continue; }
        if (y.name == "__TI_SYSMEM_SIZE") { y.value = cmd.heap_size;  continue; }
        if (y.name == "__TI_STATIC_BASE") {
            int b = out_index(".bss");
            y.value = static_base;
            y.lnk_out = b;                  /* .bss when there is one, absolute otherwise */
            continue;
        }
        std::string sec = ".cinit";
        int end = 1;
        if (y.name == "__TI_STACK_END")                 sec = ".stack";
        else if (y.name.compare(0, 18, "__TI_UNWIND_TABLE") == 0) sec = ".c6xabi.exidx";
        if (y.name == "__TI_UNWIND_TABLE_START" || y.name == "__TI_CINIT_Base" ||
            y.name == "__TI_Handler_Table_Base") end = 0;
        int oi = out_index(sec);
        if (oi < 0) continue;
        y.value = outs[oi].addr + (end ? outs[oi].size : 0);
        y.lnk_out = oi;
    }
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
            /*  A name that is not the object's own goes through the table of definitions,
             *  even when this object defines it too: q14's weak `wk` is defined in the
             *  referring object and lnk6x still reaches the strong one in the other. */
            if ((y.info >> 4) != STB_LOCAL && !y.name.empty()) {
                std::map<std::string, std::pair<int, int> >::iterator d = defined.find(y.name);
                if (d != defined.end()) {
                    tm = d->second.first;
                    tx = mods[tm].syms[d->second.second].shndx;
                } else if (y.shndx == SHN_UNDEF) {
                    /*  An undefined weak reference is not an error: it is zero, and it pulls
                     *  nothing in with it (q14 - lnk6x keeps it as UNDEF WEAK). */
                    if ((y.info >> 4) == STB_WEAK) continue;
                    err = "unresolved symbol: " + y.name; return false;
                }
            } else if (y.shndx == SHN_UNDEF) {
                err = "unresolved symbol: " + y.name; return false;
            }
            if (tx == SHN_ABS || tx == SHN_UNDEF || tx >= mods[tm].secs.size()) continue;
            InSec &t = mods[tm].secs[tx];
            if (t.live || t.dropped) continue;
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
            if (!c.live || c.dropped || !(c.flags & SHF_ALLOC)) continue;
            /*  A subsection joins its base section: the runtime's 2,731 `.text:name` sections
             *  and the corpus's `.c6xabi.extab:*` are part of `.text` and `.c6xabi.extab`,
             *  not sections of their own - unless the command file names the full name, which
             *  is what is asked first (the review's N6). */
            int oi = out_index(c.name);
            if (oi < 0 && base_section(c.name) != c.name) oi = out_index(base_section(c.name));
            if (oi < 0) {
                OutSec o;
                o.name = base_section(c.name); o.flags = 0; o.entsize = 0;
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

    /*  A `{ *(.text:early) *(.text) *(.text:*) }` list is an order, not decoration: the parts
     *  that a pattern names come first, in the order the patterns were written, and whatever
     *  the list does not name keeps the order it was read in after them. */
    for (size_t i = 0; i < cmd.secs.size() && i < outs.size(); i++) {
        const SecSpec &sp = cmd.secs[i];
        if (sp.inputs.empty() || outs[i].name != sp.name) continue;
        std::vector<int> &ps = outs[i].parts;
        std::vector<std::pair<size_t, int> > keyed;
        for (size_t k = 0; k < ps.size(); k++) {
            size_t rank = sp.inputs.size();
            for (size_t q = 0; q < sp.inputs.size(); q++)
                if (sec_matches(sp.inputs[q], all[ps[k]]->name)) { rank = q; break; }
            keyed.push_back(std::make_pair(rank, ps[k]));
        }
        std::stable_sort(keyed.begin(), keyed.end());
        for (size_t k = 0; k < ps.size(); k++) ps[k] = keyed[k].second;
    }

    for (size_t i = 0; i < outs.size(); i++) {
        OutSec &o = outs[i];
        for (size_t k = 0; k < o.parts.size(); k++) {
            InSec *c = all[o.parts[k]];
            if (c->align > o.align) o.align = c->align;
            if (c->type == SHT_PROGBITS) o.progbits = true;
            /*  The flags of a section that has input come from that input, not from the table
             *  above: the table was read off *empty* sections, where lnk6x writes 0 for
             *  .const, .switch, .cio, .stack and .sysmem, and q13 shows all three of .const,
             *  .rodata and .switch with ALLOC once they hold bytes. A name the table does not
             *  know had 0 and was therefore never laid out at all - its bytes went to file
             *  offset 0, over the ELF header (the review's N3). */
            o.flags |= c->flags & (u32)(SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
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
    /*  .stack and .sysmem are the linker's: nothing contributes to them, and their size is
     *  --stack_size and --heap_size - but only in a link that asked for the names, which is
     *  the runtime's doing. Without one both stay at zero, as every bare probe shows. */
    if (lnk_mod >= 0) {
        for (size_t k = 0; k < mods[lnk_mod].syms.size(); k++) {
            const std::string &n = mods[lnk_mod].syms[k].name;
            int oi = -1;
            if (n == "__TI_STACK_END" || n == "__TI_STACK_SIZE") oi = out_index(".stack");
            else if (n == "__TI_SYSMEM_SIZE") oi = out_index(".sysmem");
            if (oi < 0) continue;
            outs[oi].reserve = (n == "__TI_SYSMEM_SIZE") ? cmd.heap_size : cmd.stack_size;
        }
    }

    std::vector<int> order;
    int bi = out_index(".bss");
    if (bi >= 0 && !outs[bi].parts.empty()) order.push_back(bi);
    for (size_t i = 0; i < outs.size(); i++) if ((int)i != bi) order.push_back((int)i);

    for (size_t k = 0; k < order.size(); k++) {
        OutSec &o = outs[order[k]];
        Range *r = o.run.empty() ? 0 : cmd.range(o.run);
        if (o.parts.empty() && !o.reserve) {
            /*  An empty section still has an address when it holds bytes - a fill makes it
             *  initialised - and none at all when it does not. */
            o.addr = (o.progbits && r) ? r->origin : 0;
            continue;
        }
        if (o.parts.empty()) {                       /* .stack, .sysmem: size without input */
            if (!r) r = cmd.mem.empty() ? 0 : &cmd.mem[0];
            if (!r) { err = o.name + ": no memory range for it"; return false; }
            u32 at = align_up(r->origin + r->used, o.align > 8 ? o.align : 8);
            o.addr = at; o.size = o.reserve;
            at += o.reserve;
            if (at - r->origin > r->length) { err = o.name + ": does not fit in " + r->name; return false; }
            r->used = at - r->origin;
            continue;
        }
        if (!r) {
            /*  A section this command file never names - q12's `.mybss`, from a `.usect` -
             *  is allocated after the named ones, in the first range that still has room for
             *  it. q12 puts it at 0xC0000064, after `.neardata`, in the only range there is
             *  (the review's N12). */
            u32 want = 0;                      /* what the parts will take, padding included */
            for (size_t p = 0; p < o.parts.size(); p++) {
                InSec *c = all[o.parts[p]];
                want = align_up(want, c->align) + c->size;
            }
            for (size_t m = 0; m < cmd.mem.size() && !r; m++) {
                u32 at = align_up(cmd.mem[m].origin + cmd.mem[m].used, o.align);
                if ((at - cmd.mem[m].origin) + want <= cmd.mem[m].length) r = &cmd.mem[m];
            }
            if (!r) { err = o.name + ": no memory range for it"; return false; }
            o.run = o.load = r->name;
        }
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
    set_linker_symbols();

    std::map<std::string, std::pair<int, int> >::iterator e = defined.find(opt.entry);
    if (!sym_addr(e->second.first, e->second.second, entry_addr)) return false;
    return true;
}

bool Link::sym_addr(int mod, int sym, u32 &a)
{
    const Sym &y = mods[mod].syms[sym];
    /* the same rule as in eliminate: a name that is not the object's own is the table's */
    if ((y.info >> 4) != STB_LOCAL && !y.name.empty()) {
        std::map<std::string, std::pair<int, int> >::iterator d = defined.find(y.name);
        if (d != defined.end() &&
            (d->second.first != mod || d->second.second != sym))
            return sym_addr(d->second.first, d->second.second, a);
    }
    if (y.shndx == SHN_ABS) { a = y.value; return true; }
    if (y.shndx == SHN_UNDEF) {
        if ((y.info >> 4) == STB_WEAK) { a = 0; return true; }
        err = "unresolved symbol: " + y.name; return false;
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
