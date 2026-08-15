#include "jau/jau.hpp"
#include <filesystem>
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

int main(int argc, char** argv) {
    std::vector<std::string> embedded_args;
    for (int i = 1; i < argc; ++i) embedded_args.push_back(argv[i]);

    // argv[0] may contain only "jaupm" when the standalone executable is
    // launched through PATH. Resolve the real process image before reading the
    // embedded JAUEXEC payload.
    auto embedded = jau::run_embedded_executable(current_executable_path(argv[0]), embedded_args);
    if (embedded.ok) return 0;
    if (embedded.message != "no embedded Jau payload") {
        std::cerr << "jur: " << embedded.message << "\n";
        return 1;
    }

    if (argc < 2) {
        std::cout << "jur " << jau::version() << "\nusage: jur <file.jbc> [-- args...]\n";
        return 0;
    }

    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--") continue;
        args.push_back(argv[i]);
    }
    auto r = jau::run_bytecode(argv[1], args);
    if (!r.ok) {
        std::cerr << "jur: " << r.message << "\n";
        return 1;
    }
    return 0;
}
