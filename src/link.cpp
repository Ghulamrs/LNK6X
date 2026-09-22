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
    if (y.shndx == SHN_ABS || is_common(y.shndx)) return true;
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
        /*  **COMMON is a request for storage, not a definition**, so a real one replaces
         *  it and two of them are not a redefinition. in_image counts SHN_COMMON as a
         *  definition - which is what puts it in this table at all - and without this the
         *  linker's own .bss allocation of parmbuf read as a second definition of it, and
         *  take_module gave up there. Every symbol after it was then never recorded, so
         *  __TI_STACK_END and the rest of the linker's own names went missing. */
        if (is_common(had.shndx) && !is_common(y.shndx)) {
            d->second = std::make_pair(mi, (int)k);
            continue;
        }
        if (is_common(y.shndx)) continue;
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
        /*  **The decompressors the cinit table names.** Nothing in the program calls them -
         *  the startup code reaches them through the handler table this linker writes - so
         *  without asking for them here they are never pulled out of the runtime, and the
         *  table would point at nothing. */
        if (opt.rom_model) {
            static const char *const kHandlers[] = { "__TI_decompress_rle24",
                                                     "__TI_decompress_none", 0 };
            for (int h = 0; kHandlers[h]; h++)
                if (defined.find(kHandlers[h]) == defined.end() && !asked[kHandlers[h]]) {
                    want.push_back(kHandlers[h]); asked[kHandlers[h]] = true;
                }
        }
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
            if (!is_common(y.shndx) || y.name.empty()) continue;
            /*  **A real definition takes precedence, and COMMON is not one.** in_image
             *  counts SHN_COMMON as a definition, so take_module has already put every
             *  one of these in `defined` - skipping on that alone left the runtime's
             *  parmbuf with no storage, and sym_addr then refused it as "names no
             *  section". Only a definition that has a section wins here. */
            std::map<std::string, std::pair<int, int> >::const_iterator d =
                defined.find(y.name);
            if (d != defined.end() &&
                !is_common(mods[d->second.first].syms[d->second.second].shndx))
                continue;
            std::map<std::string, std::pair<u32, u32> >::iterator it = common.find(y.name);
            u32 al = common_align(y.shndx, y.value);
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
        null_sec.live = false; null_sec.dropped = false;
        null_sec.out = -1; null_sec.addr = 0; null_sec.load = 0;
        m.secs.push_back(null_sec);
        InSec bss;
        bss.name = ".bss"; bss.type = SHT_NOBITS; bss.flags = SHF_ALLOC | SHF_WRITE;
        bss.size = 0; bss.align = 1; bss.entsize = 0; bss.module = -1; bss.index = 1;
        bss.live = false; bss.dropped = false;
        bss.out = -1; bss.addr = 0; bss.load = 0;
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
        if (opt.verbose)
            fprintf(stderr, "linker symbol %s: wanted=%d defined=%d\n", table[i].name,
                    (int)wanted[table[i].name],
                    (int)(defined.find(table[i].name) != defined.end()));
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
    /*  **Its result is checked.** It was not, and a `return false` inside it - which is
     *  how it reports a redefinition - stopped it partway through this module's symbols
     *  and left the rest unrecorded, with no message at all. */
    /*  A redefinition here would be the linker's own name against an object's, which
     *  the `always` and `wanted` guards above already rule out - but if it ever happens
     *  it is reported, not swallowed: the ignored result left every symbol after the
     *  first collision unrecorded, and said nothing. */
    if (!take_module(lnk_mod)) fprintf(stderr, "lnk6x: %s\n", err.c_str());
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

    /*  The decompressors are roots of their own under --rom_model: the handler table the
     *  linker writes is the only thing that reaches them, and elimination cannot see it. */
    if (opt.rom_model) {
        static const char *const kHandlers[] = { "__TI_decompress_rle24",
                                                 "__TI_decompress_none", 0 };
        for (int h = 0; kHandlers[h]; h++) {
            std::map<std::string, std::pair<int, int> >::iterator d = defined.find(kHandlers[h]);
            if (d == defined.end()) continue;
            Module &hm = mods[d->second.first];
            u16 hx = hm.syms[d->second.second].shndx;
            if (hx == 0 || hx >= hm.secs.size() || hm.secs[hx].live) continue;
            hm.secs[hx].live = true;
            work.push_back(std::make_pair(d->second.first, (int)hx));
        }
    }

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

