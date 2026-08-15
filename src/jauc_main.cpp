#include "jau/jau.hpp"
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string current_executable_path(const char* argv0) {
#ifdef _WIN32
    std::vector<char> buf(32768);
    DWORD n = GetModuleFileNameA(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n > 0 && n < buf.size()) return std::string(buf.data(), n);
#else
    std::vector<char> buf(4096);
    ssize_t n = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (n > 0) return std::string(buf.data(), static_cast<size_t>(n));
#endif
    std::error_code ec;
    auto p = fs::absolute(argv0 ? argv0 : "", ec);
    return ec ? std::string(argv0 ? argv0 : "") : p.string();
}

static std::string quote(const std::string& x) {
#ifdef _WIN32
    std::string r = "\"";
    for (char c : x) {
        if (c == '"') r += "\\\"";
        else r += c;
    }
    return r + "\"";
#else
    std::string r = "'";
    for (char c : x) {
        if (c == '\'') r += "'\\''";
        else r += c;
    }
    return r + "'";
#endif
}

static bool is_windows_target(const std::string& target) {
    return target.rfind("windows-", 0) == 0;
}

static void help() {
    std::cout << "Jau compiler " << jau::version() << "\n"
              << "usage:\n"
              << "  jauc build <file.jau> [-o out.jbc] [-O0|-O1|-O2|-O3]\n"
              << "  jauc run <file.jau>\n"
              << "  jauc asm <file.jau> -o out.s --target <linux-x86_64|linux-x86|windows-x86_64|windows-x86> [--library]\n"
              << "  jauc obj <file.jau> -o out.o|out.obj --target <target>\n"
              << "  jauc native <file.jau> -o program --target <target>\n"
              << "  jauc standalone <file.jau> -o program [--runtime path/to/jur]\n"
              << "  jauc --version\n\n"
              << "AOT interoperability:\n"
              << "  extern func c_function(a, b);   // call C ABI code from Jau obj/asm/native\n"
              << "  Jau functions are exported as jau_fn_<name> for C/C++ callers.\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { help(); return 0; }
    std::string cmd = argv[1];
    if (cmd == "--version" || cmd == "version") {
        std::cout << jau::version() << "\n";
        return 0;
    }
    if (argc < 3) { help(); return 2; }

    std::string input = argv[2], output, target, runtime;
    std::vector<std::string> runargs;
    jau::CompileOptions opt;

    for (int i = 3; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--") {
            for (++i; i < argc; ++i) runargs.push_back(argv[i]);
            break;
        }
        if (a == "-o" && i + 1 < argc) output = argv[++i];
        else if (a == "--target" && i + 1 < argc) target = argv[++i];
        else if (a == "--runtime" && i + 1 < argc) runtime = argv[++i];
        else if (a == "--library") opt.library_mode = true;
        else if (a == "-I" && i + 1 < argc) opt.import_paths.push_back(argv[++i]);
        else if (a.rfind("-O", 0) == 0 && a.size() == 3) opt.optimize = a[2] - '0';
    }

    jau::Result r;
    if (cmd == "run") {
        r = jau::run_source(input, opt, runargs);
    } else if (cmd == "build") {
        if (output.empty()) output = input.substr(0, input.find_last_of('.')) + ".jbc";
        r = jau::compile_file(input, output, opt);
    } else if (cmd == "asm") {
        if (output.empty()) output = "out.s";
        if (target.empty()) target = "linux-x86_64";
        r = jau::emit_assembly(input, output, target, opt);
    } else if (cmd == "obj") {
        if (target.empty()) target = "linux-x86_64";
        if (output.empty()) output = is_windows_target(target) ? "out.obj" : "out.o";
        opt.library_mode = true;
        auto temp = fs::temp_directory_path() / ("jau_obj_" + std::to_string(std::hash<std::string>{}(input + output)) + ".s");
        r = jau::emit_assembly(input, temp.string(), target, opt);
        if (r.ok) {
            fs::path self = current_executable_path(argv[0]);
            fs::path assembler = self.parent_path() /
#ifdef _WIN32
                "jauas.exe";
#else
                "jauas";
#endif
            std::string command = quote(assembler.string()) + " " + quote(temp.string()) + " -o " + quote(output) +
                                  " --target " + quote(target) + " --object";
            int rc = std::system(command.c_str());
            std::error_code ec; fs::remove(temp, ec);
            if (rc != 0) r = {false, "internal jauas object build failed"};
            else r = {true, "object built: " + output + " (" + target + ", C ABI)"};
        }
    } else if (cmd == "native") {
        if (output.empty()) output = "a.out";
        if (target.empty()) target = "linux-x86_64";
        r = jau::native_build(input, output, target, opt);
    } else if (cmd == "standalone") {
        if (output.empty()) output = "jau-app";
        if (runtime.empty()) {
            fs::path self = current_executable_path(argv[0]);
            runtime = (self.parent_path() /
#ifdef _WIN32
                "jur.exe"
#else
                "jur"
#endif
            ).string();
        }
        r = jau::bundle_executable(input, output, runtime, opt);
    } else {
        help();
        return 2;
    }

    if (!r.ok) {
        std::cerr << "jauc: " << r.message << "\n";
        return 1;
    }
    if (!r.message.empty()) std::cout << r.message << "\n";
    return 0;
}
