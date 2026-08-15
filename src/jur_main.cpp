#include "jau/jau.hpp"
#include <iostream>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 2) { auto e=jau::run_embedded_executable(argv[0], {}); if(e.ok) return 0; std::cout << "jur " << jau::version() << "\nusage: jur <file.jbc> [-- args...]\n"; return 0; }
    std::vector<std::string> args; for(int i=2;i<argc;++i){if(std::string(argv[i])=="--")continue;args.push_back(argv[i]);}
    auto r = jau::run_bytecode(argv[1], args);
    if (!r.ok) { std::cerr << "jur: " << r.message << "\n"; return 1; }
    return 0;
}
