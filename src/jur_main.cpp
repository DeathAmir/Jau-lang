#include "jau/jau.hpp"
#include <iostream>
int main(int argc, char** argv) {
    if (argc != 2) { std::cout << "jur " << jau::version() << "\nusage: jur <file.jbc>\n"; return argc == 1 ? 0 : 2; }
    auto r = jau::run_bytecode(argv[1]);
    if (!r.ok) { std::cerr << "jur: " << r.message << "\n"; return 1; }
    return 0;
}