namespace {

/*  **Descending size, and a tie goes to the name.** The size is q07's map, plain enough.
 *  The tie-break was not - it is neither the archive's order (46% of 171 ties, which is
 *  chance) nor the module summary's - and it turned out to be the section's own name,
 *  ascending, with one wrinkle: a bare `.text` sorts *after* every `.text:something`,
 *  and the same for `.fardata`. 257 of 257 ties across four maps agree, the only
 *  exclusions being `.c6xabi.exidx`, which lnk6x sorts by function address instead and
 *  which this linker does not sort at all yet (docs/known.md). */
struct NameKey {
    bool bare;
    const std::string *name;
    explicit NameKey(const std::string &n) : bare(n.find(':') == std::string::npos), name(&n) {}
    bool operator<(const NameKey &o) const {
        if (bare != o.bare) return !bare;          /* a subsection comes first */
        return *name < *o.name;
    }
};

/*  The same question for whole output sections: bigger first, a tie by name. */
struct BiggerOut {
    const std::vector<OutSec> &outs;
    explicit BiggerOut(const std::vector<OutSec> &o) : outs(o) {}
    bool operator()(const std::pair<u32, int> &x, const std::pair<u32, int> &y) const {
        if (x.first != y.first) return x.first < y.first;    /* ~size: smaller ~ is bigger */
        return outs[x.second].name < outs[y.second].name;
    }
};

struct BiggerPart {
    const std::vector<InSec *> &all;
    explicit BiggerPart(const std::vector<InSec *> &a) : all(a) {}
    bool operator()(int x, int y) const {
        if (all[x]->size != all[y]->size) return all[x]->size > all[y]->size;
        return NameKey(all[x]->name) < NameKey(all[y]->name);
    }
};
} // namespace

/* ------------------------------------------------------------- allocation */

