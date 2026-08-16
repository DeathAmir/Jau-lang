/*
    Jau assembler driver

    The object writer already promotes unresolved call targets to undefined
    symbols when a relocation actually references them. AOT historically emitted
    broad `.extern` declarations (for example printf/puts) even when no generated
    instruction called those functions. COFF consumers then saw unnecessary CRT
    dependencies in otherwise freestanding Jau library objects.

    This driver removes declarative `.extern` lines before invoking the existing
    assembler implementation. Real external references remain fully preserved:
    unresolved call relocations are detected by the assembler and promoted to
    undefined symbols exactly when they are used.
*/

#define main jauas_legacy_entry_point
#include "jauas_main.cpp"
#undef main

static bool jauas_object_mode(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--object") return true;
    }
    return false;
}

static std::string jauas_prune_declarative_externs(const fs::path& input) {
    std::ifstream source(input);
    if (!source) throw std::runtime_error("cannot open input: " + input.string());

    std::ostringstream filtered;
    std::string line;
    while (std::getline(source, line)) {
        const std::string cleaned = trim(line);
        if (starts(cleaned, ".extern ")) continue;
        filtered << line << '\n';
    }
    return filtered.str();
}

int main(int argc, char** argv) {
    try {
        if (argc < 2 || !jauas_object_mode(argc, argv)) {
            return jauas_legacy_entry_point(argc, argv);
        }

        const fs::path original = argv[1];
        const fs::path temporary = fs::temp_directory_path() /
            ("jauas_filtered_" + std::to_string(std::hash<std::string>{}(
                original.string() + std::to_string(static_cast<unsigned long long>(fs::file_size(original)))) ) + ".s");

        {
            std::ofstream output(temporary, std::ios::binary);
            if (!output) throw std::runtime_error("cannot create temporary filtered assembly");
            output << jauas_prune_declarative_externs(original);
        }

        std::vector<std::string> arguments;
        arguments.reserve(static_cast<std::size_t>(argc));
        for (int i = 0; i < argc; ++i) arguments.emplace_back(argv[i]);
        arguments[1] = temporary.string();

        std::vector<char*> raw;
        raw.reserve(arguments.size());
        for (auto& argument : arguments) raw.push_back(argument.data());

        const int result = jauas_legacy_entry_point(static_cast<int>(raw.size()), raw.data());
        std::error_code ec;
        fs::remove(temporary, ec);
        return result;
    } catch (const std::exception& exception) {
        std::cerr << "jauas: " << exception.what() << "\n";
        return 1;
    }
}
