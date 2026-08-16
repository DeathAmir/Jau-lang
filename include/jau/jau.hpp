#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace jau {
struct CompileOptions {
    int optimize = 0;
    bool library_mode = false;
    std::vector<std::string> import_paths;
    std::vector<std::string> native_inputs;
    std::vector<std::string> native_symbols;
    bool debug = false;
};
struct Result {
    bool ok = false;
    std::string message;
};
Result compile_file(const std::string& input, const std::string& output, const CompileOptions& options = {});
Result run_source(const std::string& input, const CompileOptions& options = {}, const std::vector<std::string>& args = {});
Result run_bytecode(const std::string& input, const std::vector<std::string>& args = {});
Result emit_assembly(const std::string& input, const std::string& output, const std::string& target, const CompileOptions& options = {});
Result native_build(const std::string& input, const std::string& output, const std::string& target, const CompileOptions& options = {});
Result bundle_executable(const std::string& input, const std::string& output, const std::string& runtime_path, const CompileOptions& options = {});
Result run_embedded_executable(const std::string& executable, const std::vector<std::string>& args = {});
std::string package_pack(const std::string& root, const std::string& output);
std::string package_extract(const std::string& archive, const std::string& destination);
std::string package_manifest(const std::string& archive);
std::string package_read_file(const std::string& archive, const std::string& relative_path);
bool package_verify(const std::string& archive);
std::string version();
std::string copyright_notice();
}
