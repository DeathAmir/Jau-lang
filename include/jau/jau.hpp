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
Result run_source(const std::string& input, const CompileOptions& options = {});
Result run_bytecode(const std::string& input);
Result emit_assembly(const std::string& input, const std::string& output, const std::string& target, const CompileOptions& options = {});
Result native_build(const std::string& input, const std::string& output, const std::string& target, const CompileOptions& options = {});
std::string version();
}
