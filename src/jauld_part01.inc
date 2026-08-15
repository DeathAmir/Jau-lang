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
    if(!n.empty()&&n[0]=='_')n.erase(n.begin());
    auto at=n.find('@');if(at!=std::string::npos){bool digits=at+1<n.size();for(size_t i=at+1;i<n.size();++i)digits=digits&&std::isdigit((unsigned char)n[i]);if(digits)n=n.substr(0,at);}
    return n;
}
static bool is_indirect_import(const std::string&n){return n.rfind("__imp_",0)==0||n.rfind("_imp__",0)==0;}
static std::string import_dll(const std::string&canon){
    static const std::unordered_set<std::string> crt={"printf","puts","putchar","sprintf","snprintf","malloc","calloc","realloc","free","memcpy","memmove","memset","strlen","strcmp","strncmp","atoi","strtol","exit","abort"};
    static const std::unordered_set<std::string> k32={"ExitProcess","GetStdHandle","WriteFile","ReadFile","Sleep","GetLastError","SetLastError","GetTickCount","GetTickCount64","QueryPerformanceCounter","QueryPerformanceFrequency"};
    if(crt.count(canon))return "msvcrt.dll";if(k32.count(canon))return "kernel32.dll";return "";
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