bool Link::allocate()
{
    /*  **Every range starts empty.** allocate() is run twice under --rom_model - once to
     *  learn the sizes compose_cinit needs, and again once .cinit has its own - and `used`
     *  accumulates, so without this the second pass would lay everything after the first. */
    for (size_t i = 0; i < cmd.mem.size(); i++) cmd.mem[i].used = 0;
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

    /*  **The order output sections are allocated in is descending size**, not the order
     *  the command file names them. q07's run is .stack 0x4000, .text 0x3FC0, .sysmem
     *  0x1000, .const 0x26A, .c6xabi.extab 0x7C, .fardata 0x20, .switch 0x14, and q18's
     *  is the same with .data 0x80 in its place.
     *
     *  Two are held back to the end whatever their size, in the order the file names
     *  them: `.cinit` and `.c6xabi.exidx`. Both are tables that describe the rest of the
     *  image - the load images carry run addresses, the index is sorted by function
     *  address - so a linker that placed them among the others would be deciding their
     *  contents from their own position. q07 puts .cinit at 0x30 and .exidx at 0x148
     *  after .switch at 0x14, which no size rule explains and this one does.
     *
     *  `.bss` still goes first: __TI_STATIC_BASE points at it, which q03 shows. */
    std::vector<int> order;
    int bi = out_index(".bss");
    if (bi >= 0 && !outs[bi].parts.empty()) order.push_back(bi);

    std::vector<std::pair<u32, int> > rest;      /* -size, so a sort puts the big first */
    std::vector<int> held, unnamed;
    for (size_t i = 0; i < outs.size(); i++) {
        if ((int)i == bi) continue;
        /*  A section the command file never names comes after every one it does, whatever
         *  its size - q12's `.mybss` is 0x20 and still follows `.neardata` at 4 (N12). */
        if (outs[i].run.empty()) { unnamed.push_back((int)i); continue; }
        if (outs[i].name == ".cinit" || outs[i].name == ".c6xabi.exidx") { held.push_back((int)i); continue; }
        u32 want = outs[i].reserve;
        for (size_t p = 0; p < outs[i].parts.size(); p++) {
            InSec *c = all[outs[i].parts[p]];
            want = align_up(want, c->align) + c->size;
        }
        rest.push_back(std::make_pair(~want, (int)i));   /* ~ rather than -, for unsigned */
    }
    /*  **A tie between two output sections goes to the name, ascending** - q19 has .const
     *  and .text both 0x40 and lnk6x puts .const first. It cannot be the command file's
     *  order: q19 is linked twice, from flat.cmd and from order.cmd, which name the six
     *  sections differently, and lnk6x lays them out identically both times. */
    std::stable_sort(rest.begin(), rest.end(), BiggerOut(outs));
    for (size_t i = 0; i < rest.size(); i++) order.push_back(rest[i].second);
    for (size_t i = 0; i < held.size(); i++) order.push_back(held[i]);
    for (size_t i = 0; i < unnamed.size(); i++) order.push_back(unnamed[i]);

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
        /*  **lnk6x places a section's contributions in descending size**, not in the order
         *  they were read - q07's `.text` runs 0x640, 0x580, 0x4C0, 0x440, ... for the
         *  whole of the run, and its map is the evidence. A tie goes to the name - see
         *  BiggerPart, which is where the reading of it is written down. */
        std::stable_sort(o.parts.begin(), o.parts.end(), BiggerPart(all));
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

/* ------------------------------------------------------------- .cinit, --rom_model */

namespace {

/*  **lnk6x's escape byte is the smallest value the data does not contain**, which both
 *  oracle images agree on: 0x00 for q05, whose four bytes are 44 33 22 11, and 0x40 for
 *  q18, whose bytes are 0x5A and 0x00 through 0x3F. A byte that never occurs can introduce
 *  a run without ever having to be escaped itself. */
u8 escape_for(const std::vector<u8> &d)
{
    bool seen[256];
    for (int i = 0; i < 256; i++) seen[i] = false;
    for (size_t i = 0; i < d.size(); i++) seen[d[i]] = true;
    for (int i = 0; i < 256; i++) if (!seen[i]) return (u8)i;
    return 0;                         /* every value occurs: no escape is possible */
}

/*  The rle24 stream `__TI_decompress_rle_core` reads: the escape byte, then bytes that are
 *  not it stored literally and ones that are it introducing a run of (count, value). A
 *  count of zero ends the stream - q05 and q18 both finish that way.
 *
 *  A run costs three bytes and saves one per byte beyond that, so four identical bytes are
 *  where it starts to pay; below that literals are shorter. q18 is the evidence such as it
 *  is - sixty-four identical bytes run-encoded, sixty-four varied ones left alone. */
void rle24_encode(const std::vector<u8> &d, u8 E, std::vector<u8> &out)
{
    out.push_back(E);
    size_t i = 0;
    while (i < d.size()) {
        size_t j = i;
        while (j < d.size() && d[j] == d[i] && j - i < 255) j++;
        size_t run = j - i;
        if (run >= 4) {
            out.push_back(E);
            out.push_back((u8)run);
            out.push_back(d[i]);
            i = j;
            continue;
        }
        /*  A literal that happens to equal the escape has to be written as a run of one,
         *  which is why an escape the data does not contain is worth choosing. */
        if (d[i] == E) { out.push_back(E); out.push_back(1); out.push_back(d[i]); i++; continue; }
        out.push_back(d[i]);
        i++;
    }
    /*  **The terminator is the long form with a length of nothing**: the escape and three
     *  zero bytes, four in all, not two. q18 says so - its first image runs 73 bytes, and
     *  the index, the escape, one run of three and sixty-four literals account for 69. */
    out.push_back(E);
    out.push_back(0);
    out.push_back(0);
    out.push_back(0);
}

} // namespace

/*  **The load images, and room for the table that drives them.** Under --rom_model an
 *  initialised writable section is not in the image at its run address: its bytes go into
 *  .cinit as a load image and the section itself becomes SHT_NOBITS, which is what q05's
 *  `.data` is. This runs before allocation because .cinit's size moves everything after it.
 */
bool Link::compose_cinit()
{
    cinit_recs.clear();
    cinit_handlers.clear();
    std::vector<u8> image;

    for (size_t oi = 0; oi < outs.size(); oi++) {
        OutSec &o = outs[oi];
        if (!(o.flags & SHF_ALLOC) || !(o.flags & SHF_WRITE)) continue;
        if (o.type != SHT_PROGBITS || o.size == 0) continue;
        if (o.name == ".cinit") continue;

        /*  The section's bytes, laid out as they will be at run time. A hole between two
         *  contributions is zero, as it is in the image. */
        std::vector<u8> d(o.size, 0);
        for (size_t k = 0; k < o.parts.size(); k++) {
            InSec *c = all[o.parts[k]];
            if (c->data.empty()) continue;
            u32 at = c->addr - o.addr;
            if (at + c->data.size() > d.size()) { err = o.name + ": a part lands outside it"; return false; }
            memcpy(&d[at], &c->data[0], c->data.size());
        }

        CinitRec r;
        r.image = (u32)image.size();
        r.out = (int)oi;
        cinit_recs.push_back(r);

        image.push_back(0);                       /* the handler index, rle24 */
        rle24_encode(d, escape_for(d), image);

        o.type = SHT_NOBITS;                      /* its bytes live in .cinit now */
        o.progbits = false;
    }
    if (cinit_recs.empty()) return true;

    /*  Both samples list the two the runtime has, rle24 first, whether or not `none` is
     *  used - so the index of rle24 is 0 and the table is two words. */
    cinit_handlers.push_back("__TI_decompress_rle24");
    cinit_handlers.push_back("__TI_decompress_none");

    /*  The images are packed with no padding between them - q18's second begins at 0x49,
     *  which is odd - and only the table is aligned, to four. The records are aligned to
     *  eight, which is what makes __TI_CINIT_Base land on an eight-byte boundary. */
    while (image.size() % 4) image.push_back(0);
    cinit_table_off = (u32)image.size();
    image.resize(image.size() + 4 * cinit_handlers.size(), 0);
    while (image.size() % 8) image.push_back(0);
    cinit_recs_off = (u32)image.size();
    image.resize(image.size() + 8 * cinit_recs.size(), 0);

    /*  A contribution of the linker's own, so write_image copies it like any other. */
    InSec *c = new InSec();
    c->name = ".cinit";
    c->type = SHT_PROGBITS;
    c->flags = SHF_ALLOC;
    c->size = (u32)image.size();
    c->align = 8;
    c->entsize = 0;
    c->module = lnk_mod;
    c->index = 0;
    c->live = true;
    c->dropped = false;
    c->out = -1;
    c->addr = 0;
    c->load = 0;
    c->data.swap(image);
    all.push_back(c);
    cinit_in = (int)all.size() - 1;

    /*  It has to join the output section like any other contribution, or allocate() will
     *  not place it and write_image() will not copy it. `.cinit` is in the section table
     *  build_sections works from, so it exists; it is simply empty until now. */
    int oi = out_index(".cinit");
    if (oi < 0) {
        OutSec o;
        o.name = ".cinit";
        o.run = outs.empty() ? std::string() : outs[0].run;
        outs.push_back(o);
        oi = (int)outs.size() - 1;
    }
    OutSec &o = outs[oi];
    o.parts.push_back(cinit_in);
    c->out = oi;                  /* write_image copies by this, and skips a -1 */
    o.progbits = true;
    o.type = SHT_PROGBITS;
    o.flags |= SHF_ALLOC;
    o.pflags |= PF_R;
    if (o.align < 8) o.align = 8;
    return true;
}

/*  **The addresses, once there are any.** The handler table's pointers, each record's
 *  {load, run}, and the four names that bracket the two - which are not the section's own
 *  bounds, so set_linker_symbols' rule for them is overridden here. */
void Link::place_cinit()
{
    if (cinit_in < 0) return;
    InSec *c = all[cinit_in];
    std::vector<u8> &d = c->data;
    const u32 base = c->addr;

    for (size_t i = 0; i < cinit_handlers.size(); i++) {
        u32 a = 0;
        std::map<std::string, std::pair<int, int> >::const_iterator it =
            defined.find(cinit_handlers[i]);
        if (it != defined.end()) sym_addr(it->second.first, it->second.second, a);
        wr32(&d[cinit_table_off + 4 * i], a);
    }
    for (size_t i = 0; i < cinit_recs.size(); i++) {
        wr32(&d[cinit_recs_off + 8 * i],     base + cinit_recs[i].image);
        wr32(&d[cinit_recs_off + 8 * i + 4], outs[cinit_recs[i].out].addr);
    }
    if (lnk_mod < 0) return;
    Module &m = mods[lnk_mod];
    const int oi = out_index(".cinit");
    for (size_t k = 0; k < m.syms.size(); k++) {
        Sym &y = m.syms[k];
        u32 v = 0;
        if (y.name == "__TI_Handler_Table_Base")       v = base + cinit_table_off;
        else if (y.name == "__TI_Handler_Table_Limit") v = base + cinit_table_off + 4 * (u32)cinit_handlers.size();
        else if (y.name == "__TI_CINIT_Base")          v = base + cinit_recs_off;
        else if (y.name == "__TI_CINIT_Limit")         v = base + cinit_recs_off + 8 * (u32)cinit_recs.size();
        else continue;
        y.value = v;
        y.lnk_out = oi;
    }
}
