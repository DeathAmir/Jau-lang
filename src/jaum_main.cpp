#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string trim(std::string s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

static std::string unquote(std::string s) {
    s = trim(s);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')))
        return s.substr(1, s.size() - 2);
    return s;
}

static std::vector<std::string> split(std::string s) {
    std::vector<std::string> v;
    s = trim(s);
    if (s.size() >= 2 && s.front() == '[' && s.back() == ']') s = s.substr(1, s.size() - 2);
    std::string x;
    std::istringstream in(s);
    while (std::getline(in, x, ',')) {
        x = unquote(x);
        if (!x.empty()) v.push_back(x);
    }
    return v;
}

static std::string join_csv(const std::vector<std::string>& values) {
    std::string out;
    for (const auto& value : values) {
        if (!out.empty()) out += ',';
        out += value;
    }
    return out;
}

static std::string quote(const std::string& s) {
#ifdef _WIN32
    std::string r = "\"";
    for (char c : s) { if (c == '\"') r += "\\\""; else r += c; }
    return r + "\"";
#else
    std::string r = "'";
    for (char c : s) { if (c == '\'') r += "'\\''"; else r += c; }
    return r + "'";
#endif
}

static fs::path self_path(const char* argv0) {
#ifdef _WIN32
    std::vector<char> b(32768);
    DWORD n = GetModuleFileNameA(nullptr, b.data(), (DWORD)b.size());
    if (n && n < b.size()) return fs::path(std::string(b.data(), n));
#else
    std::vector<char> b(4096);
    ssize_t n = ::readlink("/proc/self/exe", b.data(), b.size() - 1);
    if (n > 0) return fs::path(std::string(b.data(), (size_t)n));
#endif
    std::error_code ec;
    auto p = fs::absolute(argv0 ? argv0 : "", ec);
    return ec ? fs::path(argv0 ? argv0 : "") : p;
}

static bool yaml_file(const fs::path& p) {
    auto e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return e == ".yaml" || e == ".yml";
}

static std::string flatten_yaml_key(const std::string& section, const std::string& key) {
    if (section.empty()) return key;
    if (section == "project" || section == "build") return key;
    if (section == "windows" && key == "subsystem") return "subsystem";
    return section + "." + key;
}

static std::map<std::string, std::string> config_yaml(const fs::path& p) {
    std::ifstream f(p);
    if (!f) throw std::runtime_error("cannot open " + p.string());

    std::map<std::string, std::string> out;
    std::map<std::string, std::vector<std::string>> lists;
    std::string section, line;

    while (std::getline(f, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        if (trim(line).empty()) continue;

        size_t indent = 0;
        while (indent < line.size() && (line[indent] == ' ' || line[indent] == '\t')) ++indent;
        std::string text = trim(line);

        if (text.rfind("- ", 0) == 0) {
            if (section.empty()) throw std::runtime_error("YAML list item without a key: " + text);
            lists[section].push_back(unquote(text.substr(2)));
            continue;
        }

        auto colon = text.find(':');
        if (colon == std::string::npos) throw std::runtime_error("invalid Jaum.yaml line: " + text);
        std::string key = trim(text.substr(0, colon));
        std::string value = trim(text.substr(colon + 1));

        if (value.empty()) {
            section = key;
            continue;
        }

        std::string full = indent == 0 ? key : flatten_yaml_key(section, key);
        value = unquote(value);
        if (value.size() >= 2 && value.front() == '[' && value.back() == ']') out[full] = join_csv(split(value));
        else out[full] = value;

        if (indent == 0) section.clear();
    }

    for (auto& kv : lists) {
        std::string key = kv.first;
        if (key.rfind("project.", 0) == 0) key = key.substr(8);
        out[key] = join_csv(kv.second);
    }
    return out;
}

static std::map<std::string, std::string> config_legacy(const fs::path& p) {
    std::ifstream f(p);
    if (!f) throw std::runtime_error("cannot open " + p.string());
    std::map<std::string, std::string> m;
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
        auto q = line.find('=');
        if (q == std::string::npos) throw std::runtime_error("invalid jaum.txt line: " + line);
        m[trim(line.substr(0, q))] = unquote(line.substr(q + 1));
    }
    return m;
}

static std::map<std::string, std::string> config(const fs::path& p) {
    return yaml_file(p) ? config_yaml(p) : config_legacy(p);
}

static std::string replace_all(std::string s, const std::string& a, const std::string& b) {
    size_t p = 0;
    while (!a.empty() && (p = s.find(a, p)) != std::string::npos) {
        s.replace(p, a.size(), b);
        p += b.size();
    }
    return s;
}

