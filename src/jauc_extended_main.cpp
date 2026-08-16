/*
    Jau compiler extended driver

    This translation unit keeps the v0.9 compiler driver intact while adding
    multi-member static archives and a Windows-safe native source preprocessing
    path. The legacy driver remains included as the single source of truth for
    parsing, AOT generation, package discovery and PE/ELF target behavior.
*/

#define main jauc_legacy_entry_point
#include "jauc_main.cpp"
#undef main

struct StaticArchiveSymbol {
    std::string name;
    std::size_t member = 0;
};

struct StaticArchiveMember {
    fs::path path;
    std::string data;
    std::vector<std::string> symbols;
};

static void write_static_archive_multi(const std::vector<fs::path>& objects,
                                       const fs::path& output,
                                       const std::string& target) {
    if (objects.empty()) throw std::runtime_error("static archive requires at least one object");
    const bool windows = is_windows_target(target);

    std::vector<StaticArchiveMember> members;
    std::vector<StaticArchiveSymbol> indexed_symbols;
    members.reserve(objects.size());

    for (std::size_t member_index = 0; member_index < objects.size(); ++member_index) {
        StaticArchiveMember member;
        member.path = objects[member_index];
        member.data = read_text(member.path);
        if (member.data.empty()) {
            throw std::runtime_error("cannot read object for static archive: " + member.path.string());
        }

        member.symbols = windows ? coff_defined_symbols(member.path) : elf_defined_symbols(member.path);
        std::sort(member.symbols.begin(), member.symbols.end());
        member.symbols.erase(std::unique(member.symbols.begin(), member.symbols.end()), member.symbols.end());
        for (const auto& symbol : member.symbols) {
            if (!symbol.empty()) indexed_symbols.push_back({symbol, member_index});
        }
        members.push_back(std::move(member));
    }

    if (indexed_symbols.empty()) throw std::runtime_error("objects have no externally defined symbols for static library");

    std::sort(indexed_symbols.begin(), indexed_symbols.end(), [](const auto& a, const auto& b) {
        if (a.name != b.name) return a.name < b.name;
        return a.member < b.member;
    });
    indexed_symbols.erase(std::unique(indexed_symbols.begin(), indexed_symbols.end(), [](const auto& a, const auto& b) {
        return a.name == b.name;
    }), indexed_symbols.end());

    std::size_t symbol_names_size = 0;
    for (const auto& symbol : indexed_symbols) symbol_names_size += symbol.name.size() + 1;

    const std::size_t first_linker_size = 4 + indexed_symbols.size() * 4 + symbol_names_size;
    const std::size_t second_linker_size = windows
        ? 4 + members.size() * 4 + 4 + indexed_symbols.size() * 2 + symbol_names_size
        : 0;

    std::vector<std::uint32_t> member_offsets(members.size());
    std::size_t cursor = 8 + ar_span(first_linker_size) + (windows ? ar_span(second_linker_size) : 0);
    for (std::size_t i = 0; i < members.size(); ++i) {
        if (cursor > 0xffffffffULL) throw std::runtime_error("static archive exceeds 32-bit archive index limits");
        member_offsets[i] = static_cast<std::uint32_t>(cursor);
        cursor += ar_span(members[i].data.size());
    }

    std::string first_linker;
    ar_be32(first_linker, static_cast<std::uint32_t>(indexed_symbols.size()));
    for (const auto& symbol : indexed_symbols) ar_be32(first_linker, member_offsets[symbol.member]);
    for (const auto& symbol : indexed_symbols) {
        first_linker += symbol.name;
        first_linker.push_back('\0');
    }

    std::string second_linker;
    if (windows) {
        ar_le32(second_linker, static_cast<std::uint32_t>(members.size()));
        for (auto offset : member_offsets) ar_le32(second_linker, offset);
        ar_le32(second_linker, static_cast<std::uint32_t>(indexed_symbols.size()));
        for (const auto& symbol : indexed_symbols) {
            if (symbol.member + 1 > 0xffffu) throw std::runtime_error("too many static archive members");
            ar_le16(second_linker, static_cast<std::uint16_t>(symbol.member + 1));
        }
        for (const auto& symbol : indexed_symbols) {
            second_linker += symbol.name;
            second_linker.push_back('\0');
        }
    }

    std::string archive = "!<arch>\n";
    ar_member(archive, "/", first_linker);
    if (windows) ar_member(archive, "/", second_linker);
    for (std::size_t i = 0; i < members.size(); ++i) {
        const std::string member_name = windows
            ? ("jau" + std::to_string(i) + ".obj/")
            : ("jau" + std::to_string(i) + ".o/");
        ar_member(archive, member_name, members[i].data);
    }
    write_binary(output, archive);
}

