#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace jau {
struct CompileOptions {
    int optimize = 2;
    std::vector<std::string> import_paths;
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
std::string version();
}
