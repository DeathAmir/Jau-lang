#include "jgui_api.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct SectionInfo {
    std::string name;
    std::uint64_t address = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

struct FunctionInfo {
    std::string name;
    std::uint32_t arity = 0;
    std::uint32_t locals = 0;
    std::vector<std::string> instructions;
};

struct Analysis {
    std::string path;
    std::string format = "Unknown";
    std::string architecture = "Unknown";
    std::string detail;
    std::uint64_t file_size = 0;
    std::vector<SectionInfo> sections;
    std::vector<FunctionInfo> functions;
};

static std::vector<std::uint8_t> read_binary(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open input file: " + path);
    in.seekg(0, std::ios::end);
    const auto end = in.tellg();
    if (end < 0) throw std::runtime_error("cannot determine input file size");
    std::vector<std::uint8_t> data(static_cast<std::size_t>(end));
    in.seekg(0, std::ios::beg);
    if (!data.empty()) in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!in && !data.empty()) throw std::runtime_error("cannot read complete input file");
    return data;
}

static void require_range(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t count) {
    if (offset > data.size() || count > data.size() - offset) throw std::runtime_error("truncated binary structure");
}

static std::uint16_t u16(const std::vector<std::uint8_t>& data, std::size_t offset) {
    require_range(data, offset, 2);
    return static_cast<std::uint16_t>(data[offset]) |
           static_cast<std::uint16_t>(data[offset + 1] << 8);
}

static std::uint32_t u32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    require_range(data, offset, 4);
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

static std::uint64_t u64(const std::vector<std::uint8_t>& data, std::size_t offset) {
    require_range(data, offset, 8);
    std::uint64_t result = 0;
    for (int i = 7; i >= 0; --i) result = (result << 8) | data[offset + static_cast<std::size_t>(i)];
    return result;
}

static std::string hex64(std::uint64_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << value;
    return out.str();
}

static std::string fixed_name(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t width) {
    require_range(data, offset, width);
    std::size_t length = 0;
    while (length < width && data[offset + length] != 0) ++length;
    return std::string(reinterpret_cast<const char*>(data.data() + offset), length);
}

static std::string read_jbc_string(const std::vector<std::uint8_t>& data, std::size_t& cursor) {
    const std::uint32_t length = u32(data, cursor);
    cursor += 4;
    require_range(data, cursor, length);
    std::string value(reinterpret_cast<const char*>(data.data() + cursor), length);
    cursor += length;
    return value;
}

static const char* opcode_name(std::uint8_t op) {
    static const char* names[] = {
        "Const", "Null", "LoadG", "StoreG", "LoadL", "StoreL", "Pop", "Add", "Sub", "Mul",
        "Div", "Mod", "Neg", "Not", "BitNot", "Eq", "Ne", "Lt", "Le", "Gt", "Ge", "And",
        "Or", "BitAnd", "BitOr", "BitXor", "Shl", "Shr", "Jump", "JumpFalse", "MakeArray", "Index",
        "Call", "Ret", "Halt", "SetIndex"
    };
    return op < sizeof(names) / sizeof(names[0]) ? names[op] : "UnknownOp";
}

