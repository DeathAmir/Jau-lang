#include "jau/jau.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
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
static uint16_t coff16(const std::string&b,size_t p){if(p+2>b.size())throw std::runtime_error("truncated COFF");return (uint16_t)((unsigned char)b[p]|((unsigned char)b[p+1]<<8));}
static uint32_t coff32(const std::string&b,size_t p){if(p+4>b.size())throw std::runtime_error("truncated COFF");return (uint32_t)(unsigned char)b[p]|((uint32_t)(unsigned char)b[p+1]<<8)|((uint32_t)(unsigned char)b[p+2]<<16)|((uint32_t)(unsigned char)b[p+3]<<24);}
static std::string coff_name8(const std::string&b,size_t p){size_t n=0;while(n<8&&p+n<b.size()&&b[p+n])++n;return b.substr(p,n);}
static std::vector<std::string> coff_defined_symbols(const fs::path&p){
    std::string b=read_text(p);std::vector<std::string> out;if(b.size()<20)return out;uint32_t sp=coff32(b,8),ns=coff32(b,12);size_t strbase=(size_t)sp+(size_t)ns*18u;if(sp==0||strbase>b.size())return out;
    auto str_at=[&](uint32_t off){if(off<4||strbase+off>=b.size())return std::string();size_t q=strbase+off,e=q;while(e<b.size()&&b[e])++e;return b.substr(q,e-q);};
    for(uint32_t i=0;i<ns;){size_t q=(size_t)sp+(size_t)i*18u;if(q+18>b.size())break;uint32_t z=coff32(b,q);std::string name=z==0?str_at(coff32(b,q+4)):coff_name8(b,q);int16_t sec=(int16_t)coff16(b,q+12);uint8_t storage=(uint8_t)b[q+16],aux=(uint8_t)b[q+17];if(sec>0&&storage==2&&!name.empty())out.push_back(name);i+=1u+aux;}
    return out;
}
static uint64_t elf64le(const std::string&b,size_t p){if(p+8>b.size())throw std::runtime_error("truncated ELF");uint64_t v=0;for(int i=7;i>=0;--i)v=(v<<8)|(unsigned char)b[p+i];return v;}
static std::vector<std::string> elf_defined_symbols(const fs::path&p){
    std::string b=read_text(p);std::vector<std::string> out;if(b.size()<52||b[0]!=0x7f||b[1]!='E'||b[2]!='L'||b[3]!='F')return out;unsigned cls=(unsigned char)b[4],data=(unsigned char)b[5];if((cls!=1&&cls!=2)||data!=1)throw std::runtime_error("static archive supports little-endian ELF32/ELF64 objects");
    auto u16=[&](size_t q){return coff16(b,q);};auto u32=[&](size_t q){return coff32(b,q);};auto u64=[&](size_t q){return elf64le(b,q);};
    uint64_t shoff=cls==2?u64(40):u32(32);uint16_t shents=cls==2?u16(58):u16(46),shnum=cls==2?u16(60):u16(48);if(!shoff||!shents||shoff+(uint64_t)shents*shnum>b.size())return out;
    auto shtype=[&](size_t h){return u32(h+4);};auto shoffset=[&](size_t h)->uint64_t{return cls==2?u64(h+24):u32(h+16);};auto shsize=[&](size_t h)->uint64_t{return cls==2?u64(h+32):u32(h+20);};auto shlink=[&](size_t h){return cls==2?u32(h+40):u32(h+24);};auto shentsize=[&](size_t h)->uint64_t{return cls==2?u64(h+56):u32(h+36);};
    std::set<std::string> seen;
    for(uint16_t si=0;si<shnum;++si){size_t h=(size_t)shoff+(size_t)si*shents;if(shtype(h)!=2)continue;uint64_t so=shoffset(h),ss=shsize(h),se=shentsize(h);uint32_t li=shlink(h);if(!se||li>=shnum||so+ss>b.size())continue;size_t lh=(size_t)shoff+(size_t)li*shents;uint64_t stro=shoffset(lh),strs=shsize(lh);if(stro+strs>b.size())continue;
        for(uint64_t q=so;q+se<=so+ss;q+=se){uint32_t no=u32((size_t)q);uint8_t info=(uint8_t)b[(size_t)q+(cls==2?4:12)];uint16_t ndx=u16((size_t)q+(cls==2?6:14));unsigned bind=info>>4;if(ndx==0||(bind!=1&&bind!=2)||no>=strs)continue;size_t a=(size_t)(stro+no),e=a;while(e<stro+strs&&e<b.size()&&b[e])++e;if(e>a){std::string n=b.substr(a,e-a);if(seen.insert(n).second)out.push_back(n);}}
    }return out;
}
static void ar_be32(std::string&b,uint32_t v){b.push_back((char)(v>>24));b.push_back((char)(v>>16));b.push_back((char)(v>>8));b.push_back((char)v);}
static void ar_le32(std::string&b,uint32_t v){b.push_back((char)v);b.push_back((char)(v>>8));b.push_back((char)(v>>16));b.push_back((char)(v>>24));}
static void ar_le16(std::string&b,uint16_t v){b.push_back((char)v);b.push_back((char)(v>>8));}
static std::string ar_field(std::string v,size_t n){if(v.size()>n)v.resize(n);v.append(n-v.size(),' ');return v;}
static std::string ar_header(const std::string&name,size_t size){return ar_field(name,16)+ar_field("0",12)+ar_field("0",6)+ar_field("0",6)+ar_field("100644",8)+ar_field(std::to_string(size),10)+"`\n";}
static size_t ar_span(size_t n){return 60+n+(n&1u);}
static void ar_member(std::string&out,const std::string&name,const std::string&data){out+=ar_header(name,data.size());out+=data;if(data.size()&1u)out.push_back('\n');}
static void write_binary(const fs::path&p,const std::string&data);
static void write_static_archive(const fs::path&object,const fs::path&output,const std::string&target){
    std::string obj=read_text(object);if(obj.empty())throw std::runtime_error("cannot read object for static archive: "+object.string());bool win=is_windows_target(target);auto symbols=win?coff_defined_symbols(object):elf_defined_symbols(object);std::sort(symbols.begin(),symbols.end());symbols.erase(std::unique(symbols.begin(),symbols.end()),symbols.end());if(symbols.empty())throw std::runtime_error("object has no externally defined symbols for static library");
    size_t names=0;for(auto&x:symbols)names+=x.size()+1;size_t first_size=4+symbols.size()*4+names;size_t second_size=win?(4+4+4+symbols.size()*2+names):0;uint32_t obj_off=(uint32_t)(8+ar_span(first_size)+(win?ar_span(second_size):0));
    std::string first;ar_be32(first,(uint32_t)symbols.size());for(size_t i=0;i<symbols.size();++i)ar_be32(first,obj_off);for(auto&x:symbols){first+=x;first.push_back(0);}
    std::string second;if(win){ar_le32(second,1);ar_le32(second,obj_off);ar_le32(second,(uint32_t)symbols.size());for(size_t i=0;i<symbols.size();++i)ar_le16(second,1);for(auto&x:symbols){second+=x;second.push_back(0);}}
    std::string archive="!<arch>\n";ar_member(archive,"/",first);if(win)ar_member(archive,"/",second);ar_member(archive,win?"jau.obj/":"jau.o/",obj);write_binary(output,archive);
}
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
    std::vector<std::string> symbols;
    std::vector<std::string> system_libs;
    std::vector<std::string> imports;
    int serial=0;
    void add_unique(std::vector<std::string>&dst,const std::string&v){if(!v.empty()&&std::find(dst.begin(),dst.end(),v)==dst.end())dst.push_back(v);}
    void collect_link_manifest(const std::string&mf){
        std::string tk=target;for(char&c:tk)if(c=='-')c='_';std::string platform=is_windows_target(target)?"windows":"linux";
        for(auto&key:std::vector<std::string>{"system_libs","system_libs_"+platform,"system_libs_"+tk})for(auto&v:split_csv(manifest_value(mf,key)))add_unique(system_libs,v);
        for(auto&key:std::vector<std::string>{"imports","imports_"+platform,"imports_"+tk})for(auto&v:split_csv(manifest_value(mf,key)))add_unique(imports,v);
    }
    fs::path resolve_local(const fs::path& origin,const std::string& imp){std::vector<fs::path> cands{origin.parent_path()/imp};for(auto&i:options.import_paths)cands.push_back(fs::path(i)/imp);cands.push_back(jau_home_path()/"stdlib"/imp);cands.push_back(fs::current_path()/"stdlib"/imp);for(auto&c:cands)if(fs::exists(c))return c;return {};}
    void scan_text(const std::string& text,const fs::path& origin){std::regex re(R"(^\s*import\s+\"([^\"]+)\"\s*;?\s*$)");std::istringstream in(text);std::string line;while(std::getline(in,line)){std::smatch m;if(!std::regex_match(line,m,re))continue;std::string imp=m[1].str();if(imp.rfind("pkg:",0)==0)package(imp.substr(4));else{auto p=resolve_local(origin,imp);if(!p.empty())file(p);}}}
    void file(const fs::path&p){std::error_code ec;auto a=fs::weakly_canonical(p,ec);std::string k=ec?p.string():a.string();if(!files.insert(k).second)return;scan_text(read_text(p),p);}
    void package(const std::string& spec){
        auto slash=spec.find('/');std::string name=slash==std::string::npos?spec:spec.substr(0,slash),sub=slash==std::string::npos?"":spec.substr(slash+1);if(!packages.insert(name).second)return;
        fs::path archive=jau_home_path()/"packages"/name/"package.jaup";if(!fs::exists(archive))return;
        std::string mf=jau::package_manifest(archive.string());std::string type=lower_copy_local(manifest_value(mf,"type"));collect_link_manifest(mf);
        std::string members=manifest_value(mf,native_key(target));if(members.empty())members=manifest_value(mf,"native");
        bool native_pkg=(type=="native"||!members.empty());
        if(native_pkg){
            if(members.empty())throw std::runtime_error("native package "+name+" has no object for target "+target+"; expected manifest key "+native_key(target));
            for(auto&member:split_csv(members)){
                auto ext=lower_copy_local(fs::path(member).extension().string());
                if(ext!=".obj"&&ext!=".o")throw std::runtime_error("native package "+name+" member is not an object file: "+member);
                std::string bytes=jau::package_read_file(archive.string(),member);
                fs::path p=temp/("pkg_"+std::to_string(serial++)+"_"+fs::path(member).filename().string());
                write_binary(p,bytes);objects.push_back(p.string());
                if(is_windows_target(target)){auto syms=coff_defined_symbols(p);symbols.insert(symbols.end(),syms.begin(),syms.end());}
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

struct NativePackageInfo {std::vector<std::string> objects,symbols,system_libs,imports;};
static NativePackageInfo collect_native_package_info(const std::string& input,const std::string& target,const fs::path&temp,const jau::CompileOptions&opt){NativeCollector c{target,temp,opt};c.file(input);return {c.objects,c.symbols,c.system_libs,c.imports};}
static bool is_cpp_ext(const std::string&e){return e==".cpp"||e==".cxx"||e==".cc"||e==".c++";}
static std::string lower_ext(fs::path p){auto e=p.extension().string();for(char&c:e)c=(char)std::tolower((unsigned char)c);return e;}
static void append_unique(std::vector<std::string>&dst,const std::vector<std::string>&src){for(auto&s:src)if(std::find(dst.begin(),dst.end(),s)==dst.end())dst.push_back(s);}
static jau::Result emit_with_native_symbols(const std::string&input,const std::string&output,const std::string&target,jau::CompileOptions opt){
    if(!is_windows_target(target))return jau::emit_assembly(input,output,target,opt);
    fs::path tmp=fs::temp_directory_path()/("jau_symbols_"+std::to_string(std::hash<std::string>{}(input+target)));std::error_code ec;fs::remove_all(tmp,ec);fs::create_directories(tmp);
    try{auto info=collect_native_package_info(input,target,tmp,opt);auto syms=info.symbols;for(auto&x:opt.native_inputs){auto e=lower_ext(x);if(e==".obj"||e==".o")append_unique(syms,coff_defined_symbols(x));}append_unique(opt.native_symbols,syms);if(opt.debug){std::cerr<<"[jauc:resolve] native objects="<<info.objects.size()<<" symbols="<<syms.size()<<"\n";for(auto&o:info.objects)std::cerr<<"[jauc:object] "<<o<<"\n";for(auto&z:syms)std::cerr<<"[jauc:symbol] "<<z<<"\n";}auto r=jau::emit_assembly(input,output,target,opt);fs::remove_all(tmp,ec);return r;}catch(const std::exception&e){fs::remove_all(tmp,ec);return {false,std::string("native symbol discovery failed: ")+e.what()};}
}
static bool compile_native_input(const std::string& src,const std::string& target,const fs::path& out){
    std::string e=lower_ext(src);bool cpp=is_cpp_ext(e);if(e!=".c"&&!cpp)return false;const char* env=std::getenv(cpp?"JAU_CXX":"JAU_CC");std::string cc=env?env:(cpp?"g++":"gcc");if(target=="windows-x86"&&!env)cc=cpp?"i686-w64-mingw32-g++":"i686-w64-mingw32-gcc";std::string cmd=quote(cc)+" -c -O2 -ffunction-sections -fdata-sections ";if(cpp)cmd+="-fno-exceptions -fno-rtti ";cmd+=quote(src)+" -o "+quote(out.string());return std::system(cmd.c_str())==0;
}

static jau::Result windows_native_build(const char* argv0,const std::string&input,std::string output,const std::string&target,jau::CompileOptions opt,bool shared=false){
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
        auto info=collect_native_package_info(input,target,tmp,opt);auto pkg=info.objects;auto discovered=info.symbols;for(auto&x:opt.native_inputs){auto e=lower_ext(x);if(e==".obj"||e==".o")append_unique(discovered,coff_defined_symbols(x));}append_unique(opt.native_symbols,discovered);append_unique(opt.system_libs,info.system_libs);append_unique(opt.native_imports,info.imports);
        if(opt.debug){std::cerr<<"[jauc:resolve] target="<<target<<" package_objects="<<pkg.size()<<" native_symbols="<<discovered.size()<<" system_libs="<<opt.system_libs.size()<<" imports="<<opt.native_imports.size()<<"\n";for(auto&p:pkg)std::cerr<<"[jauc:object] "<<p<<"\n";for(auto&z:discovered)std::cerr<<"[jauc:symbol] "<<z<<"\n";for(auto&z:opt.system_libs)std::cerr<<"[jauc:system-lib] "<<z<<"\n";for(auto&z:opt.native_imports)std::cerr<<"[jauc:import] "<<z<<"\n";}
        fs::path asmp=tmp/"app.s",obj=tmp/"app.obj";opt.library_mode=shared;opt.shared=shared;auto r=jau::emit_assembly(input,asmp.string(),target,opt);if(!r.ok){fs::remove_all(tmp,ec);return {false,"AOT stage: "+r.message};}
        if(run_process(assembler,{asmp.string(),"-o",obj.string(),"--target",target,"--object"})!=0){fs::remove_all(tmp,ec);return {false,"assembler stage: internal jauas object build failed"};}
        std::vector<std::string> objs{obj.string()};objs.insert(objs.end(),pkg.begin(),pkg.end());int serial=0;
        for(auto&x:opt.native_inputs){auto e=lower_ext(x);if(e==".obj"||e==".o")objs.push_back(x);else if(e==".c"||is_cpp_ext(e)){fs::path p=tmp/("link_"+std::to_string(serial++)+".obj");if(!compile_native_input(x,target,p)){fs::remove_all(tmp,ec);return {false,"failed to compile native C/C++ input: "+x+" (set JAU_CC/JAU_CXX if needed)"};}objs.push_back(p.string());}else{fs::remove_all(tmp,ec);return {false,"unsupported Windows native input: "+x+" (use .c/.cpp/.obj)"};}}
        if(shared){if(lower_ext(output)!=".dll")output+=".dll";}else if(fs::path(output).extension()!=".exe")output+=".exe";std::vector<std::string> args=objs;args.push_back("-o");args.push_back(output);args.push_back("--target");args.push_back(target);if(shared){args.push_back("--dll");for(auto&x:opt.exports){args.push_back("--export");args.push_back(x);}}else{args.push_back("--entry");args.push_back("main");args.push_back("--subsystem");args.push_back(opt.subsystem);}for(auto&x:opt.system_libs){args.push_back("--system-lib");args.push_back(x);}for(auto&x:opt.native_imports){args.push_back("--import");args.push_back(x);}int rc=run_process(linker,args);fs::remove_all(tmp,ec);if(rc!=0)return {false,std::string("link stage: internal jauld PE ")+(shared?"DLL":"EXE")+" link failed"};return {true,std::string(shared?"native shared library built: ":"native executable built without external linker: ")+output};
    }catch(const std::exception&e){return {false,e.what()};}
}

static jau::Result generic_native_build_with_packages(const std::string&input,const std::string&output,const std::string&target,jau::CompileOptions opt){
    fs::path tmp=fs::temp_directory_path()/("jau_pkg_native_"+std::to_string(std::hash<std::string>{}(input+output+target)));std::error_code ec;fs::remove_all(tmp,ec);fs::create_directories(tmp);
    try{auto info=collect_native_package_info(input,target,tmp,opt);append_unique(opt.native_inputs,info.objects);append_unique(opt.system_libs,info.system_libs);append_unique(opt.native_imports,info.imports);if(!opt.native_imports.empty()){fs::remove_all(tmp,ec);return {false,"imports_* manifest/--import is a Windows PE feature; Linux uses normal -l/system libraries"};}auto r=jau::native_build(input,output,target,opt);fs::remove_all(tmp,ec);return r;}catch(const std::exception&e){fs::remove_all(tmp,ec);return {false,e.what()};}
}

static void print_diagnostic(const std::string&input,const std::string&stage,const std::string&message){
    std::cerr<<"jauc["<<stage<<"]: "<<message<<"\n";std::smatch m;std::regex re("(?:at |line )line? ?([0-9]+)|line ([0-9]+)");if(std::regex_search(message,m,re)){std::string n=m[1].matched?m[1].str():m[2].str();try{int ln=std::stoi(n);std::ifstream f(input);std::string line;for(int i=1;i<=ln&&std::getline(f,line);++i)if(i==ln){std::cerr<<"  --> "<<input<<":"<<ln<<"\n  "<<ln<<" | "<<line<<"\n";break;}}catch(...){}}
}
static void help() {
    std::cout << "Jau compiler " << jau::version() << "\nusage:\n"
              << "  jauc build <file.jau> [-o out.jbc] [-O0|-O1|-O2|-O3]\n"
              << "  jauc run <file.jau>\n"
              << "  jauc debug <file.jau> [-- args...]\n"
              << "  jauc check <file.jau> [--target <target>]\n"
              << "  jauc asm <file.jau> -o out.s --target <linux-x86_64|linux-x86|windows-x86_64|windows-x86> [--library]\n"
              << "  jauc obj <file.jau> -o out.o|out.obj --target <target>\n"
              << "  jauc native <file.jau> -o program --target <target> [--link native.obj] [--system-lib name] [--import symbol=dll] [--subsystem console|windows] [-O0..-O3]\n"
              << "  jauc shared <file.jau> -o library --target <target> --export name [--export public=internal] [--system-lib name]\n"
              << "  jauc static <file.jau> -o library.lib|library.a --target <target>\n"
              << "  jauc targets\n"
              << "  jauc standalone <file.jau> -o program [--runtime path/to/jur]\n"
              << "  jauc --version\n\n"
              << "Windows native uses Jau's internal jauas + jauld PE32/PE32+ linker. Native defaults to -O3 unless an optimization level is supplied.\n"
              << "jauc obj emits <object>.jmeta (JAUMETA1) so jauld can infer target, ABI, subsystem and entry symbol.\n"
              << "Native .jaux packages can provide target objects plus system_libs_<platform>/imports_<platform> linker metadata.\n"
              << "Windows --system-lib resolves real DLL exports (or common fallback maps) inside jauld; --import symbol=dll is explicit.\n"
              << "AOT interoperability: extern func c_function(a, b); Jau exports jau_fn_<name>.\n";
}

static int jauc_main_impl(int argc, char** argv) {
    if (argc < 2) { help(); return 0; }
    std::string cmd = argv[1];
    if (cmd == "--version" || cmd == "version") { std::cout << jau::version() << "\n"; return 0; }
    if (cmd == "targets") { std::cout << "linux-x86_64\nlinux-x86\nwindows-x86_64\nwindows-x86\n"; return 0; }
    if (argc < 3) { help(); return 2; }
    std::string input = argv[2], output, target, runtime; std::vector<std::string> runargs; jau::CompileOptions opt; bool optimize_set=false;
    for (int i = 3; i < argc; ++i) { std::string a = argv[i]; if (a == "--") { for (++i; i < argc; ++i) runargs.push_back(argv[i]); break; } if (a == "-o" && i + 1 < argc) output = argv[++i]; else if (a == "--target" && i + 1 < argc) target = argv[++i]; else if (a == "--runtime" && i + 1 < argc) runtime = argv[++i]; else if (a == "--library") opt.library_mode = true; else if (a == "--debug") opt.debug = true; else if (a == "--link" && i + 1 < argc) opt.native_inputs.push_back(argv[++i]); else if (a == "--system-lib" && i + 1 < argc) opt.system_libs.push_back(argv[++i]); else if (a == "--import" && i + 1 < argc) opt.native_imports.push_back(argv[++i]); else if (a == "--subsystem" && i + 1 < argc) opt.subsystem=lower_copy_local(argv[++i]); else if (a == "--export" && i + 1 < argc) opt.exports.push_back(argv[++i]); else if (a == "-I" && i + 1 < argc) opt.import_paths.push_back(argv[++i]); else if (a.rfind("-O", 0) == 0 && a.size() == 3 && a[2]>='0'&&a[2]<='3') {opt.optimize = a[2] - '0';optimize_set=true;} }
    if((cmd=="native"||cmd=="shared"||cmd=="static")&&!optimize_set)opt.optimize=0; // correctness-first native default
    if(opt.subsystem!="console"&&opt.subsystem!="windows"){std::cerr<<"jauc: --subsystem must be console or windows\n";return 2;}
    jau::Result r;
    if (cmd == "run") r = jau::run_source(input, opt, runargs);
    else if (cmd == "debug") { opt.debug=true; r = jau::run_source(input,opt,runargs); }
    else if (cmd == "check") { fs::path tmp=fs::temp_directory_path()/("jau_check_"+std::to_string(std::hash<std::string>{}(input+target))+(target.empty()?".jbc":".s")); if(target.empty())r=jau::compile_file(input,tmp.string(),opt);else r=emit_with_native_symbols(input,tmp.string(),target,opt);std::error_code ec;fs::remove(tmp,ec);if(r.ok)r={true,"check passed: "+input}; }
    else if (cmd == "build") { if (output.empty()) output = input.substr(0, input.find_last_of('.')) + ".jbc"; r = jau::compile_file(input, output, opt); }
    else if (cmd == "asm") { if (output.empty()) output = "out.s"; if (target.empty()) target = "linux-x86_64"; r = emit_with_native_symbols(input, output, target, opt); }
    else if (cmd == "obj") { if (target.empty()) target = "linux-x86_64"; if (output.empty()) output = is_windows_target(target) ? "out.obj" : "out.o"; opt.library_mode = true; auto temp = fs::temp_directory_path() / ("jau_obj_" + std::to_string(std::hash<std::string>{}(input + output)) + ".s"); r = emit_with_native_symbols(input, temp.string(), target, opt); if (r.ok) { fs::path self = current_executable_path(argv[0]); fs::path assembler = self.parent_path() /
#ifdef _WIN32
            "jauas.exe";
#else
            "jauas";
#endif
            int rc = run_process(assembler,{temp.string(),"-o",output,"--target",target,"--object"}); std::error_code ec; fs::remove(temp, ec); if (rc != 0) r = {false, "internal jauas object build failed"}; else {write_link_metadata(output,target,"jau_fn_main",opt.optimize,"jau-object");r = {true, "object built: " + output + " (" + target + ", C ABI); metadata: " + output + ".jmeta"};} } }
    else if (cmd == "native") { if (target.empty()) target = "linux-x86_64"; if (output.empty()) output = is_windows_target(target)?"jau-app.exe":"a.out"; r = is_windows_target(target)?windows_native_build(argv[0],input,output,target,opt):generic_native_build_with_packages(input, output, target, opt); }
    else if (cmd == "shared") { if(target.empty())target="linux-x86_64";if(opt.exports.empty()){std::cerr<<"jauc: shared requires at least one --export name\n";return 2;}opt.shared=true;opt.library_mode=true;if(output.empty())output=is_windows_target(target)?"jau-lib.dll":"libjau.so";else if(!is_windows_target(target)&&lower_ext(output)!=".so")output+=".so";r=is_windows_target(target)?windows_native_build(argv[0],input,output,target,opt,true):generic_native_build_with_packages(input,output,target,opt); }
    else if (cmd == "static") { if(target.empty())target="linux-x86_64";if(output.empty())output=is_windows_target(target)?"jau-lib.lib":"libjau.a";else if(is_windows_target(target)&&lower_ext(output)!=".lib")output+=".lib";else if(!is_windows_target(target)&&lower_ext(output)!=".a")output+=".a";opt.library_mode=true;fs::path td=fs::temp_directory_path()/("jau_static_"+std::to_string(std::hash<std::string>{}(input+output+target)));std::error_code ec;fs::remove_all(td,ec);fs::create_directories(td);fs::path asmp=td/"library.s",obj=td/(is_windows_target(target)?"library.obj":"library.o");r=emit_with_native_symbols(input,asmp.string(),target,opt);if(r.ok){fs::path self=current_executable_path(argv[0]);fs::path assembler=self.parent_path()/
#ifdef _WIN32
            "jauas.exe";
#else
            "jauas";
#endif
            int rc=run_process(assembler,{asmp.string(),"-o",obj.string(),"--target",target,"--object"});if(rc!=0)r={false,"internal jauas static object build failed"};else try{write_static_archive(obj,output,target);r={true,"static library built: "+output+" ("+target+")"};}catch(const std::exception&e){r={false,e.what()};}}fs::remove_all(td,ec); }
    else if (cmd == "standalone") { if (output.empty()) output = "jau-app"; if (runtime.empty()) { fs::path self = current_executable_path(argv[0]); runtime = (self.parent_path() /
#ifdef _WIN32
            "jur.exe"
#else
            "jur"
#endif
        ).string(); } r = jau::bundle_executable(input, output, runtime, opt); }
    else { help(); return 2; }
    if (!r.ok) { print_diagnostic(input,cmd,r.message); return 1; } if (!r.message.empty()) std::cout << r.message << "\n"; return 0;
}

int main(int argc,char**argv){std::cerr<<jau::copyright_notice()<<"\n";try{return jauc_main_impl(argc,argv);}catch(const std::exception&e){std::cerr<<"jauc[fatal]: "<<e.what()<<"\n";return 1;}catch(...){std::cerr<<"jauc[fatal]: unknown internal error\n";return 1;}}
