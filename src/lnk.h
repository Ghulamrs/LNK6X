/*  lnk.h - what the passes of this linker say to each other.
 *
 *  The shape is TI's, not the PE linker's: input sections keep their own names, the command
 *  file decides which output section each one joins and which memory range that section is
 *  cut from, and a section nothing refers to is not in the image at all. docs/elf-observed.md
 *  holds the numbers, and every rule here was read off tests/ref rather than out of a manual.
 */
#ifndef LNK_H
#define LNK_H

#include <map>
#include <string>
#include <vector>

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef signed int         i32;
typedef unsigned long long u64;

enum {
    SHT_NULL = 0, SHT_PROGBITS = 1, SHT_SYMTAB = 2, SHT_STRTAB = 3, SHT_RELA = 4,
    SHT_NOBITS = 8, SHT_REL = 9,
    SHT_C6000_ATTRIBUTES = 0x70000003u,
    SHT_TI_SECTION_FLAGS = 0x7F000005u,
    SHT_TI_SYMBOL_ALIAS  = 0x7F000006u
};
enum { SHF_WRITE = 1, SHF_ALLOC = 2, SHF_EXECINSTR = 4 };
enum { PF_X = 1, PF_W = 2, PF_R = 4 };
enum { STB_LOCAL = 0, STB_GLOBAL = 1, STB_WEAK = 2 };
enum { STT_NOTYPE = 0, STT_OBJECT = 1, STT_FUNC = 2, STT_SECTION = 3, STT_FILE = 4 };
enum { SHN_UNDEF = 0, SHN_ABS = 0xFFF1u, SHN_COMMON = 0xFFF2u };

/*  The C6000 relocations the bed produces. The numbering is the C6000 ELF ABI's; the three
 *  that appear in tests/ref are PCR_S21 for a branch, and ABS_L16 and ABS_H16 for the MVKL and
 *  MVKH that carry a 32-bit address in two halves. */
enum {
    R_C6000_NONE = 0, R_C6000_ABS32 = 1, R_C6000_ABS16 = 2, R_C6000_ABS8 = 3,
    R_C6000_PCR_S21 = 4, R_C6000_PCR_S12 = 5, R_C6000_PCR_S10 = 6, R_C6000_PCR_S7 = 7,
    R_C6000_ABS_S16 = 8, R_C6000_ABS_L16 = 9, R_C6000_ABS_H16 = 10,
    /*  Met in the runtime and in every C++ program, and not applied here: what each one
     *  computes has not been read off the oracle yet, and a relocation applied by guesswork
     *  is a wrong image written in silence. docs/known.md has the measurements so far. */
    R_C6000_PREL31 = 25, R_C6000_EHTYPE = 28, R_C6000_PCR_H16 = 29, R_C6000_PCR_L16 = 30
};

struct Rel { u32 offset; u32 sym; u32 type; i32 addend; };

struct Sym {
    std::string name;
    u32 value, size;
    u8  info, other;
    u16 shndx;
    int out;            /* index in the output symbol table, -1 while it has none */
    /*  A name the linker defines itself is absolute while it is being resolved - its value is
     *  the address - but lnk6x writes it against an output section: q07 has __TI_STACK_END in
     *  .stack and __TI_CINIT_Base in .cinit. This says which, or -1 for an ordinary symbol. */
    int lnk_out;
    Sym() : value(0), size(0), info(0), other(0), shndx(0), out(-1), lnk_out(-1) {}
};

/* one input section, from one object */
struct InSec {
    std::string name;
    u32 type, flags, size, align, entsize;
    std::vector<u8> data;        /* empty when SHT_NOBITS */
    std::vector<Rel> relocs;
    int  module;
    int  index;                  /* its section number inside that object */
    bool live;                   /* survived unused-section elimination */
    int  out;                    /* output section, -1 */
    u32  addr;                   /* run address, once allocated */
    u32  load;                   /* load address: the same unless the command file parts them */
};

struct Module {
    std::string name;            /* the file name, as the map and the symbol table show it */
    std::vector<InSec> secs;     /* secs[i] is section number i */
    std::vector<Sym>   syms;
    int file_sym;                /* the STT_FILE symbol, or -1 */
};

/* ------------------------------------------------------ the command file */

struct Range {
    std::string name;
    u32 origin, length, used;
};