static bool compile_native_source_safe(const std::string& source,
                                       const std::string& target,
                                       const fs::path& output) {
    const std::string extension = lower_ext(source);
    const bool cpp = is_cpp_ext(extension);
    if (extension != ".c" && !cpp) return false;

#ifdef _WIN32
    const char* configured = std::getenv(cpp ? "JAU_CXX" : "JAU_CC");
    std::string compiler = configured ? configured : (cpp ? "g++" : "gcc");
    if (target == "windows-x86" && !configured) compiler = cpp ? "i686-w64-mingw32-g++" : "i686-w64-mingw32-gcc";

    fs::path executable = compiler;
    if (!fs::exists(executable)) {
        std::vector<char> path_buffer(32768);
        DWORD length = SearchPathA(nullptr, compiler.c_str(), ".exe",
                                   static_cast<DWORD>(path_buffer.size()), path_buffer.data(), nullptr);
        if (!length || length >= path_buffer.size()) return false;
        executable = std::string(path_buffer.data(), length);
    }

    std::vector<std::string> args{"-c", "-O2", "-ffunction-sections", "-fdata-sections"};
    if (cpp) {
        args.push_back("-fno-exceptions");
        args.push_back("-fno-rtti");
    }
    args.push_back(source);
    args.push_back("-o");
    args.push_back(output.string());
    return run_process(executable, args) == 0;
#else
    return compile_native_input(source, target, output);
#endif
}

static int extended_static_command(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "jauc: static requires a Jau input file\n";
        return 2;
    }

    const std::string input = argv[2];
    std::string output;
    std::string target = "linux-x86_64";
    jau::CompileOptions options;
    bool optimize_set = false;

    for (int i = 3; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "-o" && i + 1 < argc) output = argv[++i];
        else if (argument == "--target" && i + 1 < argc) target = argv[++i];
        else if (argument == "--link" && i + 1 < argc) options.native_inputs.push_back(argv[++i]);
        else if (argument == "-I" && i + 1 < argc) options.import_paths.push_back(argv[++i]);
        else if (argument == "--debug") options.debug = true;
        else if (argument.rfind("-O", 0) == 0 && argument.size() == 3 && argument[2] >= '0' && argument[2] <= '3') {
            options.optimize = argument[2] - '0';
            optimize_set = true;
        }
    }
    if (!optimize_set) options.optimize = 0;
    options.library_mode = true;

    if (output.empty()) output = is_windows_target(target) ? "jau-lib.lib" : "libjau.a";
    else if (is_windows_target(target) && lower_ext(output) != ".lib") output += ".lib";
    else if (!is_windows_target(target) && lower_ext(output) != ".a") output += ".a";

    const fs::path temporary = fs::temp_directory_path() /
        ("jau_static_multi_" + std::to_string(std::hash<std::string>{}(input + output + target)));
    std::error_code error;
    fs::remove_all(temporary, error);
    fs::create_directories(temporary);

    try {
        const auto package_info = collect_native_package_info(input, target, temporary / "packages", options);

        const fs::path assembly = temporary / "library.s";
        const fs::path jau_object = temporary / (is_windows_target(target) ? "library.obj" : "library.o");
        auto result = emit_with_native_symbols(input, assembly.string(), target, options);
        if (!result.ok) {
            fs::remove_all(temporary, error);
            print_diagnostic(input, "static", result.message);
            return 1;
        }

        const fs::path self = current_executable_path(argv[0]);
        const fs::path assembler = self.parent_path() /
#ifdef _WIN32
            "jauas.exe";
#else
            "jauas";
#endif
        if (run_process(assembler, {assembly.string(), "-o", jau_object.string(), "--target", target, "--object"}) != 0) {
            fs::remove_all(temporary, error);
            std::cerr << "jauc[static]: internal jauas static object build failed\n";
            return 1;
        }

        std::vector<fs::path> objects{jau_object};
        for (const auto& package_object : package_info.objects) objects.emplace_back(package_object);

        int serial = 0;
        for (const auto& native_input : options.native_inputs) {
            const std::string extension = lower_ext(native_input);
            if (extension == ".obj" || extension == ".o") {
                objects.emplace_back(native_input);
            } else if (extension == ".c" || is_cpp_ext(extension)) {
                const fs::path compiled = temporary /
                    ("link_" + std::to_string(serial++) + (is_windows_target(target) ? ".obj" : ".o"));
                if (!compile_native_source_safe(native_input, target, compiled)) {
                    fs::remove_all(temporary, error);
                    std::cerr << "jauc[static]: failed to compile native C/C++ input: " << native_input
                              << " (set JAU_CC/JAU_CXX if needed)\n";
                    return 1;
                }
                objects.push_back(compiled);
            } else {
                fs::remove_all(temporary, error);
                std::cerr << "jauc[static]: unsupported native input: " << native_input
                          << " (use .c/.cpp/.o/.obj)\n";
                return 1;
            }
        }

        write_static_archive_multi(objects, output, target);
        fs::remove_all(temporary, error);
        std::cout << "static library built: " << output << " (" << target << ", " << objects.size() << " objects)\n";
        return 0;
    } catch (const std::exception& exception) {
        fs::remove_all(temporary, error);
        std::cerr << "jauc[static]: " << exception.what() << "\n";
        return 1;
    }
}

