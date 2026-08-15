#include "jau/jau.hpp"
#include <iostream>
#include <vector>
#include <string>

int main(int argc, char** argv) {
    std::vector<std::string> embedded_args;
    for (int i = 1; i < argc; ++i) embedded_args.push_back(argv[i]);
    auto embedded = jau::run_embedded_executable(argv[0], embedded_args);
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
    if (!r.ok) { std::cerr << "jur: " << r.message << "\n"; return 1; }
    return 0;
}