struct SecSpec {
    std::string name;
    std::string load, run;       /* memory range names; run empty means the same as load */
    u32  align;  bool has_align;
    u32  fill;   bool has_fill;
    /*  The input-section list of a `.name : { *(.text:early) *(.text) } > RAM` entry, in the
     *  order it was written: that order is the order the parts are laid down in. A pattern
     *  may end in `*`. Empty when the entry has no list. */
    std::vector<std::string> inputs;
};

/*  `*(.text:early)` against `.text:early`, and `*(.text:*)` against any subsection of .text.
 *  A pattern is a section name, optionally ending in `*`. */
bool sec_matches(const std::string &pattern, const std::string &name);

/*  A subsection joins its base section: `.text:_outc` is part of `.text` unless the command
 *  file names `.text:_outc` itself. */
std::string base_section(const std::string &name);

struct Cmd {
    std::vector<Range>   mem;
    std::vector<SecSpec> secs;
    u32 stack_size, heap_size;
    /*  Options written inside the command file. RIDE's kTiLinkCmd puts --rom_model on its
     *  second line, and a linker that reads options only from its command line links RIDE's
     *  programs as --ram_model without saying so (the review's N4). 0 = the file said
     *  nothing, 1 = --ram_model, 2 = --rom_model. */
    int model;
    std::string entry;
    Cmd() : stack_size(0), heap_size(0), model(0) {}
    bool parse(const std::string &path, std::string &err);
    Range *range(const std::string &name);
};

/* ----------------------------------------------------- the output image */

struct OutSec {
    std::string name;
    u32 type, flags, addr, size, align, entsize, offset;
    u32 reserve;                 /* bytes the linker itself asks for: .stack, .sysmem */
    u32 pflags;                  /* the segment attributes, taken from the input sections */
    std::vector<int> parts;      /* indices into Link::all, in allocation order */
    bool progbits;               /* a fill makes an empty section initialised */
    std::string load, run;
    OutSec() : type(SHT_NOBITS), flags(0), addr(0), size(0), align(1), entsize(0), offset(0),
               reserve(0), pflags(0), progbits(false) {}
};

struct Seg { u32 offset, vaddr, paddr, filesz, memsz, flags, align; };

struct Options {
    std::string out, map, entry, cmdfile;
    std::vector<std::string> inputs;
    std::vector<std::string> libdirs;
    bool ram_model, rom_model, verbose;
    bool model_given, entry_given;       /* whether the command line said so itself */
    Options() : entry("_c_int00"), ram_model(true), rom_model(false), verbose(false),
                model_given(false), entry_given(false) {}
};

struct Link {
    Options opt;
    Cmd cmd;
    std::vector<Module>  mods;
    std::vector<InSec*>  all;
    std::vector<OutSec>  outs;
    std::vector<Seg>     segs;
    std::map<std::string, std::pair<int, int> > defined;   /* name -> module, symbol */
    u32 entry_addr, static_base;
    int lnk_mod;                 /* the module holding the names the linker defines, or -1 */
    std::string err;

    Link() : entry_addr(0), static_base(0), lnk_mod(-1) {}

    bool run();
    bool read_inputs();
    bool take_module(int mi);
    bool in_image(int mi, int sym) const;
    void add_linker_symbols();
    void set_linker_symbols();
    bool eliminate();
    bool build_sections();
    bool allocate();
    bool fix_up();
    bool write_image();
    bool sym_addr(int mod, int sym, u32 &a);
    int  out_index(const std::string &name) const;
};

/* elf.cpp */
bool elf_read(const u8 *p, size_t n, const std::string &name, Module &m, std::string &err);

/* archive.cpp */
struct Archive {
    std::string name;
    std::vector<u8> bytes;
    std::vector<std::pair<std::string, u32> > index;   /* symbol -> the member's offset */
    std::vector<u32> taken;                            /* members already pulled */
    size_t longnames_at;                               /* the `//` member's data, 0 when none */
    Archive() : longnames_at(0) {}
    bool load(const std::string &path, std::string &err);
    bool member(u32 off, Module &m, std::string &err) const;
};

/* where a `-l name` is looked for: as given, then each `-i` directory in order */
std::string find_library(const Options &o, const std::string &nm);

/* reloc.cpp */
bool apply_reloc(u32 type, u8 *p, u32 P, u32 S, i32 A, std::string &err);

inline u16 rd16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }
inline u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
inline void wr16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
inline void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
inline u32 align_up(u32 v, u32 a) { return (a > 1) ? ((v + a - 1) / a) * a : v; }

#endif
