from pathlib import Path


def read(p):
    return Path(p).read_text(encoding="utf-8")


def write(p, s):
    Path(p).parent.mkdir(parents=True, exist_ok=True)
    Path(p).write_text(s, encoding="utf-8")


def rep(s, old, new, label):
    if old not in s:
        raise SystemExit(f"patch anchor not found: {label}")
    return s.replace(old, new, 1)

# -----------------------------------------------------------------------------
# Public compile options: linker/system library information belongs to the
# compile request so CLI, package manifests and JauM use one pipeline.
# -----------------------------------------------------------------------------
p = "include/jau/jau.hpp"
s = read(p)
s = rep(s,
'''    std::vector<std::string> native_inputs;
    std::vector<std::string> native_symbols;
    bool debug = false;
''',
'''    std::vector<std::string> native_inputs;
    std::vector<std::string> native_symbols;
    // Native linker requests. system_libs contains logical names (user32,
    // pthread, m, ...) or explicit library paths. native_imports uses
    // symbol=dll syntax for deterministic Windows PE imports.
    std::vector<std::string> system_libs;
    std::vector<std::string> native_imports;
    std::vector<std::string> exports;
    std::string subsystem = "console";
    bool shared = false;
    bool debug = false;
''', "CompileOptions native linker fields")
write(p, s)

# -----------------------------------------------------------------------------
# AOT: generated assembly/binaries are user artifacts, so do not brand them.
# Also allow immutable string literals to cross the native ABI as const char*.
# This is intentionally a borrowed pointer valid for the lifetime of the image.
# -----------------------------------------------------------------------------
p = "src/jau_part03.inc"
s = read(p)
s = rep(s,
'''            if(auto p=std::get_if<bool>(&e->v.data)){o<<"  mov eax, "<<(*p?1:0)<<"\\n";push(is64?"rax":"eax");return;}
            throw Error("AOT supports integer/bool values; strings/arrays remain VM-only");
''',
'''            if(auto p=std::get_if<bool>(&e->v.data)){o<<"  mov eax, "<<(*p?1:0)<<"\\n";push(is64?"rax":"eax");return;}
            if(auto p=std::get_if<std::string>(&e->v.data)){
                // Native string literals are emitted in .rodata and passed as a
                // borrowed const char*. This makes C ABI calls such as Win32 A
                // APIs practical without pretending Jau has a native owned-string ABI.
                auto lab=intern_string(*p);
                if(is64)o<<"  lea rax, [rip+"<<lab<<"]\\n";
                else o<<"  mov eax, OFFSET FLAT:"<<lab<<"\\n";
                push(is64?"rax":"eax");return;
            }
            throw Error("AOT supports integer/bool and string-literal pointer values; dynamic strings/arrays remain VM-only");
''', "AOT string literal ABI")
s = rep(s,
'''        o<<".intel_syntax noprefix\\n# DeathAmir Jau @ DeathAmir 2026 (C)\\n.section .rodata\\nfmt_i: .asciz \\\""<<(is64?"%lld":"%d")<<"\\\\n\\\"\\njau_copyright: .asciz \\\"DeathAmir Jau @ DeathAmir 2026 (C)\\\"\\n.text\\n.extern printf\\n.extern puts\\n";
''',
'''        // Generated assembly is a user artifact. Keep tool branding in CLI
        // stderr only; never inject it into source/object/executable output.
        o<<".intel_syntax noprefix\\n.section .rodata\\nfmt_i: .asciz \\\""<<(is64?"%lld":"%d")<<"\\\\n\\\"\\n.text\\n.extern printf\\n.extern puts\\n";
''', "remove generated copyright")
# Linux/system linker support. Keep the current AOT target set honest.
s = rep(s,
'''    for(auto&x:o.native_inputs)cc+=" "+shell_quote(x);
    std::string final_out=output;if(target.rfind("windows-",0)==0&&lower_copy(fs::path(final_out).extension().string())!=".exe")final_out+=".exe";
    cc+=" -o "+shell_quote(final_out);
''',
'''    for(auto&x:o.native_inputs)cc+=" "+shell_quote(x);
    // Linux uses the host/system linker for final ELF generation. Logical
    // library names become -lfoo; explicit .a/.so paths are passed verbatim.
    for(auto lib:o.system_libs){
        auto ext=lower_copy(fs::path(lib).extension().string());
        if(fs::exists(lib)||ext==".a"||ext==".so"||ext==".dylib")cc+=" "+shell_quote(lib);
        else{
            if(lib.rfind("lib",0)==0)lib=lib.substr(3);
            auto q=lib.find('.');if(q!=std::string::npos)lib=lib.substr(0,q);
            cc+=" -l"+lib;
        }
    }
    std::string final_out=output;if(target.rfind("windows-",0)==0&&lower_copy(fs::path(final_out).extension().string())!=".exe")final_out+=".exe";
    cc+=" -o "+shell_quote(final_out);
''', "native system libs")
s = rep(s, 'std::string version(){return "0.8.2";}', 'std::string version(){return "0.9.0";}', "version")
write(p, s)

