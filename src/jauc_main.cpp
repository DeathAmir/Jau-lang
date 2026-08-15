#include "jau/jau.hpp"
#include <iostream>
#include <string>

static void help() {
    std::cout << "Jau compiler " << jau::version() << "\n"
              << "usage:\n"
              << "  jauc build <file.jau> [-o out.jbc] [-O0|-O1|-O2]\n"
              << "  jauc run <file.jau>\n"
              << "  jauc asm <file.jau> -o out.s --target <linux-x86_64|linux-x86|windows-x86_64|windows-x86>\n"
              << "  jauc native <file.jau> -o program --target <target>\n"
              << "  jauc --version\n";
}
int main(int argc, char** argv) {
    if (argc < 2) { help(); return 0; }
    std::string cmd = argv[1];
    if (cmd == "--version" || cmd == "version") { std::cout << jau::version() << "\n"; return 0; }
    if (argc < 3) { help(); return 2; }
    std::string input = argv[2], output, target;
    jau::CompileOptions opt;
    for (int i = 3; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) output = argv[++i];
        else if (a == "--target" && i + 1 < argc) target = argv[++i];
        else if (a == "-I" && i + 1 < argc) opt.import_paths.push_back(argv[++i]);
        else if (a.rfind("-O", 0) == 0 && a.size() == 3) opt.optimize = a[2] - '0';
    }
    jau::Result r;
    if (cmd == "run") r = jau::run_source(input, opt);
    else if (cmd == "build") { if (output.empty()) output = input.substr(0, input.find_last_of('.')) + ".jbc"; r = jau::compile_file(input, output, opt); }
    else if (cmd == "asm") { if (output.empty()) output = "out.s"; if (target.empty()) target = "linux-x86_64"; r = jau::emit_assembly(input, output, target, opt); }
    else if (cmd == "native") { if (output.empty()) output = "a.out"; if (target.empty()) target = "linux-x86_64"; r = jau::native_build(input, output, target, opt); }
    else { help(); return 2; }
    if (!r.ok) { std::cerr << "jauc: " << r.message << "\n"; return 1; }
    if (!r.message.empty()) std::cout << r.message << "\n";
    return 0;
}
