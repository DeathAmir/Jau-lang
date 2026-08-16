from pathlib import Path


def read(p): return Path(p).read_text(encoding='utf-8')
def write(p,s): Path(p).write_text(s,encoding='utf-8')
def rep(s,a,b,label):
    if a not in s: raise SystemExit('anchor not found: '+label)
    return s.replace(a,b,1)

p='src/jauc_main.cpp'; s=read(p)
s=rep(s,'#include <iostream>\n','#include <iostream>\n#include <algorithm>\n#include <cstdint>\n#include <map>\n#include <set>\n','static archive includes')

anchor='''static void write_binary(const fs::path&p,const std::string&data){if(p.has_parent_path())fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("cannot create temporary native object");f.write(data.data(),(std::streamsize)data.size());}\n'''
helper=r'''static uint64_t elf64le(const std::string&b,size_t p){if(p+8>b.size())throw std::runtime_error("truncated ELF");uint64_t v=0;for(int i=7;i>=0;--i)v=(v<<8)|(unsigned char)b[p+i];return v;}
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
static std::string ar_header(const std::string&name,size_t size){return ar_field(name,16)+ar_field("0",12)+ar_field("0",6)+ar_field("0",6)+ar_field("100644",8)+ar_field(std::to_string(size),10)+"`\\n";}
static size_t ar_span(size_t n){return 60+n+(n&1u);}
static void ar_member(std::string&out,const std::string&name,const std::string&data){out+=ar_header(name,data.size());out+=data;if(data.size()&1u)out.push_back('\\n');}
static void write_static_archive(const fs::path&object,const fs::path&output,const std::string&target){
    std::string obj=read_text(object);if(obj.empty())throw std::runtime_error("cannot read object for static archive: "+object.string());bool win=is_windows_target(target);auto symbols=win?coff_defined_symbols(object):elf_defined_symbols(object);std::sort(symbols.begin(),symbols.end());symbols.erase(std::unique(symbols.begin(),symbols.end()),symbols.end());if(symbols.empty())throw std::runtime_error("object has no externally defined symbols for static library");
    size_t names=0;for(auto&x:symbols)names+=x.size()+1;size_t first_size=4+symbols.size()*4+names;size_t second_size=win?(4+4+4+symbols.size()*2+names):0;uint32_t obj_off=(uint32_t)(8+ar_span(first_size)+(win?ar_span(second_size):0));
    std::string first;ar_be32(first,(uint32_t)symbols.size());for(size_t i=0;i<symbols.size();++i)ar_be32(first,obj_off);for(auto&x:symbols){first+=x;first.push_back(0);}
    std::string second;if(win){ar_le32(second,1);ar_le32(second,obj_off);ar_le32(second,(uint32_t)symbols.size());for(size_t i=0;i<symbols.size();++i)ar_le16(second,1);for(auto&x:symbols){second+=x;second.push_back(0);}}
    std::string archive="!<arch>\\n";ar_member(archive,"/",first);if(win)ar_member(archive,"/",second);ar_member(archive,win?"jau.obj/":"jau.o/",obj);write_binary(output,archive);
}
static void write_binary(const fs::path&p,const std::string&data){if(p.has_parent_path())fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("cannot create temporary native object");f.write(data.data(),(std::streamsize)data.size());}
'''
s=rep(s,anchor,helper,'archive helper insertion')

s=rep(s,
'''              << "  jauc shared <file.jau> -o library --target <target> --export name [--export public=internal] [--system-lib name]\\n"\n              << "  jauc targets\\n"''',
'''              << "  jauc shared <file.jau> -o library --target <target> --export name [--export public=internal] [--system-lib name]\\n"\n              << "  jauc static <file.jau> -o library.lib|library.a --target <target>\\n"\n              << "  jauc targets\\n"''','static help')
s=rep(s,'if((cmd=="native"||cmd=="shared")&&!optimize_set)opt.optimize=0;','if((cmd=="native"||cmd=="shared"||cmd=="static")&&!optimize_set)opt.optimize=0;','static safe optimization')
needle='''    else if (cmd == "shared") { if(target.empty())target="linux-x86_64";if(opt.exports.empty()){std::cerr<<"jauc: shared requires at least one --export name\\n";return 2;}opt.shared=true;opt.library_mode=true;if(output.empty())output=is_windows_target(target)?"jau-lib.dll":"libjau.so";else if(!is_windows_target(target)&&lower_ext(output)!=".so")output+=".so";r=is_windows_target(target)?windows_native_build(argv[0],input,output,target,opt,true):generic_native_build_with_packages(input,output,target,opt); }\n'''
static_branch=r'''    else if (cmd == "static") { if(target.empty())target="linux-x86_64";if(output.empty())output=is_windows_target(target)?"jau-lib.lib":"libjau.a";else if(is_windows_target(target)&&lower_ext(output)!=".lib")output+=".lib";else if(!is_windows_target(target)&&lower_ext(output)!=".a")output+=".a";opt.library_mode=true;fs::path td=fs::temp_directory_path()/("jau_static_"+std::to_string(std::hash<std::string>{}(input+output+target)));std::error_code ec;fs::remove_all(td,ec);fs::create_directories(td);fs::path asmp=td/"library.s",obj=td/(is_windows_target(target)?"library.obj":"library.o");r=emit_with_native_symbols(input,asmp.string(),target,opt);if(r.ok){fs::path self=current_executable_path(argv[0]);fs::path assembler=self.parent_path()/
#ifdef _WIN32
            "jauas.exe";
#else
            "jauas";
#endif
            int rc=run_process(assembler,{asmp.string(),"-o",obj.string(),"--target",target,"--object"});if(rc!=0)r={false,"internal jauas static object build failed"};else try{write_static_archive(obj,output,target);r={true,"static library built: "+output+" ("+target+")"};}catch(const std::exception&e){r={false,e.what()};}}fs::remove_all(td,ec); }
'''
s=rep(s,needle,needle+static_branch,'static command')
write(p,s)

p='src/jaum_main.cpp'; s=read(p)
s=rep(s,
'''if(type=="shared")return win?".dll":".so";if(type=="obj")''',
'''if(type=="shared")return win?".dll":".so";if(type=="static")return win?".lib":".a";if(type=="obj")''','JauM static extension')
s=rep(s,
'''if(type!="exe"&&type!="shared"&&type!="obj"&&type!="asm")throw std::runtime_error("type must be exe, shared, obj or asm in JauM 0.9");''',
'''if(type!="exe"&&type!="shared"&&type!="static"&&type!="obj"&&type!="asm")throw std::runtime_error("type must be exe, shared, static, obj or asm in JauM 0.9");''','JauM static type')
write(p,s)

Path('tests/v090_jaum_static.txt').write_text('''name=jaucalc\nsource=tests/v090_shared.jau\ntype=static\ntarget=windows-x86_64\noutput=build-jaum/{name}-{target}{ext}\noptimize=0\n''',encoding='utf-8')
print('Jau 0.9 static-library patch applied')