# -----------------------------------------------------------------------------
# Standalone programs should not inherit the jur tool banner. Keep the banner
# for the actual jur/jaupm command tools based on the executable image name.
# -----------------------------------------------------------------------------
p = "src/jur_main.cpp"
s = read(p)
s = rep(s,
'''int main(int argc, char** argv) {
    std::cerr << jau::copyright_notice() << "\\n";
    std::vector<std::string> embedded_args;
''',
'''int main(int argc, char** argv) {
    const auto image = current_executable_path(argv[0]);
    auto stem = fs::path(image).stem().string();
    for(char& c:stem)c=(char)std::tolower((unsigned char)c);
    // A bundled user application is not a Jau command-line tool. Do not inject
    // tool branding into its stderr. jur and the bundled jaupm tool keep it.
    if(stem=="jur"||stem=="jaupm") std::cerr << jau::copyright_notice() << "\\n";
    std::vector<std::string> embedded_args;
''', "jur banner policy")
s = s.replace('jau::run_embedded_executable(current_executable_path(argv[0]), embedded_args)', 'jau::run_embedded_executable(image, embedded_args)')
write(p, s)

# -----------------------------------------------------------------------------
# jauld: dynamic Windows system-library resolver. --system-lib can inspect the
# export table of a real DLL (System32 or explicit path). Common DLLs also have
# a fallback symbol list so cross-linking remains useful when the DLL is absent.
# --import symbol=dll is the deterministic escape hatch.
# -----------------------------------------------------------------------------
p = "src/jauld_part00.inc"
s = read(p)
s = rep(s, '#include <map>\n', '#include <map>\n#include <optional>\n#include <cstdlib>\n#include <sstream>\n', "jauld includes")
write(p, s)

