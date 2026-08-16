};

static std::string name_from8(const uint8_t* p){ size_t n=0; while(n<8&&p[n])++n; return std::string((const char*)p,n); }
static std::string string_at(const std::vector<uint8_t>& b,size_t strbase,uint32_t off){
    if(off<4||strbase+off>=b.size())return ""; size_t p=strbase+off,e=p; while(e<b.size()&&b[e])++e; return std::string((const char*)&b[p],e-p);
}
static Object read_coff(const fs::path& path){
    auto b=read_file(path); if(b.size()<20)throw std::runtime_error("not a COFF object: "+path.string());
    Object o;o.path=path;o.machine=r16(b,0);uint16_t nsec=r16(b,2);uint32_t symptr=r32(b,8),nsym=r32(b,12);uint16_t opt=r16(b,16);
    if(o.machine!=0x8664&&o.machine!=0x014c)throw std::runtime_error("unsupported COFF machine in "+path.string());
    size_t sh=20u+opt;if(sh+(size_t)nsec*40u>b.size())throw std::runtime_error("truncated COFF section table");
    size_t strbase=(size_t)symptr+(size_t)nsym*18u;
    if(strbase+4>b.size())strbase=b.size();
    auto secname=[&](size_t p){std::string s=name_from8(&b[p]);if(!s.empty()&&s[0]=='/'&&s.size()>1){try{return string_at(b,strbase,(uint32_t)std::stoul(s.substr(1)));}catch(...){}}return s;};
    o.sections.resize(nsec);
    for(uint16_t i=0;i<nsec;++i){size_t p=sh+(size_t)i*40u;auto&s=o.sections[i];s.name=secname(p);s.virtual_size=r32(b,p+8);uint32_t rawsz=r32(b,p+16),raw=r32(b,p+20),rp=r32(b,p+24);uint16_t nr=r16(b,p+32);s.chars=r32(b,p+36);if(rawsz){if((size_t)raw+rawsz>b.size())throw std::runtime_error("truncated COFF section data");s.data.assign(b.begin()+raw,b.begin()+raw+rawsz);}if(!s.virtual_size)s.virtual_size=rawsz;if(nr){if((size_t)rp+(size_t)nr*10u>b.size())throw std::runtime_error("truncated COFF relocations");for(uint16_t q=0;q<nr;++q){size_t z=(size_t)rp+(size_t)q*10u;s.relocs.push_back({r32(b,z),r32(b,z+4),r16(b,z+8)});}}}
    o.symbols.resize(nsym);
    if(nsym&&((size_t)symptr+(size_t)nsym*18u>b.size()))throw std::runtime_error("truncated COFF symbols");
    for(uint32_t i=0;i<nsym;){size_t p=(size_t)symptr+(size_t)i*18u;Symbol s;s.valid=true;uint32_t z=r32(b,p);if(z==0)s.name=string_at(b,strbase,r32(b,p+4));else s.name=name_from8(&b[p]);s.value=r32(b,p+8);s.section=(int16_t)r16(b,p+12);s.type=r16(b,p+14);s.storage=b[p+16];s.aux=b[p+17];o.symbols[i]=s;for(uint8_t a=0;a<s.aux&&i+1+a<nsym;++a){o.symbols[i+1+a].aux_entry=true;}i+=1u+s.aux;}
    return o;
}

static bool skip_section(const InSection&s){
    auto n=lower(s.name);
    if(s.chars&0x00000800u)return true; // IMAGE_SCN_LNK_REMOVE
    if(n==".drectve"||n==".llvm_addrsig"||n.rfind(".debug",0)==0||n==".comment")return true;
    return false;
}
struct OutSection { std::string name;std::vector<uint8_t> data;uint32_t virtual_size=0,rva=0,raw=0,raw_size=0,chars=0; };
static int group_for(const InSection&s){if(s.chars&0x00000020u)return 0;if(s.chars&0x80000000u)return 2;return 1;}
static size_t group_align(int g){return g==0?16u:8u;}

struct ImportDesc {std::string exact,canonical,dll;bool indirect=false;uint32_t thunk_off=0,iat_off=0,hint_off=0;};
static std::string undecorate(std::string n){
    if(n.rfind("__imp_",0)==0)n=n.substr(6);else if(n.rfind("_imp__",0)==0)n=n.substr(6);
    while(n.size()>1&&n[0]=='_')n.erase(n.begin());
    auto at=n.find('@');if(at!=std::string::npos){bool digits=at+1<n.size();for(size_t i=at+1;i<n.size();++i)digits=digits&&std::isdigit((unsigned char)n[i]);if(digits)n=n.substr(0,at);}
    return n;
}
static bool is_indirect_import(const std::string&n){return n.rfind("__imp_",0)==0||n.rfind("_imp__",0)==0;}

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
    static const std::unordered_set<std::string> crt={"printf","puts","putchar","sprintf","snprintf","malloc","calloc","realloc","free","memcpy","memmove","memset","strlen","strcmp","strncmp","atoi","strtol","exit","abort"};
    static const std::unordered_set<std::string> k32={"ExitProcess","GetStdHandle","WriteFile","ReadFile","Sleep","GetLastError","SetLastError","GetTickCount","GetTickCount64","QueryPerformanceCounter","QueryPerformanceFrequency"};
    auto ex=g_import_overrides.find(canon);if(ex!=g_import_overrides.end())return ex->second;
    if(crt.count(canon))return "msvcrt.dll";if(k32.count(canon))return "kernel32.dll";
    auto si=g_system_imports.find(canon);if(si!=g_system_imports.end()){if(si->second==AMBIGUOUS_DLL)throw std::runtime_error("system symbol '"+canon+"' is exported by multiple requested DLLs; disambiguate with --import "+canon+"=name.dll");return si->second;}
    return "";
}

struct DefRef {size_t obj=0;uint32_t sym=0;};
struct LinkState {
    bool x64=false;uint16_t machine=0;uint64_t image_base=0;
    std::vector<Object> objs;
    std::unordered_map<std::string,DefRef> globals;
    std::vector<OutSection> outs;
    std::vector<ImportDesc> imports;
    std::unordered_map<std::string,size_t> import_by_canon;
    std::unordered_map<std::string,uint32_t> import_target;