static void extended_help() {
    std::cout << "Jau compiler " << jau::version() << "\nusage:\n"
              << "  jauc build <file.jau> [-o out.jbc] [-O0|-O1|-O2|-O3]\n"
              << "  jauc run <file.jau>\n"
              << "  jauc debug <file.jau> [-- args...]\n"
              << "  jauc check <file.jau> [--target <target>]\n"
              << "  jauc asm <file.jau> -o out.s --target <target> [--library]\n"
              << "  jauc obj <file.jau> -o out.o|out.obj --target <target>\n"
              << "  jauc native <file.jau> -o program --target <target> [--link native.obj] [--system-lib name] [--import symbol=dll] [--subsystem console|windows]\n"
              << "  jauc shared <file.jau> -o library --target <target> --export name [--link native.obj] [--system-lib name] [--import symbol=dll]\n"
              << "  jauc static <file.jau> -o library.lib|library.a --target <target> [--link native.obj]\n"
              << "  jauc targets\n"
              << "  jauc standalone <file.jau> -o program [--runtime path/to/jur]\n"
              << "  jauc --version\n\n"
              << "Multiple --link options are accepted by native, shared and static builds.\n"
              << "Windows shared libraries use Jau's internal PE DLL linker; VM programs can load DLL/SO runtimes through stdlib/ffi.jau.\n";
}

static int dispatch_with_safe_windows_sources(int argc, char** argv) {
#ifdef _WIN32
    if (argc >= 3) {
        const std::string command = argv[1];
        if (command == "native" || command == "shared") {
            std::string target = "linux-x86_64";
            for (int i = 3; i + 1 < argc; ++i) {
                if (std::string(argv[i]) == "--target") target = argv[i + 1];
            }
            if (is_windows_target(target)) {
                const fs::path temporary = fs::temp_directory_path() /
                    ("jau_native_sources_" + std::to_string(std::hash<std::string>{}(argv[2] + target)));
                std::error_code error;
                fs::remove_all(temporary, error);
                fs::create_directories(temporary);

                std::vector<std::string> arguments;
                arguments.reserve(static_cast<std::size_t>(argc));
                for (int i = 0; i < argc; ++i) arguments.emplace_back(argv[i]);

                int serial = 0;
                for (std::size_t i = 3; i + 1 < arguments.size(); ++i) {
                    if (arguments[i] != "--link") continue;
                    const std::string extension = lower_ext(arguments[i + 1]);
                    if (extension != ".c" && !is_cpp_ext(extension)) continue;
                    const fs::path object = temporary / ("source_" + std::to_string(serial++) + ".obj");
                    if (!compile_native_source_safe(arguments[i + 1], target, object)) {
                        fs::remove_all(temporary, error);
                        std::cerr << "jauc[" << command << "]: failed to compile native source: " << arguments[i + 1] << "\n";
                        return 1;
                    }
                    arguments[i + 1] = object.string();
                }

                std::vector<char*> raw;
                raw.reserve(arguments.size());
                for (auto& argument : arguments) raw.push_back(argument.data());
                const int result = jauc_main_impl(static_cast<int>(raw.size()), raw.data());
                fs::remove_all(temporary, error);
                return result;
            }
        }
    }
#endif
    return jauc_main_impl(argc, argv);
}

int main(int argc, char** argv) {
    std::cerr << jau::copyright_notice() << "\n";
    try {
        if (argc < 2) {
            extended_help();
            return 0;
        }
        const std::string command = argv[1];
        if (command == "help" || command == "--help" || command == "-h") {
            extended_help();
            return 0;
        }
        if (command == "static") return extended_static_command(argc, argv);
        return dispatch_with_safe_windows_sources(argc, argv);
    } catch (const std::exception& exception) {
        std::cerr << "jauc[fatal]: " << exception.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "jauc[fatal]: unknown internal error\n";
        return 1;
    }
}