p = "src/jauld_part01.inc"
s = read(p)
anchor = '''static bool is_indirect_import(const std::string&n){return n.rfind("__imp_",0)==0||n.rfind("_imp__",0)==0;}
static std::string import_dll(const std::string&canon){
'''
insert = r'''static bool is_indirect_import(const std::string&n){return n.rfind("__imp_",0)==0||n.rfind("_imp__",0)==0;}

// Explicit imports and discovered DLL exports are process-local linker state.
// jauld performs one link per process, so keeping this resolver outside LinkState
// avoids threading configuration through every relocation helper.
static std::unordered_map<std::string,std::string> g_import_overrides;
static std::unordered_map<std::string,std::string> g_system_imports;
static constexpr const char* AMBIGUOUS_DLL="<ambiguous>";

static std::string dll_basename(std::string s){
    fs::path p=s;std::string n=p.filename().string();if(n.empty())n=s;
    auto l=lower(n);if(l.size()<4||l.substr(l.size()-4)!=".dll")n+=".dll";return n;
}
static std::vector<fs::path> system_dll_candidates(const std::string&spec){
    std::vector<fs::path> out;fs::path p=spec;out.push_back(p);
    std::string n=dll_basename(spec);if(p.extension().empty())out.push_back(p.parent_path()/n);
    if(const char* env=std::getenv("JAU_SYSTEM_LIB_PATH")){
        std::string v=env,cur;char sep=';';
#ifndef _WIN32
        sep=':';
#endif
        std::istringstream in(v);while(std::getline(in,cur,sep))if(!cur.empty())out.push_back(fs::path(cur)/n);
    }
    if(const char* root=std::getenv("SystemRoot"))out.push_back(fs::path(root)/"System32"/n);
    if(const char* root=std::getenv("WINDIR"))out.push_back(fs::path(root)/"System32"/n);
    out.push_back(fs::current_path()/n);return out;
}
static size_t pe_rva_to_offset(const std::vector<uint8_t>&b,uint32_t rva,size_t sh,uint16_t nsec){
    for(uint16_t i=0;i<nsec;++i){size_t p=sh+(size_t)i*40u;if(p+40>b.size())break;uint32_t vs=r32(b,p+8),va=r32(b,p+12),rs=r32(b,p+16),rp=r32(b,p+20),span=std::max(vs,rs);if(rva>=va&&rva<va+span){size_t off=(size_t)rp+(rva-va);if(off>=b.size())throw std::runtime_error("DLL export RVA points outside file");return off;}}
    throw std::runtime_error("DLL export RVA not covered by a section");
}
static std::vector<std::string> pe_export_names(const fs::path&path){
    auto b=read_file(path);if(b.size()<0x100||b[0]!='M'||b[1]!='Z')throw std::runtime_error("not a PE DLL: "+path.string());uint32_t pe=r32(b,0x3c);if((size_t)pe+24>b.size()||b[pe]!='P'||b[pe+1]!='E')throw std::runtime_error("invalid PE DLL: "+path.string());uint16_t nsec=r16(b,pe+6),optsz=r16(b,pe+20);size_t opt=pe+24;if(opt+optsz>b.size())throw std::runtime_error("truncated PE optional header: "+path.string());uint16_t magic=r16(b,opt);size_t dirs=opt+(magic==0x20b?112u:magic==0x10b?96u:0u);if(!dirs||dirs+8>b.size())throw std::runtime_error("unsupported PE optional header: "+path.string());uint32_t erva=r32(b,dirs);if(!erva)return {};size_t sh=opt+optsz,ed=pe_rva_to_offset(b,erva,sh,nsec);if(ed+40>b.size())throw std::runtime_error("truncated PE export directory");uint32_t nn=r32(b,ed+24),names=r32(b,ed+32);size_t no=pe_rva_to_offset(b,names,sh,nsec);std::vector<std::string> out;for(uint32_t i=0;i<nn;++i){if(no+(size_t)i*4+4>b.size())break;uint32_t nrva=r32(b,no+(size_t)i*4);size_t q=pe_rva_to_offset(b,nrva,sh,nsec),e=q;while(e<b.size()&&b[e])++e;if(e>q)out.emplace_back((const char*)&b[q],e-q);}return out;
}
static std::vector<std::string> fallback_exports(const std::string&dll){
    auto d=lower(dll_basename(dll));
    if(d=="user32.dll")return {"MessageBoxA","MessageBoxW","MessageBeep","GetSystemMetrics","RegisterClassA","RegisterClassW","CreateWindowExA","CreateWindowExW","DefWindowProcA","DefWindowProcW","ShowWindow","UpdateWindow","GetMessageA","GetMessageW","PeekMessageA","PeekMessageW","TranslateMessage","DispatchMessageA","DispatchMessageW","PostQuitMessage","DestroyWindow","SetWindowTextA","SetWindowTextW","GetClientRect","BeginPaint","EndPaint","LoadIconA","LoadIconW","LoadCursorA","LoadCursorW","SetTimer","KillTimer"};
    if(d=="gdi32.dll")return {"TextOutA","TextOutW","CreateSolidBrush","DeleteObject","SelectObject","Rectangle","Ellipse","BitBlt","StretchBlt","CreateCompatibleDC","CreateCompatibleBitmap","SetTextColor","SetBkColor","SetBkMode","MoveToEx","LineTo"};
    if(d=="shell32.dll")return {"ShellExecuteA","ShellExecuteW","SHGetFolderPathA","SHGetFolderPathW"};
    if(d=="ws2_32.dll")return {"WSAStartup","WSACleanup","socket","connect","bind","listen","accept","send","recv","closesocket","getaddrinfo","freeaddrinfo","inet_pton","inet_ntop"};
    if(d=="advapi32.dll")return {"RegOpenKeyExA","RegOpenKeyExW","RegQueryValueExA","RegQueryValueExW","RegSetValueExA","RegSetValueExW","RegCloseKey","OpenProcessToken","LookupPrivilegeValueA","AdjustTokenPrivileges"};
    if(d=="ole32.dll")return {"CoInitialize","CoInitializeEx","CoUninitialize","CoCreateInstance","CoTaskMemFree"};
    if(d=="comdlg32.dll")return {"GetOpenFileNameA","GetOpenFileNameW","GetSaveFileNameA","GetSaveFileNameW","ChooseColorA","ChooseColorW"};
    if(d=="comctl32.dll")return {"InitCommonControls","InitCommonControlsEx","ImageList_Create","ImageList_Destroy"};
    return {};
}
static void register_system_import(const std::string&symbol,const std::string&dll){
    auto c=undecorate(symbol);if(c.empty())return;auto it=g_system_imports.find(c);if(it==g_system_imports.end())g_system_imports[c]=dll;else if(lower(it->second)!=lower(dll))it->second=AMBIGUOUS_DLL;
}
static void register_system_lib(const std::string&spec){
    std::string dll=dll_basename(spec);std::vector<std::string> names;std::string last_error;
    for(auto&p:system_dll_candidates(spec)){std::error_code ec;if(!fs::exists(p,ec)||ec)continue;try{names=pe_export_names(p);dll=p.filename().string();if(!names.empty())break;}catch(const std::exception&e){last_error=e.what();}}
    if(names.empty())names=fallback_exports(dll);
    if(names.empty())throw std::runtime_error("cannot resolve exports for system library '"+spec+"'"+(last_error.empty()?std::string():std::string(": ")+last_error)+"; pass a DLL path, set JAU_SYSTEM_LIB_PATH, or use --import symbol=dll");
    for(auto&n:names)register_system_import(n,dll);
}
static void register_import_override(const std::string&spec){auto q=spec.find('=');if(q==std::string::npos||q==0||q+1>=spec.size())throw std::runtime_error("--import expects symbol=dll");auto sym=undecorate(spec.substr(0,q));g_import_overrides[sym]=dll_basename(spec.substr(q+1));}

static std::string import_dll(const std::string&canon){
'''
s = rep(s, anchor, insert, "system library resolver insertion")
s = rep(s,
'''    if(crt.count(canon))return "msvcrt.dll";if(k32.count(canon))return "kernel32.dll";return "";
}
''',
'''    auto ex=g_import_overrides.find(canon);if(ex!=g_import_overrides.end())return ex->second;
    if(crt.count(canon))return "msvcrt.dll";if(k32.count(canon))return "kernel32.dll";
    auto si=g_system_imports.find(canon);if(si!=g_system_imports.end()){if(si->second==AMBIGUOUS_DLL)throw std::runtime_error("system symbol '"+canon+"' is exported by multiple requested DLLs; disambiguate with --import "+canon+"=name.dll");return si->second;}
    return "";
}
''', "import_dll dynamic maps")
write(p, s)