static void parse_jbc(const std::vector<std::uint8_t>& data, Analysis& out) {
    require_range(data, 0, 8);
    std::size_t cursor = 4;
    const std::uint32_t version = u32(data, cursor);
    cursor += 4;
    out.format = "Jau Bytecode (JBC1)";
    out.architecture = "Portable VM";
    out.detail = "bytecode version " + std::to_string(version);

    const std::uint32_t constants = u32(data, cursor);
    cursor += 4;
    for (std::uint32_t i = 0; i < constants; ++i) {
        require_range(data, cursor, 1);
        const std::uint8_t tag = data[cursor++];
        if (tag == 0) continue;
        if (tag == 1 || tag == 2) { require_range(data, cursor, 8); cursor += 8; }
        else if (tag == 3) { require_range(data, cursor, 1); cursor += 1; }
        else if (tag == 4) { (void)read_jbc_string(data, cursor); }
        else throw std::runtime_error("unknown JBC constant tag " + std::to_string(tag));
    }

    const std::uint32_t globals = u32(data, cursor);
    cursor += 4;
    for (std::uint32_t i = 0; i < globals; ++i) (void)read_jbc_string(data, cursor);

    const std::uint32_t functions = u32(data, cursor);
    cursor += 4;
    for (std::uint32_t i = 0; i < functions; ++i) {
        FunctionInfo function;
        function.name = read_jbc_string(data, cursor);
        function.arity = u32(data, cursor); cursor += 4;
        function.locals = u32(data, cursor); cursor += 4;
        const std::uint32_t count = u32(data, cursor); cursor += 4;
        function.instructions.reserve(count);
        for (std::uint32_t ip = 0; ip < count; ++ip) {
            require_range(data, cursor, 5);
            const std::uint8_t op = data[cursor++];
            const std::int32_t argument = static_cast<std::int32_t>(u32(data, cursor));
            cursor += 4;
            std::ostringstream line;
            line << std::setw(4) << std::setfill('0') << ip << "  " << opcode_name(op);
            if (op == 0 || op == 2 || op == 3 || op == 4 || op == 5 || op == 28 || op == 29 || op == 30 || op == 32)
                line << " " << argument;
            function.instructions.push_back(line.str());
        }
        out.functions.push_back(std::move(function));
    }
}

static std::string pe_machine(std::uint16_t machine) {
    if (machine == 0x8664) return "x86_64";
    if (machine == 0x014c) return "x86";
    if (machine == 0xaa64) return "ARM64";
    return "PE machine " + hex64(machine);
}

static void parse_pe(const std::vector<std::uint8_t>& data, Analysis& out) {
    require_range(data, 0x3c, 4);
    const std::uint32_t pe = u32(data, 0x3c);
    require_range(data, pe, 24);
    if (std::memcmp(data.data() + pe, "PE\0\0", 4) != 0) throw std::runtime_error("invalid PE signature");
    const std::uint16_t machine = u16(data, pe + 4);
    const std::uint16_t section_count = u16(data, pe + 6);
    const std::uint16_t optional_size = u16(data, pe + 20);
    const std::size_t optional = static_cast<std::size_t>(pe) + 24;
    require_range(data, optional, optional_size);
    const std::uint16_t magic = optional_size >= 2 ? u16(data, optional) : 0;

    out.format = magic == 0x20b ? "PE32+" : magic == 0x10b ? "PE32" : "Portable Executable";
    out.architecture = pe_machine(machine);
    out.detail = std::to_string(section_count) + " sections";

    const std::size_t table = optional + optional_size;
    for (std::uint16_t i = 0; i < section_count; ++i) {
        const std::size_t offset = table + static_cast<std::size_t>(i) * 40;
        require_range(data, offset, 40);
        SectionInfo section;
        section.name = fixed_name(data, offset, 8);
        section.size = u32(data, offset + 16);
        section.address = u32(data, offset + 12);
        section.offset = u32(data, offset + 20);
        out.sections.push_back(std::move(section));
    }
}

static std::string elf_machine(std::uint16_t machine) {
    if (machine == 3) return "x86";
    if (machine == 62) return "x86_64";
    if (machine == 183) return "ARM64";
    return "ELF machine " + std::to_string(machine);
}

