#include "Driver.h"

#include <cstring>
#include <iostream>
#include <string>

static void usage() {
    std::cerr <<
        "usage: elf-linker [options] input.o [input.o ...]\n"
        "  -o FILE        output path (default: a.out)\n"
        "  -e NAME        entry symbol (default: _start)\n"
        "  --dump         parse inputs and print ELF summary; produce no output\n"
        "  --keep-metadata preserve .note/.comment sections\n"
        "  -v / --verbose verbose logging\n"
        "  -h / --help    this help\n";
}

int main(int argc, char** argv) {
    lnk::Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) { std::cerr << flag << " needs an argument\n"; std::exit(2); }
            return argv[++i];
        };
        if (a == "-o")            opt.output = need("-o");
        else if (a == "-e")       opt.entry  = need("-e");
        else if (a == "--dump")   opt.dump_only = true;
        else if (a == "--keep-metadata") opt.keep_metadata = true;
        else if (a == "-v" || a == "--verbose") opt.verbose = true;
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "unknown flag: " << a << "\n"; usage(); return 2;
        } else {
            opt.inputs.push_back(a);
        }
    }
    if (opt.inputs.empty()) { usage(); return 2; }
    return lnk::run(opt);
}