p = "src/jauld_part02.inc"
s = read(p)
s = rep(s,
'static int link_pe(const std::vector<std::string>&inputs,const std::string&output,const std::string&target,const std::string&entry){',
'static int link_pe(const std::vector<std::string>&inputs,const std::string&output,const std::string&target,const std::string&entry,const std::string&subsystem){',
"link_pe subsystem signature")
write(p, s)

p = "src/jauld_part03.inc"
s = read(p)
s = rep(s,
'''p32(pe,p,0);p+=4;p16(pe,p,3);p+=2;p16(pe,p,0x8100);p+=2;''',
'''p32(pe,p,0);p+=4;uint16_t subsystem_id=lower(subsystem)=="windows"?2:3;p16(pe,p,subsystem_id);p+=2;p16(pe,p,0x8100);p+=2;''', "PE subsystem")
s = rep(s,
'''static void usage(){std::cout<<"jauld 0.4\\nusage: jauld <input.obj> [more.obj ...] -o <program.exe> [--target <windows-x86_64|windows-x86>] [--entry symbol] [--meta file.jmeta]\\nmetadata: JAUMETA1 with target/entry/abi/kind/subsystem/producer/version. If omitted, <first-object>.jmeta is auto-detected.\\n";}
int main(int argc,char**argv){std::cerr<<"DeathAmir Jau @ DeathAmir 2026 (C)\\n";try{
    if(argc<2){usage();return 0;}std::vector<std::string>in;std::string out="a.exe",target,entry,meta_path;
    for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="-o"&&i+1<argc)out=argv[++i];else if(a=="--target"&&i+1<argc)target=argv[++i];else if(a=="--entry"&&i+1<argc)entry=argv[++i];else if(a=="--meta"&&i+1<argc)meta_path=argv[++i];else if(a=="--help"||a=="-h"){usage();return 0;}else in.push_back(a);}
''',
'''static void usage(){std::cout<<"jauld 0.9\\nusage: jauld <input.obj> [more.obj ...] -o <program.exe> [--target <windows-x86_64|windows-x86>] [--entry symbol] [--meta file.jmeta] [--system-lib user32] [--import symbol=dll] [--subsystem console|windows]\\n\\n--system-lib accepts a DLL name/path. On Windows jauld reads the real PE export table; common DLLs have cross-link fallback maps.\\n--import provides an exact symbol=dll mapping when a symbol is ambiguous or a DLL is not locally available.\\n";}
int main(int argc,char**argv){std::cerr<<"DeathAmir Jau @ DeathAmir 2026 (C)\\n";try{
    if(argc<2){usage();return 0;}std::vector<std::string>in,system_libs,imports;std::string out="a.exe",target,entry,meta_path,subsystem="console";
    for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="-o"&&i+1<argc)out=argv[++i];else if(a=="--target"&&i+1<argc)target=argv[++i];else if(a=="--entry"&&i+1<argc)entry=argv[++i];else if(a=="--meta"&&i+1<argc)meta_path=argv[++i];else if(a=="--system-lib"&&i+1<argc)system_libs.push_back(argv[++i]);else if(a=="--import"&&i+1<argc)imports.push_back(argv[++i]);else if(a=="--subsystem"&&i+1<argc)subsystem=lower(argv[++i]);else if(a=="--help"||a=="-h"){usage();return 0;}else in.push_back(a);}
    if(subsystem!="console"&&subsystem!="windows")throw std::runtime_error("--subsystem must be console or windows");
    for(auto&x:system_libs)register_system_lib(x);for(auto&x:imports)register_import_override(x);
''', "jauld CLI system libs")
s = rep(s,
'''    return link_pe(in,out,target,entry);
''',
'''    return link_pe(in,out,target,entry,subsystem);
''', "jauld link call")
write(p, s)