static void parse_elf(const std::vector<std::uint8_t>& data, Analysis& out) {
    require_range(data, 0, 52);
    const std::uint8_t elf_class = data[4];
    const std::uint8_t endian = data[5];
    if ((elf_class != 1 && elf_class != 2) || endian != 1) throw std::runtime_error("only little-endian ELF32/ELF64 is supported");
    const bool is64 = elf_class == 2;
    if (is64) require_range(data, 0, 64);

    const std::uint16_t machine = u16(data, 18);
    const std::uint64_t section_offset = is64 ? u64(data, 40) : u32(data, 32);
    const std::uint16_t section_entry_size = is64 ? u16(data, 58) : u16(data, 46);
    const std::uint16_t section_count = is64 ? u16(data, 60) : u16(data, 48);
    const std::uint16_t string_index = is64 ? u16(data, 62) : u16(data, 50);

    out.format = is64 ? "ELF64" : "ELF32";
    out.architecture = elf_machine(machine);
    out.detail = std::to_string(section_count) + " sections";
    if (!section_offset || !section_entry_size || string_index >= section_count) return;

    auto section_header = [&](std::uint16_t index) -> std::size_t {
        const std::uint64_t value = section_offset + static_cast<std::uint64_t>(index) * section_entry_size;
        if (value > static_cast<std::uint64_t>(data.size())) throw std::runtime_error("ELF section table out of range");
        require_range(data, static_cast<std::size_t>(value), section_entry_size);
        return static_cast<std::size_t>(value);
    };

    const std::size_t string_header = section_header(string_index);
    const std::uint64_t strings_offset = is64 ? u64(data, string_header + 24) : u32(data, string_header + 16);
    const std::uint64_t strings_size = is64 ? u64(data, string_header + 32) : u32(data, string_header + 20);
    if (strings_offset + strings_size > data.size()) throw std::runtime_error("ELF section-name table out of range");

    for (std::uint16_t i = 0; i < section_count; ++i) {
        const std::size_t header = section_header(i);
        const std::uint32_t name_offset = u32(data, header);
        SectionInfo section;
        if (name_offset < strings_size) {
            const std::size_t start = static_cast<std::size_t>(strings_offset + name_offset);
            std::size_t end = start;
            while (end < strings_offset + strings_size && data[end] != 0) ++end;
            section.name = std::string(reinterpret_cast<const char*>(data.data() + start), end - start);
        }
        section.address = is64 ? u64(data, header + 16) : u32(data, header + 12);
        section.offset = is64 ? u64(data, header + 24) : u32(data, header + 16);
        section.size = is64 ? u64(data, header + 32) : u32(data, header + 20);
        out.sections.push_back(std::move(section));
    }
}

static Analysis analyze(const std::string& path) {
    Analysis out;
    out.path = path;
    const auto data = read_binary(path);
    out.file_size = data.size();
    if (data.size() >= 4 && std::memcmp(data.data(), "JBC1", 4) == 0) parse_jbc(data, out);
    else if (data.size() >= 2 && data[0] == 'M' && data[1] == 'Z') parse_pe(data, out);
    else if (data.size() >= 4 && data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') parse_elf(data, out);
    else out.detail = "No supported PE, ELF or JBC header was found";
    return out;
}

static void ui_text(const std::string& value) {
    jgui_text(value.c_str());
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: jau-analyzer <file.exe|file.dll|file.so|file.o|file.jbc>\n");
        return 2;
    }

    Analysis analysis;
    try {
        analysis = analyze(argv[1]);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "jau-analyzer: %s\n", e.what());
        return 1;
    }

    if (!jgui_create("Jau Analyzer", 1280, 800)) {
        std::fprintf(stderr, "jau-analyzer: JGui initialization failed\n");
        return 3;
    }

    int view = 0;
    while (jgui_begin_frame()) {
        jgui_begin_window("Jau Analyzer");
        ui_text("File: " + analysis.path);
        ui_text("Format: " + analysis.format + " | Architecture: " + analysis.architecture);
        ui_text("Size: " + std::to_string(analysis.file_size) + " bytes | " + analysis.detail);
        jgui_separator();

        if (jgui_button("Overview / Sections")) view = 0;
        jgui_same_line();
        if (jgui_button("JBC Disassembly")) view = 1;
        jgui_separator();

        if (view == 0) {
            if (analysis.sections.empty()) ui_text("No PE/ELF sections available for this file.");
            for (const auto& section : analysis.sections) {
                ui_text((section.name.empty() ? "<unnamed>" : section.name) +
                        "  VA=" + hex64(section.address) +
                        "  file=" + hex64(section.offset) +
                        "  size=" + hex64(section.size));
            }
        } else {
            if (analysis.functions.empty()) ui_text("JBC function metadata is not available for this file.");
            for (const auto& function : analysis.functions) {
                jgui_separator();
                ui_text("func " + function.name + "  arity=" + std::to_string(function.arity) +
                        " locals=" + std::to_string(function.locals));
                for (const auto& instruction : function.instructions) ui_text(instruction);
            }
        }

        jgui_end_window();
        jgui_render();
    }

    jgui_destroy();
    return 0;
}
