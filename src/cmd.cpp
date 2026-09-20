/*  cmd.cpp - the linker command file.
 *
 *  TI's language, not cmd.exe's, and only the part the bed uses: option lines that begin with
 *  two dashes, a MEMORY block of named ranges, and a SECTIONS block in which each entry says
 *  where its section is loaded, where it is run if that is somewhere else, what it must be
 *  aligned to, and what to write into the gaps. The order of the SECTIONS block is not
 *  decoration: it is the order the output sections appear in the image.
 */
#include "lnk.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct Lex {
    std::string s;
    size_t i;
    std::string err;
    Lex(const std::string &t) : s(t), i(0) {}

    void skip() {
        for (;;) {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
            if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
                i += 2;
                while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) i++;
                i = (i + 2 < s.size()) ? i + 2 : s.size();
                continue;
            }
            if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') {
                while (i < s.size() && s[i] != '\n') i++;
                continue;
            }
            break;
        }
    }
    /* a token is a run of the characters a name, a number or an option can be made of,
       or one punctuation mark */
    std::string next() {
        skip();
        if (i >= s.size()) return std::string();
        char c = s[i];
        if (strchr("{}:,=><()", c)) { i++; return std::string(1, c); }
        size_t b = i;
        while (i < s.size() && !strchr(" \t\r\n{}:,=><()", s[i])) i++;
        return s.substr(b, i - b);
    }
    std::string peek() { size_t k = i; std::string t = next(); i = k; return t; }
};

u32 number(const std::string &t) { return (u32)strtoul(t.c_str(), 0, 0); }

/*  The body of a `{ ... }` input-section list, the opening brace already taken. Every
 *  `*(name)` - and a bare `name` - is recorded in the order it appears, because that order
 *  is the order lnk6x lays the parts down in. Anything else inside is stepped over. */
void input_list(Lex &lx, SecSpec &sp)
{
    int depth = 1;
    std::string prev;
    while (depth) {
        std::string k = lx.next();
        if (k.empty()) break;
        if (k == "{") { depth++; prev.clear(); continue; }
        if (k == "}") { depth--; prev.clear(); continue; }
        if (k == "(") {
            /*  The lexer breaks on `:`, so `.text:early` arrives as three tokens; the name is
             *  whatever stands between the parentheses, put back together. */
            std::string inner;
            for (;;) {
                std::string t = lx.next();
                if (t.empty() || t == ")") break;
                inner += t;
            }
            if (!inner.empty()) sp.inputs.push_back(inner);
            prev.clear();
            continue;
        }
        prev = k;
    }
}

} /* namespace */

bool sec_matches(const std::string &pattern, const std::string &name)
{
    if (!pattern.empty() && pattern[pattern.size() - 1] == '*')
        return name.compare(0, pattern.size() - 1, pattern, 0, pattern.size() - 1) == 0;
    return pattern == name;
}

std::string base_section(const std::string &name)
{
    size_t c = name.find(':');
    return (c == std::string::npos) ? name : name.substr(0, c);
}

Range *Cmd::range(const std::string &name)
{
    for (size_t i = 0; i < mem.size(); i++) if (mem[i].name == name) return &mem[i];
    return 0;
}

bool Cmd::parse(const std::string &path, std::string &err)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) { err = path + ": cannot open"; return false; }
    std::string text;
    char buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
    fclose(f);

    Lex lx(text);
    for (;;) {
        std::string t = lx.next();
        if (t.empty()) break;

        if (t.size() > 2 && t[0] == '-' && t[1] == '-') {
            size_t eq = t.find('=');
            std::string k = (eq == std::string::npos) ? t : t.substr(0, eq);
            std::string v = (eq == std::string::npos) ? std::string() : t.substr(eq + 1);
            if (v.empty() && lx.peek() == "=") { lx.next(); v = lx.next(); }
            if (k == "--stack_size") stack_size = number(v);
            else if (k == "--heap_size") heap_size = number(v);
            /* other options are the command line's business, not this file's */
            continue;
        }

        if (t == "MEMORY") {
            if (lx.next() != "{") { err = path + ": MEMORY without a brace"; return false; }
            for (;;) {
                std::string name = lx.next();
                if (name == "}" || name.empty()) break;
                Range r; r.name = name; r.origin = r.length = r.used = 0;
                if (lx.next() != ":") { err = path + ": " + name + ": expected a colon"; return false; }
                for (;;) {
                    std::string k = lx.peek();
                    if (k == "origin" || k == "o" || k == "ORIGIN" || k == "O") {
                        lx.next(); if (lx.peek() == "=") lx.next();
                        r.origin = number(lx.next());
                    } else if (k == "length" || k == "l" || k == "LENGTH" || k == "L" || k == "len") {
                        lx.next(); if (lx.peek() == "=") lx.next();
                        r.length = number(lx.next());
                    } else if (k == ",") {
                        lx.next();
                    } else break;
                }
                mem.push_back(r);
            }
            continue;
        }

        if (t == "SECTIONS") {
            if (lx.next() != "{") { err = path + ": SECTIONS without a brace"; return false; }
            for (;;) {
                std::string name = lx.next();
                if (name == "}" || name.empty()) break;
                SecSpec sp;
                sp.name = name;
                sp.align = 0; sp.has_align = false;
                sp.fill = 0;  sp.has_fill = false;
                std::string t2 = lx.next();
                if (t2 == ">") {                       /* .name > RANGE */
                    sp.load = lx.next();
                } else if (t2 == ":") {                /* .name : load = R, run = R, align = n */
                    for (;;) {
                        std::string k = lx.peek();
                        if (k == "," ) { lx.next(); continue; }
                        if (k == ">") { lx.next(); sp.load = lx.next(); continue; }
                        /*  `.text : { *(.text:early) *(.text) } > RAM`. The list used to fall
                         *  through to the break below, leaving the braces for the outer loop
                         *  to read as section names and this entry with no range at all - the
                         *  review's N6, which stopped q09-subsect-named. */
                        if (k == "{") { lx.next(); input_list(lx, sp); continue; }
                        if (k == "load" || k == "LOAD") {
                            lx.next(); if (lx.peek() == "=" || lx.peek() == ">") lx.next();
                            sp.load = lx.next(); continue;
                        }
                        if (k == "run" || k == "RUN") {
                            lx.next(); if (lx.peek() == "=" || lx.peek() == ">") lx.next();
                            sp.run = lx.next(); continue;
                        }
                        if (k == "align" || k == "ALIGN") {
                            lx.next(); if (lx.peek() == "=") lx.next();
                            if (lx.peek() == "(") lx.next();
                            sp.align = number(lx.next()); sp.has_align = true;
                            if (lx.peek() == ")") lx.next();
                            continue;
                        }
                        if (k == "fill" || k == "FILL") {
                            lx.next(); if (lx.peek() == "=") lx.next();
                            if (lx.peek() == "(") lx.next();
                            sp.fill = number(lx.next()); sp.has_fill = true;
                            if (lx.peek() == ")") lx.next();
                            continue;
                        }
                        break;
                    }
                } else if (t2 == "{") {
                    input_list(lx, sp);
                    if (lx.peek() == ">") { lx.next(); sp.load = lx.next(); }
                }
                secs.push_back(sp);
            }
            continue;
        }
        /* anything else in the file is not something the bed asks for */
    }
    if (mem.empty()) { err = path + ": no MEMORY block"; return false; }
    return true;
}