# -----------------------------------------------------------------------------
# jauc package + linker plumbing.
# -----------------------------------------------------------------------------
p = "src/jauc_main.cpp"
s = read(p)
s = rep(s,
'''    std::vector<std::string> objects;
    std::vector<std::string> symbols;
    int serial=0;
''',
'''    std::vector<std::string> objects;
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
''', "NativeCollector link metadata")
s = rep(s,
'''        std::string mf=jau::package_manifest(archive.string());std::string type=lower_copy_local(manifest_value(mf,"type"));
        std::string members=manifest_value(mf,native_key(target));''',
'''        std::string mf=jau::package_manifest(archive.string());std::string type=lower_copy_local(manifest_value(mf,"type"));collect_link_manifest(mf);
        std::string members=manifest_value(mf,native_key(target));''', "collect manifest libs")
s = rep(s,
'''                auto syms=coff_defined_symbols(p);symbols.insert(symbols.end(),syms.begin(),syms.end());
''',
'''                if(is_windows_target(target)){auto syms=coff_defined_symbols(p);symbols.insert(symbols.end(),syms.begin(),syms.end());}
''', "ELF package object safety")
old = '''static std::vector<std::string> collect_native_package_objects(const std::string& input,const std::string& target,const fs::path&temp,const jau::CompileOptions&opt,std::vector<std::string>*symbols=nullptr){NativeCollector c{target,temp,opt};c.file(input);if(symbols)*symbols=c.symbols;return c.objects;}
'''
new = '''struct NativePackageInfo {std::vector<std::string> objects,symbols,system_libs,imports;};
static NativePackageInfo collect_native_package_info(const std::string& input,const std::string& target,const fs::path&temp,const jau::CompileOptions&opt){NativeCollector c{target,temp,opt};c.file(input);return {c.objects,c.symbols,c.system_libs,c.imports};}
'''
s = rep(s, old, new, "native package info API")
s = rep(s,
'''try{std::vector<std::string> syms;auto objs=collect_native_package_objects(input,target,tmp,opt,&syms);for(auto&x:opt.native_inputs){auto e=lower_ext(x);if(e==".obj"||e==".o")append_unique(syms,coff_defined_symbols(x));}append_unique(opt.native_symbols,syms);if(opt.debug){std::cerr<<"[jauc:resolve] native objects="<<objs.size()<<" symbols="<<syms.size()<<"\\n";for(auto&o:objs)std::cerr<<"[jauc:object] "<<o<<"\\n";for(auto&z:syms)std::cerr<<"[jauc:symbol] "<<z<<"\\n";}auto r=jau::emit_assembly(input,output,target,opt);fs::remove_all(tmp,ec);return r;}''',
'''try{auto info=collect_native_package_info(input,target,tmp,opt);auto syms=info.symbols;for(auto&x:opt.native_inputs){auto e=lower_ext(x);if(e==".obj"||e==".o")append_unique(syms,coff_defined_symbols(x));}append_unique(opt.native_symbols,syms);if(opt.debug){std::cerr<<"[jauc:resolve] native objects="<<info.objects.size()<<" symbols="<<syms.size()<<"\\n";for(auto&o:info.objects)std::cerr<<"[jauc:object] "<<o<<"\\n";for(auto&z:syms)std::cerr<<"[jauc:symbol] "<<z<<"\\n";}auto r=jau::emit_assembly(input,output,target,opt);fs::remove_all(tmp,ec);return r;}''', "emit package info")
s = rep(s,
'''        std::vector<std::string> discovered;auto pkg=collect_native_package_objects(input,target,tmp,opt,&discovered);for(auto&x:opt.native_inputs){auto e=lower_ext(x);if(e==".obj"||e==".o")append_unique(discovered,coff_defined_symbols(x));}append_unique(opt.native_symbols,discovered);
        if(opt.debug){std::cerr<<"[jauc:resolve] target="<<target<<" package_objects="<<pkg.size()<<" native_symbols="<<discovered.size()<<"\\n";for(auto&p:pkg)std::cerr<<"[jauc:object] "<<p<<"\\n";for(auto&z:discovered)std::cerr<<"[jauc:symbol] "<<z<<"\\n";}
''',
'''        auto info=collect_native_package_info(input,target,tmp,opt);auto pkg=info.objects;auto discovered=info.symbols;for(auto&x:opt.native_inputs){auto e=lower_ext(x);if(e==".obj"||e==".o")append_unique(discovered,coff_defined_symbols(x));}append_unique(opt.native_symbols,discovered);append_unique(opt.system_libs,info.system_libs);append_unique(opt.native_imports,info.imports);
        if(opt.debug){std::cerr<<"[jauc:resolve] target="<<target<<" package_objects="<<pkg.size()<<" native_symbols="<<discovered.size()<<" system_libs="<<opt.system_libs.size()<<" imports="<<opt.native_imports.size()<<"\\n";for(auto&p:pkg)std::cerr<<"[jauc:object] "<<p<<"\\n";for(auto&z:discovered)std::cerr<<"[jauc:symbol] "<<z<<"\\n";for(auto&z:opt.system_libs)std::cerr<<"[jauc:system-lib] "<<z<<"\\n";for(auto&z:opt.native_imports)std::cerr<<"[jauc:import] "<<z<<"\\n";}
''', "windows package linker metadata")
s = rep(s,
'''args.push_back("--entry");args.push_back("main");int rc=run_process(linker,args);''',
'''args.push_back("--entry");args.push_back("main");args.push_back("--subsystem");args.push_back(opt.subsystem);for(auto&x:opt.system_libs){args.push_back("--system-lib");args.push_back(x);}for(auto&x:opt.native_imports){args.push_back("--import");args.push_back(x);}int rc=run_process(linker,args);''', "pass linker options")
# Add generic Linux package linker wrapper before diagnostics.
insert_before = '''static void print_diagnostic(const std::string&input,const std::string&stage,const std::string&message){
'''
helper = '''static jau::Result generic_native_build_with_packages(const std::string&input,const std::string&output,const std::string&target,jau::CompileOptions opt){
    fs::path tmp=fs::temp_directory_path()/("jau_pkg_native_"+std::to_string(std::hash<std::string>{}(input+output+target)));std::error_code ec;fs::remove_all(tmp,ec);fs::create_directories(tmp);
    try{auto info=collect_native_package_info(input,target,tmp,opt);append_unique(opt.native_inputs,info.objects);append_unique(opt.system_libs,info.system_libs);append_unique(opt.native_imports,info.imports);if(!opt.native_imports.empty()){fs::remove_all(tmp,ec);return {false,"imports_* manifest/--import is a Windows PE feature; Linux uses normal -l/system libraries"};}auto r=jau::native_build(input,output,target,opt);fs::remove_all(tmp,ec);return r;}catch(const std::exception&e){fs::remove_all(tmp,ec);return {false,e.what()};}
}

'''+insert_before
s = rep(s, insert_before, helper, "generic package native helper")
# Help and options.
s = rep(s,
'''              << "  jauc native <file.jau> -o program --target <target> [--link file.c|file.cpp|file.obj] [-O0..-O3]\\n"
''',
'''              << "  jauc native <file.jau> -o program --target <target> [--link native.obj] [--system-lib name] [--import symbol=dll] [--subsystem console|windows] [-O0..-O3]\\n"
              << "  jauc targets\\n"
''', "jauc help native")
s = rep(s,
'''              << "Native .jaux packages can provide native_windows_x86_64/native_windows_x86 objects.\\n"
''',
'''              << "Native .jaux packages can provide target objects plus system_libs_<platform>/imports_<platform> linker metadata.\\n"
              << "Windows --system-lib resolves real DLL exports (or common fallback maps) inside jauld; --import symbol=dll is explicit.\\n"
''', "jauc help system libs")
s = rep(s,
'''    if (cmd == "--version" || cmd == "version") { std::cout << jau::version() << "\\n"; return 0; }
    if (argc < 3) { help(); return 2; }
''',
'''    if (cmd == "--version" || cmd == "version") { std::cout << jau::version() << "\\n"; return 0; }
    if (cmd == "targets") { std::cout << "linux-x86_64\\nlinux-x86\\nwindows-x86_64\\nwindows-x86\\n"; return 0; }
    if (argc < 3) { help(); return 2; }
''', "jauc targets")
s = rep(s,
'''else if (a == "--debug") opt.debug = true; else if (a == "--link" && i + 1 < argc) opt.native_inputs.push_back(argv[++i]); else if (a == "-I" && i + 1 < argc) opt.import_paths.push_back(argv[++i]);''',
'''else if (a == "--debug") opt.debug = true; else if (a == "--link" && i + 1 < argc) opt.native_inputs.push_back(argv[++i]); else if (a == "--system-lib" && i + 1 < argc) opt.system_libs.push_back(argv[++i]); else if (a == "--import" && i + 1 < argc) opt.native_imports.push_back(argv[++i]); else if (a == "--subsystem" && i + 1 < argc) opt.subsystem=lower_copy_local(argv[++i]); else if (a == "-I" && i + 1 < argc) opt.import_paths.push_back(argv[++i]);''', "jauc parse linker options")
s = rep(s,
'''    if(cmd=="native"&&!optimize_set)opt.optimize=0; // v0.8.2 correctness-first native default
''',
'''    if(cmd=="native"&&!optimize_set)opt.optimize=0; // correctness-first native default
    if(opt.subsystem!="console"&&opt.subsystem!="windows"){std::cerr<<"jauc: --subsystem must be console or windows\\n";return 2;}
''', "jauc subsystem validation")
s = rep(s,
'''    else if (cmd == "native") { if (target.empty()) target = "linux-x86_64"; if (output.empty()) output = is_windows_target(target)?"jau-app.exe":"a.out"; r = is_windows_target(target)?windows_native_build(argv[0],input,output,target,opt):jau::native_build(input, output, target, opt); }
''',
'''    else if (cmd == "native") { if (target.empty()) target = "linux-x86_64"; if (output.empty()) output = is_windows_target(target)?"jau-app.exe":"a.out"; r = is_windows_target(target)?windows_native_build(argv[0],input,output,target,opt):generic_native_build_with_packages(input, output, target, opt); }
''', "generic native package pipeline")
write(p, s)