static int run(const fs::path& exe, const std::vector<std::string>& args, bool verbose) {
    std::string cmd = quote(exe.string());
    for (auto& a : args) cmd += " " + quote(a);
    if (verbose) std::cerr << "[jaum] " << cmd << "\n";
#ifdef _WIN32
    std::vector<char> mutable_cmd(cmd.begin(), cmd.end());
    mutable_cmd.push_back('\0');
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::string app = exe.string();
    if (!CreateProcessA(app.c_str(), mutable_cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::cerr << "[jaum] CreateProcess failed, win32=" << GetLastError() << "\n";
        return 127;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
#else
    return std::system(cmd.c_str());
#endif
}

static std::string ext_for(const std::string& type, const std::string& target) {
    bool win = target.rfind("windows-", 0) == 0;
    if (type == "exe") return win ? ".exe" : "";
    if (type == "shared") return win ? ".dll" : ".so";
    if (type == "static") return win ? ".lib" : ".a";
    if (type == "obj") return win ? ".obj" : ".o";
    if (type == "asm") return ".s";
    return "";
}

static int build_one(const fs::path& cfg, const std::string& override_target) {
    auto m = config(cfg);
    auto get = [&](const std::string& k, const std::string& d = "") {
        auto i = m.find(k);
        return i == m.end() ? d : i->second;
    };

    std::string name = get("name", "app");
    std::string source = get("source", "src/main.jau");
    std::string type = get("type", "exe");
    std::string target = override_target.empty() ? get("target", "linux-x86_64") : override_target;
    std::string opt = get("optimize", "0");
    if (type == "native") type = "exe";
    if (type != "exe" && type != "shared" && type != "static" && type != "obj" && type != "asm")
        throw std::runtime_error("type must be exe, shared, static, obj or asm");

    std::string out = get("output");
    if (out.empty()) out = (fs::path(get("build_dir", "build")) / (name + "-" + target + ext_for(type, target))).string();
    out = replace_all(out, "{name}", name);
    out = replace_all(out, "{target}", target);
    out = replace_all(out, "{ext}", ext_for(type, target));
    if (fs::path(out).has_parent_path()) fs::create_directories(fs::path(out).parent_path());

    auto self = self_path(nullptr);
    auto jauc = self.parent_path() /
#ifdef _WIN32
        "jauc.exe";
#else
        "jauc";
#endif
    if (!fs::exists(jauc)) throw std::runtime_error("jauc not found next to jaum: " + jauc.string());

    std::vector<std::string> a;
    a.push_back(type == "exe" ? "native" : type);
    a.push_back(source);
    a.push_back("-o"); a.push_back(out);
    a.push_back("--target"); a.push_back(target);
    a.push_back("-O" + opt);

    for (auto& x : split(get("links"))) { a.push_back("--link"); a.push_back(x); }
    for (auto& x : split(get("system_libs"))) { a.push_back("--system-lib"); a.push_back(x); }
    for (auto& x : split(get("imports"))) { a.push_back("--import"); a.push_back(x); }
    for (auto& x : split(get("exports"))) { a.push_back("--export"); a.push_back(x); }
    for (auto& x : split(get("include_paths"))) { a.push_back("-I"); a.push_back(x); }

    if (type == "exe" || (type == "shared" && target.rfind("windows-", 0) == 0)) {
        a.push_back("--subsystem");
        a.push_back(get("subsystem", "console"));
    }
    if (get("debug", "false") == "true") a.push_back("--debug");

    int rc = run(jauc, a, true);
    if (rc != 0) std::cerr << "[jaum] build failed for " << target << "\n";
    else std::cout << "JauM built " << out << "\n";
    return rc;
}

static fs::path default_config() {
    if (fs::exists("Jaum.yaml")) return "Jaum.yaml";
    if (fs::exists("jaum.yaml")) return "jaum.yaml";
    if (fs::exists("Jaum.yml")) return "Jaum.yml";
    return "jaum.txt";
}

static void usage() {
    std::cout << "JauM 1.0\n"
              << "usage: jaum init [name] | jaum build [-f Jaum.yaml] [--target target] | "
              << "jaum build-all [-f Jaum.yaml] | jaum clean [-f Jaum.yaml] | jaum show [-f Jaum.yaml]\n";
}

int main(int argc, char** argv) {
    std::cerr << "DeathAmir Jau @ DeathAmir 2026 (C)\n";
    try {
        if (argc < 2) { usage(); return 0; }
        std::string cmd = argv[1];
        fs::path file = default_config();
        std::string target;
        bool explicit_file = false;
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "-f" && i + 1 < argc) { file = argv[++i]; explicit_file = true; }
            else if (a == "--target" && i + 1 < argc) target = argv[++i];
        }

        if (cmd == "init") {
            std::string name = argc > 2 && argv[2][0] != '-' ? argv[2] : fs::current_path().filename().string();
            fs::path out = explicit_file ? file : fs::path("Jaum.yaml");
            if (fs::exists(out)) throw std::runtime_error(out.string() + " already exists");
            std::ofstream o(out);
            o << "# JauM project file\n"
              << "project:\n"
              << "  name: " << name << "\n"
              << "  source: src/main.jau\n"
              << "  type: exe\n"
              << "  output: build/{name}-{target}{ext}\n"
              << "  optimize: 0\n"
              << "targets:\n"
              << "  - windows-x86_64\n"
              << "  - linux-x86_64\n"
              << "subsystem: console\n"
              << "links: []\n"
              << "system_libs: []\n"
              << "imports: []\n"
              << "exports: []\n"
              << "include_paths:\n"
              << "  - stdlib\n";
            fs::create_directories("src");
            if (!fs::exists("src/main.jau")) {
                std::ofstream j("src/main.jau");
                j << "func main() {\n    print(\"Hello from JauM\");\n    return 0;\n}\n";
            }
            std::cout << "created " << out.string() << "\n";
            return 0;
        }

        if (cmd == "build") return build_one(file, target);
        if (cmd == "build-all") {
            auto m = config(file);
            auto it = m.find("targets");
            if (it == m.end() || split(it->second).empty()) throw std::runtime_error("build-all requires targets in " + file.string());
            int bad = 0;
            for (auto& t : split(it->second)) if (build_one(file, t) != 0) bad = 1;
            return bad;
        }
        if (cmd == "clean") {
            auto m = config(file);
            auto it = m.find("build_dir");
            fs::path d = it == m.end() ? "build" : it->second;
            std::error_code ec;
            fs::remove_all(d, ec);
            if (ec) throw std::runtime_error("cannot clean " + d.string() + ": " + ec.message());
            std::cout << "cleaned " << d.string() << "\n";
            return 0;
        }
        if (cmd == "show") {
            for (auto& kv : config(file)) std::cout << kv.first << "=" << kv.second << "\n";
            return 0;
        }
        usage();
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "jaum: " << e.what() << "\n";
        return 1;
    }
}
