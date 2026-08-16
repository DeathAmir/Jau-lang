#include "jau/jau.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string current_executable_path(const char* argv0) {
#ifdef _WIN32
    std::vector<char> buf(32768);
    DWORD n = GetModuleFileNameA(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n > 0 && n < buf.size()) return std::string(buf.data(), n);
#else
    std::vector<char> buf(4096);
    ssize_t n = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (n > 0) return std::string(buf.data(), static_cast<size_t>(n));
#endif
    std::error_code ec;
    auto p = fs::absolute(argv0 ? argv0 : "", ec);
    return ec ? std::string(argv0 ? argv0 : "") : p.string();
}

static std::string quote(const std::string& x) {
#ifdef _WIN32
    std::string r = "\"";
    for (char c : x) { if (c == '"') r += "\\\""; else r += c; }
    return r + "\"";
#else
    std::string r = "'";
    for (char c : x) { if (c == '\'') r += "'\\''"; else r += c; }
    return r + "'";
#endif
}

static bool is_windows_target(const std::string& target) { return target.rfind("windows-", 0) == 0; }

static int run_process(const fs::path& executable, const std::vector<std::string>& args) {
    std::string command = quote(executable.string());
    for (const auto& a : args) command += " " + quote(a);
#ifdef _WIN32
    std::vector<char> mutable_command(command.begin(), command.end());
    mutable_command.push_back('\0');
    STARTUPINFOA si{}; si.cb = sizeof(si); PROCESS_INFORMATION pi{};
    std::string application = executable.string();
    if (!CreateProcessA(application.c_str(), mutable_command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE); DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return static_cast<int>(code);
#else
    return std::system(command.c_str());
#endif
}

static std::string trim_copy(std::string x) { size_t a=0,b=x.size(); while(a<b&&std::isspace((unsigned char)x[a]))++a; while(b>a&&std::isspace((unsigned char)x[b-1]))--b; return x.substr(a,b-a); }
static std::string manifest_value(const std::string& text, const std::string& key) {
    std::istringstream in(text); std::string line;
    while (std::getline(in,line)) { line=trim_copy(line); if(line.empty()||line[0]=='#')continue; auto q=line.find('='); if(q==std::string::npos)continue; if(trim_copy(line.substr(0,q))==key){auto v=trim_copy(line.substr(q+1));if(v.size()>=2&&v.front()=='"'&&v.back()=='"')v=v.substr(1,v.size()-2);return v;} }
    return "";
}
static fs::path jau_home_path(){ if(const char* h=std::getenv("JAU_HOME"))return fs::path(h); return fs::current_path()/".jau"; }
static std::vector<std::string> split_csv(const std::string& s){std::vector<std::string> out;std::string cur;std::istringstream in(s);while(std::getline(in,cur,',')){cur=trim_copy(cur);if(!cur.empty())out.push_back(cur);}return out;}
static std::string native_key(std::string target){for(char&c:target)if(c=='-')c='_';return "native_"+target;}
static std::string lower_copy_local(std::string x){for(char&c:x)c=(char)std::tolower((unsigned char)c);return x;}
static std::string read_text(const fs::path&p){std::ifstream f(p,std::ios::binary);if(!f)return "";std::ostringstream s;s<<f.rdbuf();return s.str();}
static void write_binary(const fs::path&p,const std::string&data){if(p.has_parent_path())fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("cannot create temporary native object");f.write(data.data(),(std::streamsize)data.size());}
static void write_link_metadata(const fs::path& object,const std::string& target,const std::string& entry,int optimize,const std::string& kind){
    fs::path p=object.string()+".jmeta";std::ofstream f(p,std::ios::trunc);if(!f)throw std::runtime_error("cannot write linker metadata: "+p.string());
    f<<"JAUMETA1\n";
    f<<"producer=\"jauc\"\n";
    f<<"version=\""<<jau::version()<<"\"\n";
    f<<"target=\""<<target<<"\"\n";
    f<<"abi=\"jau-c-v1\"\n";
    f<<"kind=\""<<kind<<"\"\n";
    f<<"subsystem=\"console\"\n";
    f<<"entry=\""<<entry<<"\"\n";
    f<<"optimize="<<optimize<<"\n";
}

struct NativeCollector {
    std::string target;
    fs::path temp;
    const jau::CompileOptions& options;
    std::unordered_set<std::string> packages;
    std::unordered_set<std::string> files;
    std::vector<std::string> objects;
    int serial=0;
    fs::path resolve_local(const fs::path& origin,const std::string& imp){std::vector<fs::path> cands{origin.parent_path()/imp};for(auto&i:options.import_paths)cands.push_back(fs::path(i)/imp);cands.push_back(jau_home_path()/"stdlib"/imp);cands.push_back(fs::current_path()/"stdlib"/imp);for(auto&c:cands)if(fs::exists(c))return c;return {};}
    void scan_text(const std::string& text,const fs::path& origin){std::regex re(R"(^\s*import\s+\"([^\"]+)\"\s*;?\s*$)");std::istringstream in(text);std::string line;while(std::getline(in,line)){std::smatch m;if(!std::regex_match(line,m,re))continue;std::string imp=m[1].str();if(imp.rfind("pkg:",0)==0)package(imp.substr(4));else{auto p=resolve_local(origin,imp);if(!p.empty())file(p);}}}
    void file(const fs::path&p){std::error_code ec;auto a=fs::weakly_canonical(p,ec);std::string k=ec?p.string():a.string();if(!files.insert(k).second)return;scan_text(read_text(p),p);}
    void package(const std::string& spec){
        auto slash=spec.find('/');std::string name=slash==std::string::npos?spec:spec.substr(0,slash),sub=slash==std::string::npos?"":spec.substr(slash+1);if(!packages.insert(name).second)return;
        fs::path archive=jau_home_path()/"packages"/name/"package.jaup";if(!fs::exists(archive))return;
        std::string mf=jau::package_manifest(archive.string());std::string type=lower_copy_local(manifest_value(mf,"type"));
        if(type=="native"){
            std::string members=manifest_value(mf,native_key(target));if(members.empty())members=manifest_value(mf,"native");
            if(members.empty())throw std::runtime_error("native package "+name+" has no object for target "+target);
            for(auto&member:split_csv(members)){
                auto ext=lower_copy_local(fs::path(member).extension().string());
                if(ext!=".obj"&&ext!=".o")throw std::runtime_error("native package "+name+" member is not an object file: "+member);
                std::string bytes=jau::package_read_file(archive.string(),member);
                fs::path p=temp/("pkg_"+std::to_string(serial++)+"_"+fs::path(member).filename().string());
                write_binary(p,bytes);objects.push_back(p.string());
            }
        }
        for(auto&dep:split_csv(manifest_value(mf,"dependencies")))package(dep);
        if(sub.empty())sub=manifest_value(mf,"main");
        if(!sub.empty()){
            auto ext=lower_copy_local(fs::path(sub).extension().string());
            if(!ext.empty()&&ext!=".jau")throw std::runtime_error("package source entry is not Jau source: "+sub);
            scan_text(jau::package_read_file(archive.string(),sub),fs::current_path()/"__package__.jau");
        }
    }
};

static std::vector<std::string> collect_native_package_objects(const std::string& input,const std::string& target,const fs::path&temp,const jau::CompileOptions&opt){NativeCollector c{target,temp,opt};c.file(input);return c.objects;}
static bool is_cpp_ext(const std::string&e){return e==".cpp"||e==".cxx"||e==".cc"||e==".c++";}
static std::string lower_ext(fs::path p){auto e=p.extension().string();for(char&c:e)c=(char)std::tolower((unsigned char)c);return e;}
static bool compile_native_input(const std::string& src,const std::string& target,const fs::path& out){
    std::string e=lower_ext(src);bool cpp=is_cpp_ext(e);if(e!=".c"&&!cpp)return false;const char* env=std::getenv(cpp?"JAU_CXX":"JAU_CC");std::string cc=env?env:(cpp?"g++":"gcc");if(target=="windows-x86"&&!env)cc=cpp?"i686-w64-mingw32-g++":"i686-w64-mingw32-gcc";std::string cmd=quote(cc)+" -c -O2 -ffunction-sections -fdata-sections ";if(cpp)cmd+="-fno-exceptions -fno-rtti ";cmd+=quote(src)+" -o "+quote(out.string());return std::system(cmd.c_str())==0;
}

static jau::Result windows_native_build(const char* argv0,const std::string&input,std::string output,const std::string&target,jau::CompileOptions opt){
    try{
        fs::path self=current_executable_path(argv0);fs::path dir=self.parent_path();fs::path assembler=dir/
#ifdef _WIN32
            "jauas.exe";
#else
            "jauas";
#endif
        fs::path linker=dir/
#ifdef _WIN32
            "jauld.exe";
#else
            "jauld";
#endif
        if(!fs::exists(assembler))return {false,"internal assembler not found: "+assembler.string()};if(!fs::exists(linker))return {false,"internal PE linker not found: "+linker.string()};
        fs::path tmp=fs::temp_directory_path()/("jau_native_"+std::to_string(std::hash<std::string>{}(input+output+target)));std::error_code ec;fs::remove_all(tmp,ec);fs::create_directories(tmp);
        fs::path asmp=tmp/"app.s",obj=tmp/"app.obj";opt.library_mode=false;auto r=jau::emit_assembly(input,asmp.string(),target,opt);if(!r.ok){fs::remove_all(tmp,ec);return r;}
        if(run_process(assembler,{asmp.string(),"-o",obj.string(),"--target",target,"--object"})!=0){fs::remove_all(tmp,ec);return {false,"internal jauas object build failed"};}
        std::vector<std::string> objs{obj.string()};auto pkg=collect_native_package_objects(input,target,tmp,opt);objs.insert(objs.end(),pkg.begin(),pkg.end());int serial=0;
        for(auto&x:opt.native_inputs){auto e=lower_ext(x);if(e==".obj"||e==".o")objs.push_back(x);else if(e==".c"||is_cpp_ext(e)){fs::path p=tmp/("link_"+std::to_string(serial++)+".obj");if(!compile_native_input(x,target,p)){fs::remove_all(tmp,ec);return {false,"failed to compile native C/C++ input: "+x+" (set JAU_CC/JAU_CXX if needed)"};}objs.push_back(p.string());}else{fs::remove_all(tmp,ec);return {false,"unsupported Windows native input: "+x+" (use .c/.cpp/.obj)"};}}
        if(fs::path(output).extension()!=".exe")output+=".exe";std::vector<std::string> args=objs;args.push_back("-o");args.push_back(output);args.push_back("--target");args.push_back(target);args.push_back("--entry");args.push_back("main");int rc=run_process(linker,args);fs::remove_all(tmp,ec);if(rc!=0)return {false,"internal jauld PE link failed"};return {true,"native executable built without external linker: "+output};
    }catch(const std::exception&e){return {false,e.what()};}
}

static void help() {
    std::cout << "Jau compiler " << jau::version() << "\nusage:\n"
              << "  jauc build <file.jau> [-o out.jbc] [-O0|-O1|-O2|-O3]\n"
              << "  jauc run <file.jau>\n"
              << "  jauc asm <file.jau> -o out.s --target <linux-x86_64|linux-x86|windows-x86_64|windows-x86> [--library]\n"
              << "  jauc obj <file.jau> -o out.o|out.obj --target <target>\n"
              << "  jauc native <file.jau> -o program --target <target> [--link file.c|file.cpp|file.obj] [-O0..-O3]\n"
              << "  jauc standalone <file.jau> -o program [--runtime path/to/jur]\n"
              << "  jauc --version\n\n"
              << "Windows native uses Jau's internal jauas + jauld PE32/PE32+ linker. Native defaults to -O3 unless an optimization level is supplied.\n"
              << "jauc obj emits <object>.jmeta (JAUMETA1) so jauld can infer target, ABI, subsystem and entry symbol.\n"
              << "Native .jaux packages can provide native_windows_x86_64/native_windows_x86 objects.\n"
              << "AOT interoperability: extern func c_function(a, b); Jau exports jau_fn_<name>.\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { help(); return 0; }
    std::string cmd = argv[1];
    if (cmd == "--version" || cmd == "version") { std::cout << jau::version() << "\n"; return 0; }
    if (argc < 3) { help(); return 2; }
    std::string input = argv[2], output, target, runtime; std::vector<std::string> runargs; jau::CompileOptions opt; bool optimize_set=false;
    for (int i = 3; i < argc; ++i) { std::string a = argv[i]; if (a == "--") { for (++i; i < argc; ++i) runargs.push_back(argv[i]); break; } if (a == "-o" && i + 1 < argc) output = argv[++i]; else if (a == "--target" && i + 1 < argc) target = argv[++i]; else if (a == "--runtime" && i + 1 < argc) runtime = argv[++i]; else if (a == "--library") opt.library_mode = true; else if (a == "--link" && i + 1 < argc) opt.native_inputs.push_back(argv[++i]); else if (a == "-I" && i + 1 < argc) opt.import_paths.push_back(argv[++i]); else if (a.rfind("-O", 0) == 0 && a.size() == 3 && a[2]>='0'&&a[2]<='3') {opt.optimize = a[2] - '0';optimize_set=true;} }
    if(cmd=="native"&&!optimize_set)opt.optimize=3;
    jau::Result r;
    if (cmd == "run") r = jau::run_source(input, opt, runargs);
    else if (cmd == "build") { if (output.empty()) output = input.substr(0, input.find_last_of('.')) + ".jbc"; r = jau::compile_file(input, output, opt); }
    else if (cmd == "asm") { if (output.empty()) output = "out.s"; if (target.empty()) target = "linux-x86_64"; r = jau::emit_assembly(input, output, target, opt); }
    else if (cmd == "obj") { if (target.empty()) target = "linux-x86_64"; if (output.empty()) output = is_windows_target(target) ? "out.obj" : "out.o"; opt.library_mode = true; auto temp = fs::temp_directory_path() / ("jau_obj_" + std::to_string(std::hash<std::string>{}(input + output)) + ".s"); r = jau::emit_assembly(input, temp.string(), target, opt); if (r.ok) { fs::path self = current_executable_path(argv[0]); fs::path assembler = self.parent_path() /
#ifdef _WIN32
            "jauas.exe";
#else
            "jauas";
#endif
            int rc = run_process(assembler,{temp.string(),"-o",output,"--target",target,"--object"}); std::error_code ec; fs::remove(temp, ec); if (rc != 0) r = {false, "internal jauas object build failed"}; else {write_link_metadata(output,target,"jau_fn_main",opt.optimize,"jau-object");r = {true, "object built: " + output + " (" + target + ", C ABI); metadata: " + output + ".jmeta"};} } }
    else if (cmd == "native") { if (target.empty()) target = "linux-x86_64"; if (output.empty()) output = is_windows_target(target)?"jau-app.exe":"a.out"; r = is_windows_target(target)?windows_native_build(argv[0],input,output,target,opt):jau::native_build(input, output, target, opt); }
    else if (cmd == "standalone") { if (output.empty()) output = "jau-app"; if (runtime.empty()) { fs::path self = current_executable_path(argv[0]); runtime = (self.parent_path() /
#ifdef _WIN32
            "jur.exe"
#else
            "jur"
#endif
        ).string(); } r = jau::bundle_executable(input, output, runtime, opt); }
    else { help(); return 2; }
    if (!r.ok) { std::cerr << "jauc: " << r.message << "\n"; return 1; } if (!r.message.empty()) std::cout << r.message << "\n"; return 0;
}