# -----------------------------------------------------------------------------
# JauM: small, deterministic project builder. It deliberately shells only to
# Jau's own drivers and keeps project intent in a human-readable jaum.txt.
# -----------------------------------------------------------------------------
jaum = r'''#include <algorithm>
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
namespace fs=std::filesystem;
static std::string trim(std::string s){size_t a=0,b=s.size();while(a<b&&std::isspace((unsigned char)s[a]))++a;while(b>a&&std::isspace((unsigned char)s[b-1]))--b;return s.substr(a,b-a);}
static std::vector<std::string> split(std::string s){std::vector<std::string>v;std::string x;std::istringstream in(s);while(std::getline(in,x,',')){x=trim(x);if(!x.empty())v.push_back(x);}return v;}
static std::string quote(const std::string&s){
#ifdef _WIN32
 std::string r="\"";for(char c:s){if(c=='\"')r+="\\\"";else r+=c;}return r+"\"";
#else
 std::string r="'";for(char c:s){if(c=='\'')r+="'\\''";else r+=c;}return r+"'";
#endif
}
static fs::path self_path(const char*argv0){
#ifdef _WIN32
 std::vector<char>b(32768);DWORD n=GetModuleFileNameA(nullptr,b.data(),(DWORD)b.size());if(n&&n<b.size())return fs::path(std::string(b.data(),n));
#else
 std::vector<char>b(4096);ssize_t n=::readlink("/proc/self/exe",b.data(),b.size()-1);if(n>0)return fs::path(std::string(b.data(),(size_t)n));
#endif
 std::error_code ec;auto p=fs::absolute(argv0?argv0:"",ec);return ec?fs::path(argv0?argv0:""):p;
}
static std::map<std::string,std::string> config(const fs::path&p){std::ifstream f(p);if(!f)throw std::runtime_error("cannot open "+p.string());std::map<std::string,std::string>m;std::string line;while(std::getline(f,line)){line=trim(line);if(line.empty()||line[0]=='#'||line[0]==';'||line[0]=='[')continue;auto q=line.find('=');if(q==std::string::npos)throw std::runtime_error("invalid jaum.txt line: "+line);auto k=trim(line.substr(0,q)),v=trim(line.substr(q+1));if(v.size()>=2&&((v.front()=='\"'&&v.back()=='\"')||(v.front()=='\''&&v.back()=='\'')))v=v.substr(1,v.size()-2);m[k]=v;}return m;}
static std::string replace_all(std::string s,const std::string&a,const std::string&b){size_t p=0;while(!a.empty()&&(p=s.find(a,p))!=std::string::npos){s.replace(p,a.size(),b);p+=b.size();}return s;}
static int run(const fs::path&exe,const std::vector<std::string>&args,bool verbose){std::string cmd=quote(exe.string());for(auto&a:args)cmd+=" "+quote(a);if(verbose)std::cerr<<"[jaum] "<<cmd<<"\n";return std::system(cmd.c_str());}
static std::string ext_for(const std::string&type,const std::string&target){bool win=target.rfind("windows-",0)==0;if(type=="exe")return win?".exe":"";if(type=="obj")return win?".obj":".o";if(type=="asm")return ".s";return "";}
static int build_one(const fs::path&cfg,const std::string&override_target){auto m=config(cfg);auto get=[&](const std::string&k,const std::string&d=""){auto i=m.find(k);return i==m.end()?d:i->second;};std::string name=get("name","app"),source=get("source","src/main.jau"),type=get("type","exe"),target=override_target.empty()?get("target","linux-x86_64"):override_target,opt=get("optimize","0");if(type=="native")type="exe";if(type!="exe"&&type!="obj"&&type!="asm")throw std::runtime_error("type must be exe, obj or asm in JauM 0.9");std::string out=get("output");if(out.empty())out=(fs::path(get("build_dir","build"))/(name+"-"+target+ext_for(type,target))).string();out=replace_all(out,"{name}",name);out=replace_all(out,"{target}",target);out=replace_all(out,"{ext}",ext_for(type,target));if(fs::path(out).has_parent_path())fs::create_directories(fs::path(out).parent_path());auto self=self_path(nullptr);auto jauc=self.parent_path()/
#ifdef _WIN32
"jauc.exe";
#else
"jauc";
#endif
if(!fs::exists(jauc))throw std::runtime_error("jauc not found next to jaum: "+jauc.string());std::vector<std::string>a; a.push_back(type=="exe"?"native":type);a.push_back(source);a.push_back("-o");a.push_back(out);a.push_back("--target");a.push_back(target);a.push_back("-O"+opt);for(auto&x:split(get("links"))){a.push_back("--link");a.push_back(x);}for(auto&x:split(get("system_libs"))){a.push_back("--system-lib");a.push_back(x);}for(auto&x:split(get("imports"))){a.push_back("--import");a.push_back(x);}for(auto&x:split(get("include_paths"))){a.push_back("-I");a.push_back(x);}if(type=="exe"){a.push_back("--subsystem");a.push_back(get("subsystem","console"));}if(get("debug","false")=="true")a.push_back("--debug");int rc=run(jauc,a,true);if(rc!=0)std::cerr<<"[jaum] build failed for "<<target<<"\n";else std::cout<<"JauM built "<<out<<"\n";return rc;}
static void usage(){std::cout<<"JauM 0.9\nusage: jaum init [name] | jaum build [-f jaum.txt] [--target target] | jaum build-all [-f jaum.txt] | jaum clean [-f jaum.txt] | jaum show [-f jaum.txt]\n";}
int main(int argc,char**argv){std::cerr<<"DeathAmir Jau @ DeathAmir 2026 (C)\n";try{if(argc<2){usage();return 0;}std::string cmd=argv[1];fs::path file="jaum.txt";std::string target;for(int i=2;i<argc;++i){std::string a=argv[i];if(a=="-f"&&i+1<argc)file=argv[++i];else if(a=="--target"&&i+1<argc)target=argv[++i];}
if(cmd=="init"){std::string name=argc>2?argv[2]:fs::current_path().filename().string();if(fs::exists(file))throw std::runtime_error(file.string()+" already exists");std::ofstream o(file);o<<"# JauM project file\nname="<<name<<"\nsource=src/main.jau\ntype=exe\ntarget=windows-x86_64\n# targets=windows-x86_64,windows-x86,linux-x86_64,linux-x86\noutput=build/{name}-{target}{ext}\noptimize=0\nsubsystem=console\nlinks=\nsystem_libs=\nimports=\ninclude_paths=stdlib\n";fs::create_directories("src");if(!fs::exists("src/main.jau")){std::ofstream j("src/main.jau");j<<"func main() {\n    print(\"Hello from JauM\");\n    return 0;\n}\n";}std::cout<<"created "<<file.string()<<"\n";return 0;}
if(cmd=="build")return build_one(file,target);
if(cmd=="build-all"){auto m=config(file);auto it=m.find("targets");if(it==m.end()||split(it->second).empty())throw std::runtime_error("build-all requires targets=... in jaum.txt");int bad=0;for(auto&t:split(it->second))if(build_one(file,t)!=0)bad=1;return bad;}
if(cmd=="clean"){auto m=config(file);auto it=m.find("build_dir");fs::path d=it==m.end()?"build":it->second;std::error_code ec;fs::remove_all(d,ec);if(ec)throw std::runtime_error("cannot clean "+d.string()+": "+ec.message());std::cout<<"cleaned "<<d.string()<<"\n";return 0;}
if(cmd=="show"){for(auto&kv:config(file))std::cout<<kv.first<<"="<<kv.second<<"\n";return 0;}usage();return 2;}catch(const std::exception&e){std::cerr<<"jaum: "<<e.what()<<"\n";return 1;}}
'''
write("src/jaum_main.cpp", jaum)

# CMake version + JauM target.
p = "CMakeLists.txt"
s = read(p)
s = rep(s, 'project(Jau VERSION 0.8.2 LANGUAGES CXX)', 'project(Jau VERSION 0.9.0 LANGUAGES CXX)', "cmake version")
s = rep(s, 'add_executable(jauld src/jauld_v2_main.cpp)\n', 'add_executable(jauld src/jauld_v2_main.cpp)\nadd_executable(jaum src/jaum_main.cpp)\n', "jaum target")
s = rep(s, 'install(TARGETS jauc jur jauas jauld jau-setup RUNTIME DESTINATION bin)', 'install(TARGETS jauc jur jauas jauld jaum jau-setup RUNTIME DESTINATION bin)', "install jaum")
write(p, s)

# Focused regression source/config used by CI and future maintainers.
write("tests/v090_core.jau", '''func main() {\n    let x = 20 + 22;\n    print(x);\n    return 0;\n}\n''')
write("tests/v090_jaum.txt", '''name=v090\nsource=tests/v090_core.jau\ntype=exe\ntarget=windows-x86_64\noutput=build-jaum/{name}-{target}{ext}\noptimize=0\nsubsystem=console\n''')
print("Jau 0.9.0 core patch applied")
