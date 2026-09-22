/*  main.cpp - the command line, in lnk6x's spelling, and the order of the passes.
 *
 *  Only what the bed uses is acted on. -mv, --abi and -i are accepted and ignored: this
 *  linker is the C674x's and EABI's, and has no library path to search until it reads
 *  archives at all.
 */
#include "lnk.h"
#include <cstdio>
#include <cstring>

bool Link::run()
{
    if (!cmd.parse(opt.cmdfile, err)) return false;
    /*  What the command file said, where the command line did not say it itself. RIDE writes
     *  the model into the file and passes none on the line. Which of the two wins when both
     *  speak is not something the bed shows; the line is taken to, as the nearer word. */
    if (!opt.model_given && cmd.model) {
        opt.rom_model = (cmd.model == 2);
        opt.ram_model = !opt.rom_model;
    }
    if (!opt.entry_given && !cmd.entry.empty()) opt.entry = cmd.entry;
    if (!read_inputs())    return false;
    if (!eliminate())      return false;
    if (!build_sections()) return false;
    if (!allocate())       return false;
    if (!fix_up())         return false;
    /*  **--rom_model runs the middle of the link twice, and it has to.** A load image is
     *  the section's bytes *after* relocation - .fardata is full of pointers - so the
     *  images cannot be made until fix_up has run; but .cinit's size moves everything
     *  after it, so once they are made the addresses are wrong and it must all be done
     *  again. Relocation is idempotent here: every type writes its field rather than
     *  adding to it, so the second pass computes the same words from the new addresses. */
    if (opt.rom_model) {
        if (!compose_cinit()) return false;
        if (cinit_in >= 0) {
            if (!allocate()) return false;
            if (!fix_up())   return false;
            place_cinit();
        }
    }
    return write_image();
}

static bool starts(const std::string &a, const char *p) { return a.compare(0, strlen(p), p) == 0; }

int main(int argc, char **argv)
{
    Link lk;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc)      { lk.opt.out = argv[++i]; continue; }
        if (starts(a, "--output_file="))    { lk.opt.out = a.substr(14); continue; }
        if (a == "-m" && i + 1 < argc)      { lk.opt.map = argv[++i]; continue; }
        if (a == "-i" && i + 1 < argc)      { lk.opt.libdirs.push_back(argv[++i]); continue; }
        if (starts(a, "-i"))                { lk.opt.libdirs.push_back(a.substr(2)); continue; }
        if (a == "-e" && i + 1 < argc)      { lk.opt.entry = argv[++i]; lk.opt.entry_given = true; continue; }
        if (starts(a, "--entry_point="))    { lk.opt.entry = a.substr(14); lk.opt.entry_given = true; continue; }
        if (a == "--ram_model")             { lk.opt.ram_model = true;  lk.opt.rom_model = false; lk.opt.model_given = true; continue; }
        if (a == "--rom_model")             { lk.opt.rom_model = true;  lk.opt.ram_model = false; lk.opt.model_given = true; continue; }
        if (a == "--verbose" || a == "-v")  { lk.opt.verbose = true; continue; }
        if (starts(a, "-mv") || starts(a, "--abi=") || a == "-c" || a == "-cr"
            || a == "--no_compress" || starts(a, "--diag")) continue;
        if (a == "-l" && i + 1 < argc)      { lk.opt.inputs.push_back(argv[++i]); continue; }
        if (starts(a, "-l"))                { lk.opt.inputs.push_back(a.substr(2)); continue; }
        if (a.size() > 1 && a[0] == '-')    { fprintf(stderr, "lnk6x: unknown option %s\n", argv[i]); return 2; }
        if (a.size() > 4 && a.compare(a.size() - 4, 4, ".cmd") == 0) { lk.opt.cmdfile = a; continue; }
        lk.opt.inputs.push_back(a);
    }
    if (lk.opt.cmdfile.empty() || lk.opt.inputs.empty()) {
        fprintf(stderr, "usage: lnk6x [-mv6740] [--abi=eabi] <command file> object...\n"
                        "             [--ram_model] [-e sym] -o image.out [-m image.map]\n");
        return 2;
    }
    if (lk.opt.out.empty()) lk.opt.out = "a.out";

    if (!lk.run()) { fprintf(stderr, "lnk6x: %s\n", lk.err.c_str()); return 1; }
    if (lk.opt.verbose)
        for (size_t i = 0; i < lk.outs.size(); i++)
            if (lk.outs[i].size)
                printf("%-16s %08x  %6x bytes at file %6x\n", lk.outs[i].name.c_str(),
                       lk.outs[i].addr, lk.outs[i].size, lk.outs[i].offset);
    return 0;
}
