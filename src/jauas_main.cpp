#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

static std::string trim(std::string s){
    size_t a=0,b=s.size();
    while(a<b&&std::isspace((unsigned char)s[a]))++a;
    while(b>a&&std::isspace((unsigned char)s[b-1]))--b;
    return s.substr(a,b-a);
}
static bool starts(const std::string&s,const std::string&p){return s.rfind(p,0)==0;}
static void u8(std::vector<uint8_t>&o,uint8_t v){o.push_back(v);}
static void u16(std::vector<uint8_t>&o,uint16_t v){o.push_back((uint8_t)v);o.push_back((uint8_t)(v>>8));}
static void u32(std::vector<uint8_t>&o,uint32_t v){for(int i=0;i<4;++i)o.push_back((uint8_t)(v>>(8*i)));}
static void u64(std::vector<uint8_t>&o,uint64_t v){for(int i=0;i<8;++i)o.push_back((uint8_t)(v>>(8*i)));}
static void i64(std::vector<uint8_t>&o,int64_t v){u64(o,(uint64_t)v);}
static void patch32(std::vector<uint8_t>&o,size_t p,uint32_t v){if(p+4>o.size())throw std::runtime_error("internal patch overflow");for(int i=0;i<4;++i)o[p+i]=(uint8_t)(v>>(8*i));}
static void align_to(std::vector<uint8_t>&o,size_t n){while(o.size()%n)o.push_back(0);}
static void put_name8(std::vector<uint8_t>&o,const std::string&name){for(size_t i=0;i<8;++i)o.push_back(i<name.size()?(uint8_t)name[i]:0);}

static std::string unquote(const std::string&s){
    auto a=s.find('"'),b=s.rfind('"');
    if(a==std::string::npos||b<=a)throw std::runtime_error("bad string directive");
    std::string r;
    for(size_t i=a+1;i<b;++i){
        char c=s[i];
        if(c=='\\'&&i+1<b){
            char e=s[++i];
            if(e=='n')r+='\n';else if(e=='r')r+='\r';else if(e=='t')r+='\t';else if(e=='0')r+='\0';else if(e=='\\')r+='\\';else if(e=='"')r+='"';else r+=e;
        }else r+=c;
    }
    return r;
}

// -----------------------------------------------------------------------------
// Legacy freestanding ELF writer used by the Stage-1 bootstrap compiler.
// -----------------------------------------------------------------------------

struct BootFix{size_t pos;std::string label;bool rip;};

static int build_legacy_elf(const std::string&in,const std::string&out,const std::string&target){
    bool x64=target=="linux-x86_64";
    if(!x64&&target!="linux-x86")throw std::runtime_error("direct executable mode is available for linux-x86_64/linux-x86; use --object for Windows COFF");
    std::ifstream f(in);if(!f)throw std::runtime_error("cannot open input");
    std::vector<uint8_t> code,data;std::unordered_map<std::string,size_t> clabel,dlabel;std::vector<BootFix> fix;bool text=true;std::string line;
    auto imm=[](const std::string&s)->uint64_t{return (uint64_t)std::stoll(trim(s),nullptr,0);};
    while(std::getline(f,line)){
        auto hash=line.find('#');if(hash!=std::string::npos)line=line.substr(0,hash);line=trim(line);if(line.empty())continue;
        if(line==".text"){text=true;continue;}
        if(starts(line,".section")){text=line.find(".rodata")==std::string::npos;continue;}
        if(line[0]=='.')continue;
        auto colon=line.find(':');
        if(colon!=std::string::npos){
            std::string label=trim(line.substr(0,colon));(text?clabel:dlabel)[label]=text?code.size():data.size();line=trim(line.substr(colon+1));if(line.empty())continue;
        }
        if(!text){
            if(starts(line,".ascii")){auto q=unquote(line);data.insert(data.end(),q.begin(),q.end());continue;}
            if(starts(line,".asciz")){auto q=unquote(line);data.insert(data.end(),q.begin(),q.end());data.push_back(0);continue;}
            throw std::runtime_error("unsupported data directive: "+line);
        }
        if(line=="syscall"){code.insert(code.end(),{0x0f,0x05});continue;}
        if(line=="int 0x80"){code.insert(code.end(),{0xcd,0x80});continue;}
        if(x64&&starts(line,"lea rsi, [rip+")&&line.back()==']'){
            std::string l=line.substr(14,line.size()-15);code.insert(code.end(),{0x48,0x8d,0x35});size_t pos=code.size();u32(code,0);fix.push_back({pos,l,true});continue;
        }
        if(!x64&&starts(line,"mov ecx,")){
            std::string rhs=trim(line.substr(8));
            if(!rhs.empty()&&!std::isdigit((unsigned char)rhs[0])&&rhs[0]!='-'){code.push_back(0xb9);size_t pos=code.size();u32(code,0);fix.push_back({pos,rhs,false});continue;}
        }
        if(starts(line,"mov ")){
            auto c=line.find(',');if(c==std::string::npos)throw std::runtime_error("bad mov");
            std::string r=trim(line.substr(4,c-4)),v=trim(line.substr(c+1));uint64_t n=imm(v);
            if(x64){
                uint8_t op=0;if(r=="rax")op=0xb8;else if(r=="rdx")op=0xba;else if(r=="rsi")op=0xbe;else if(r=="rdi")op=0xbf;else throw std::runtime_error("unsupported bootstrap x64 register: "+r);
                code.push_back(0x48);code.push_back(op);u64(code,n);
            }else{
                uint8_t op=0;if(r=="eax")op=0xb8;else if(r=="ecx")op=0xb9;else if(r=="edx")op=0xba;else if(r=="ebx")op=0xbb;else throw std::runtime_error("unsupported bootstrap x86 register: "+r);
                code.push_back(op);u32(code,(uint32_t)n);
            }
            continue;
        }
        throw std::runtime_error("unsupported bootstrap instruction: "+line);
    }
    size_t hdr=x64?120:84;uint64_t base=x64?0x400000ull:0x08048000ull;
    if(!clabel.count("_start"))throw std::runtime_error("missing _start label");
    for(auto&x:fix){
        auto it=dlabel.find(x.label);if(it==dlabel.end())throw std::runtime_error("unknown data label: "+x.label);
        uint64_t target_addr=base+hdr+code.size()+it->second;
        if(x.rip){uint64_t next=base+hdr+x.pos+4;int64_t d=(int64_t)target_addr-(int64_t)next;patch32(code,x.pos,(uint32_t)(int32_t)d);}
        else patch32(code,x.pos,(uint32_t)target_addr);
    }
    uint64_t filesz=hdr+code.size()+data.size(),entry=base+hdr+clabel["_start"];std::vector<uint8_t> bin;
    bin.insert(bin.end(),{0x7f,'E','L','F',(uint8_t)(x64?2:1),1,1,0});while(bin.size()<16)bin.push_back(0);
    if(x64){
        u16(bin,2);u16(bin,62);u32(bin,1);u64(bin,entry);u64(bin,64);u64(bin,0);u32(bin,0);u16(bin,64);u16(bin,56);u16(bin,1);u16(bin,0);u16(bin,0);u16(bin,0);
        u32(bin,1);u32(bin,5);u64(bin,0);u64(bin,base);u64(bin,base);u64(bin,filesz);u64(bin,filesz);u64(bin,0x1000);
    }else{
        u16(bin,2);u16(bin,3);u32(bin,1);u32(bin,(uint32_t)entry);u32(bin,52);u32(bin,0);u32(bin,0);u16(bin,52);u16(bin,32);u16(bin,1);u16(bin,0);u16(bin,0);u16(bin,0);
        u32(bin,1);u32(bin,0);u32(bin,(uint32_t)base);u32(bin,(uint32_t)base);u32(bin,(uint32_t)filesz);u32(bin,(uint32_t)filesz);u32(bin,5);u32(bin,0x1000);
    }
    if(bin.size()!=hdr)throw std::runtime_error("internal ELF header size error");
    bin.insert(bin.end(),code.begin(),code.end());bin.insert(bin.end(),data.begin(),data.end());
    std::ofstream of(out,std::ios::binary);of.write((char*)bin.data(),(std::streamsize)bin.size());of.close();
#ifndef _WIN32
    fs::permissions(out,fs::perms::owner_exec|fs::perms::group_exec|fs::perms::others_exec,fs::perm_options::add);
#endif
    std::cout<<"jauas: wrote "<<out<<" ("<<target<<", ELF executable, "<<bin.size()<<" bytes)\n";
    return 0;
}

// -----------------------------------------------------------------------------
// Relocatable object assembler for the Jau AOT instruction subset.
// -----------------------------------------------------------------------------

enum class RelocKind{Rel32,Abs32};
struct Reloc{size_t offset=0;std::string symbol;RelocKind kind=RelocKind::Rel32;bool call=false;};
struct LocalFix{size_t offset=0;std::string label;size_t next=0;};
struct AsmImage{
    bool x64=false,windows=false;
    std::vector<uint8_t> text,rodata;
    std::unordered_map<std::string,size_t> text_labels,data_labels;
    std::set<std::string> globals,externs;
    std::vector<Reloc> relocs;
    std::vector<LocalFix> local_fixes;
};

static int reg64(const std::string&r){
    static const std::map<std::string,int> m={{"rax",0},{"rcx",1},{"rdx",2},{"rbx",3},{"rsp",4},{"rbp",5},{"rsi",6},{"rdi",7},{"r8",8},{"r9",9}};
    auto it=m.find(r);return it==m.end()?-1:it->second;
}
static int reg32(const std::string&r){
    static const std::map<std::string,int> m={{"eax",0},{"ecx",1},{"edx",2},{"ebx",3},{"esp",4},{"ebp",5},{"esi",6},{"edi",7}};
    auto it=m.find(r);return it==m.end()?-1:it->second;
}
static bool isnum(const std::string&s){
    if(s.empty())return false;size_t i=(s[0]=='-'||s[0]=='+')?1:0;
    if(i>=s.size())return false;
    if(s.size()>i+2&&s[i]=='0'&&(s[i+1]=='x'||s[i+1]=='X'))return true;
    for(;i<s.size();++i)if(!std::isdigit((unsigned char)s[i]))return false;
    return true;
}
static int64_t num(const std::string&s){return std::stoll(trim(s),nullptr,0);}
static void rex(std::vector<uint8_t>&o,bool w,int r,int x,int b){
    uint8_t v=0x40|(w?8:0)|((r>>3)&1?4:0)|((x>>3)&1?2:0)|((b>>3)&1?1:0);
    if(v!=0x40)o.push_back(v);
}
static uint8_t modrm(int mod,int reg,int rm){return (uint8_t)(((mod&3)<<6)|((reg&7)<<3)|(rm&7));}

struct Mem{bool ok=false;int base=-1;int disp=0;bool qword=false;};
static Mem parse_mem(std::string op,bool x64){
    Mem m;m.qword=starts(op,"QWORD PTR");
    auto l=op.find('['),r=op.find(']');
    if(l==std::string::npos||r==std::string::npos||r<=l)return m;
    std::string in=trim(op.substr(l+1,r-l-1));
    std::string base=x64?"rbp":"ebp";
    if(!starts(in,base))return m;
    m.base=x64?5:5;m.disp=0;
    auto rest=trim(in.substr(base.size()));
    if(!rest.empty())m.disp=(int)num(rest);
    m.ok=true;return m;
}
static void emit_mem_disp(std::vector<uint8_t>&o,int regfield,const Mem&m,bool x64,bool store){
    if(x64)rex(o,true,regfield,0,m.base);
    o.push_back(store?0x89:0x8b);
    if(m.disp>=-128&&m.disp<=127){o.push_back(modrm(1,regfield,m.base));o.push_back((uint8_t)(int8_t)m.disp);}
    else{o.push_back(modrm(2,regfield,m.base));u32(o,(uint32_t)(int32_t)m.disp);}
}
static std::pair<std::string,std::string> operands(const std::string&line,size_t start){
    auto c=line.find(',',start);if(c==std::string::npos)throw std::runtime_error("expected comma: "+line);
    return {trim(line.substr(start,c-start)),trim(line.substr(c+1))};
}
static void emit_rel_placeholder(AsmImage&im,const std::string&sym,bool call){
    size_t p=im.text.size();u32(im.text,0);im.relocs.push_back({p,sym,RelocKind::Rel32,call});
}
static void emit_local_rel(AsmImage&im,const std::string&label,size_t instr_end){
    size_t p=im.text.size();u32(im.text,0);im.local_fixes.push_back({p,label,instr_end+4});
}

static AsmImage assemble_object_text(const std::string&input,const std::string&target){
    AsmImage im;im.windows=starts(target,"windows-");im.x64=target.find("x86_64")!=std::string::npos;
    if(target!="linux-x86_64"&&target!="linux-x86"&&target!="windows-x86_64"&&target!="windows-x86")throw std::runtime_error("unknown target: "+target);
    std::ifstream f(input);if(!f)throw std::runtime_error("cannot open input: "+input);
    enum Sec{TEXT,RODATA};Sec sec=TEXT;std::string line;
    while(std::getline(f,line)){
        auto hash=line.find('#');if(hash!=std::string::npos)line=line.substr(0,hash);line=trim(line);if(line.empty())continue;
        if(line==".text"){sec=TEXT;continue;}
        if(starts(line,".section")){if(line.find(".rodata")!=std::string::npos||line.find(".rdata")!=std::string::npos)sec=RODATA;else sec=TEXT;continue;}
        if(starts(line,".intel_syntax"))continue;
        if(starts(line,".globl ")||starts(line,".global ")){auto p=line.find(' ');im.globals.insert(trim(line.substr(p+1)));continue;}
        if(starts(line,".extern ")){im.externs.insert(trim(line.substr(8)));continue;}

        auto colon=line.find(':');
        if(colon!=std::string::npos && line.substr(0,colon).find_first_of(" \t")==std::string::npos){
            std::string label=trim(line.substr(0,colon));
            (sec==TEXT?im.text_labels:im.data_labels)[label]=(sec==TEXT?im.text.size():im.rodata.size());
            line=trim(line.substr(colon+1));if(line.empty())continue;
        }

        if(sec==RODATA){
            if(starts(line,".asciz")){auto q=unquote(line);im.rodata.insert(im.rodata.end(),q.begin(),q.end());im.rodata.push_back(0);continue;}
            if(starts(line,".ascii")){auto q=unquote(line);im.rodata.insert(im.rodata.end(),q.begin(),q.end());continue;}
            throw std::runtime_error("unsupported object data directive: "+line);
        }
        if(!line.empty()&&line[0]=='.')continue;

        auto& o=im.text;
        if(line=="leave"){o.push_back(0xc9);continue;}
        if(line=="ret"){o.push_back(0xc3);continue;}
        if(line=="cqo"){o.insert(o.end(),{0x48,0x99});continue;}
        if(line=="cdq"){o.push_back(0x99);continue;}
        if(line=="syscall"){o.insert(o.end(),{0x0f,0x05});continue;}
        if(line=="int 0x80"){o.insert(o.end(),{0xcd,0x80});continue;}

        if(starts(line,"push OFFSET FLAT:")){
            if(im.x64)throw std::runtime_error("OFFSET push is x86-only");
            std::string sym=trim(line.substr(std::strlen("push OFFSET FLAT:")));o.push_back(0x68);size_t p=o.size();u32(o,0);im.relocs.push_back({p,sym,RelocKind::Abs32,false});continue;
        }
        if(starts(line,"push ")){
            std::string r=trim(line.substr(5));int c=im.x64?reg64(r):reg32(r);if(c<0)throw std::runtime_error("unsupported push register: "+r);
            if(im.x64&&c>=8)o.push_back(0x41);o.push_back((uint8_t)(0x50+(c&7)));continue;
        }
        if(starts(line,"pop ")){
            std::string r=trim(line.substr(4));int c=im.x64?reg64(r):reg32(r);if(c<0)throw std::runtime_error("unsupported pop register: "+r);
            if(im.x64&&c>=8)o.push_back(0x41);o.push_back((uint8_t)(0x58+(c&7)));continue;
        }

        if(starts(line,"lea ")){
            auto [dst,src]=operands(line,4);int d=reg64(dst);
            if(!im.x64||d<0||!starts(src,"[rip+")||src.back()!=']')throw std::runtime_error("unsupported lea: "+line);
            std::string sym=src.substr(5,src.size()-6);rex(o,true,d,0,5);o.push_back(0x8d);o.push_back(modrm(0,d,5));emit_rel_placeholder(im,sym,false);continue;
        }

        if(starts(line,"movzx ")){
            auto [dst,src]=operands(line,6);if(dst!="eax"||(src!="al"&&src!="cl"))throw std::runtime_error("unsupported movzx: "+line);
            o.insert(o.end(),{0x0f,0xb6,(uint8_t)(src=="al"?0xc0:0xc1)});continue;
        }
        if(starts(line,"sete ")||starts(line,"setne ")||starts(line,"setl ")||starts(line,"setle ")||starts(line,"setg ")||starts(line,"setge ")){
            auto sp=line.find(' ');std::string cc=line.substr(3,sp-3),dst=trim(line.substr(sp+1));uint8_t op=0;
            if(cc=="e")op=0x94;else if(cc=="ne")op=0x95;else if(cc=="l")op=0x9c;else if(cc=="le")op=0x9e;else if(cc=="g")op=0x9f;else if(cc=="ge")op=0x9d;else throw std::runtime_error("bad setcc");
            int rm=dst=="al"?0:dst=="cl"?1:-1;if(rm<0)throw std::runtime_error("unsupported setcc register: "+dst);
            o.insert(o.end(),{0x0f,op,modrm(3,0,rm)});continue;
        }

        if(starts(line,"mov ")){
            auto [dst,src]=operands(line,4);
            Mem dm=parse_mem(dst,im.x64),sm=parse_mem(src,im.x64);
            int dr=im.x64?reg64(dst):reg32(dst),sr=im.x64?reg64(src):reg32(src);
            if(dm.ok&&sr>=0){emit_mem_disp(o,sr,dm,im.x64,true);continue;}
            if(dr>=0&&sm.ok){emit_mem_disp(o,dr,sm,im.x64,false);continue;}
            if(dr>=0&&sr>=0){
                if(im.x64)rex(o,true,sr,0,dr);o.push_back(0x89);o.push_back(modrm(3,sr,dr));continue;
            }
            if(dr>=0&&isnum(src)){
                int64_t v=num(src);
                if(im.x64){rex(o,true,0,0,dr);o.push_back((uint8_t)(0xb8+(dr&7)));u64(o,(uint64_t)v);}
                else{o.push_back((uint8_t)(0xb8+(dr&7)));u32(o,(uint32_t)v);}
                continue;
            }
            throw std::runtime_error("unsupported mov: "+line);
        }

        if(starts(line,"add ")||starts(line,"sub ")||starts(line,"or ")||starts(line,"cmp ")||starts(line,"test ")){
            auto sp=line.find(' ');std::string op=line.substr(0,sp);auto [dst,src]=operands(line,sp+1);
            int dr=im.x64?reg64(dst):reg32(dst),sr=im.x64?reg64(src):reg32(src);
            if(dr<0)throw std::runtime_error("unsupported "+op+" destination: "+dst);
            if(sr>=0){
                uint8_t opc=op=="add"?0x01:op=="sub"?0x29:op=="or"?0x09:op=="cmp"?0x39:0x85;
                if(im.x64)rex(o,true,sr,0,dr);o.push_back(opc);o.push_back(modrm(3,sr,dr));continue;
            }
            if(isnum(src)&&(op=="add"||op=="sub"||op=="cmp")){
                int64_t v=num(src);int ext=op=="add"?0:op=="sub"?5:7;if(im.x64)rex(o,true,ext,0,dr);
                if(v>=-128&&v<=127){o.push_back(0x83);o.push_back(modrm(3,ext,dr));o.push_back((uint8_t)(int8_t)v);}
                else{o.push_back(0x81);o.push_back(modrm(3,ext,dr));u32(o,(uint32_t)(int32_t)v);}
                continue;
            }
            throw std::runtime_error("unsupported "+op+": "+line);
        }

        if(starts(line,"imul ")){
            auto [dst,src]=operands(line,5);int dr=im.x64?reg64(dst):reg32(dst),sr=im.x64?reg64(src):reg32(src);if(dr<0||sr<0)throw std::runtime_error("unsupported imul");
            if(im.x64)rex(o,true,dr,0,sr);o.insert(o.end(),{0x0f,0xaf,modrm(3,dr,sr)});continue;
        }
        if(starts(line,"idiv ")){
            std::string r=trim(line.substr(5));int rr=im.x64?reg64(r):reg32(r);if(rr<0)throw std::runtime_error("unsupported idiv");
            if(im.x64)rex(o,true,7,0,rr);o.push_back(0xf7);o.push_back(modrm(3,7,rr));continue;
        }
        if(starts(line,"neg ")){
            std::string r=trim(line.substr(4));int rr=im.x64?reg64(r):reg32(r);if(rr<0)throw std::runtime_error("unsupported neg");
            if(im.x64)rex(o,true,3,0,rr);o.push_back(0xf7);o.push_back(modrm(3,3,rr));continue;
        }
        if(line=="and al,cl"||line=="and al, cl"){o.insert(o.end(),{0x20,0xc8});continue;}
        if(line=="xor eax,eax"||line=="xor eax, eax"){o.insert(o.end(),{0x31,0xc0});continue;}

        if(starts(line,"call ")){
            std::string sym=trim(line.substr(5));if(sym.size()>4&&sym.substr(sym.size()-4)=="@PLT")sym=sym.substr(0,sym.size()-4);
            o.push_back(0xe8);size_t p=o.size();u32(o,0);im.local_fixes.push_back({p,sym,p+4});continue;
        }
        if(starts(line,"jmp ")){
            std::string lab=trim(line.substr(4));o.push_back(0xe9);size_t p=o.size();u32(o,0);im.local_fixes.push_back({p,lab,p+4});continue;
        }
        if(starts(line,"je ")){
            std::string lab=trim(line.substr(3));o.insert(o.end(),{0x0f,0x84});size_t p=o.size();u32(o,0);im.local_fixes.push_back({p,lab,p+4});continue;
        }

        throw std::runtime_error("unsupported AOT instruction: "+line);
    }

    // Resolve same-section branches/calls. Unknown call labels become external
    // relocations; jumps must always remain local.
    std::vector<LocalFix> unresolved;
    for(auto&fx:im.local_fixes){
        auto it=im.text_labels.find(fx.label);
        if(it!=im.text_labels.end()){
            int64_t d=(int64_t)it->second-(int64_t)fx.next;patch32(im.text,fx.offset,(uint32_t)(int32_t)d);
        }else{
            // The byte before the relocation is E8 for calls. Only calls may
            // legally target external symbols.
            bool call = fx.offset>0 && im.text[fx.offset-1]==0xe8;
            if(!call)throw std::runtime_error("unknown local branch label: "+fx.label);
            im.relocs.push_back({fx.offset,fx.label,RelocKind::Rel32,true});
            im.externs.insert(fx.label);
        }
    }
    return im;
}

struct ObjSymbol{std::string name;uint32_t value=0;int16_t section=0;bool global=false;bool function=false;};

static std::vector<ObjSymbol> collect_symbols(const AsmImage&im){
    std::vector<ObjSymbol> syms;
    for(auto&kv:im.data_labels)syms.push_back({kv.first,(uint32_t)kv.second,2,false,false});
    for(auto&g:im.globals){auto it=im.text_labels.find(g);if(it!=im.text_labels.end())syms.push_back({g,(uint32_t)it->second,1,true,true});}
    std::set<std::string> known;for(auto&s:syms)known.insert(s.name);
    for(auto&e:im.externs)if(!known.count(e)){syms.push_back({e,0,0,true,true});known.insert(e);}
    for(auto&r:im.relocs)if(!known.count(r.symbol)&&!im.text_labels.count(r.symbol)){syms.push_back({r.symbol,0,0,true,r.call});known.insert(r.symbol);}
    return syms;
}

static uint32_t stradd(std::vector<uint8_t>&tab,const std::string&s){
    uint32_t off=(uint32_t)tab.size();tab.insert(tab.end(),s.begin(),s.end());tab.push_back(0);return off;
}

static void coff_name(std::vector<uint8_t>&o,const std::string&name,std::vector<uint8_t>&strings){
    if(name.size()<=8){put_name8(o,name);return;}
    u32(o,0);u32(o,stradd(strings,name));
}

static int write_coff(const AsmImage&im,const std::string&out){
    auto syms=collect_symbols(im);
    std::unordered_map<std::string,uint32_t> sidx;
    for(uint32_t i=0;i<syms.size();++i)sidx[syms[i].name]=i;
    std::vector<uint8_t> strings(4,0);
    const uint16_t machine=im.x64?0x8664:0x014c;
    const uint16_t rel32=im.x64?0x0004:0x0014,abs32=0x0006;
    uint32_t hdr=20+2*40;
    uint32_t text_off=hdr;
    uint32_t rdata_off=text_off+(uint32_t)im.text.size();
    uint32_t reloc_off=rdata_off+(uint32_t)im.rodata.size();
    uint32_t sym_off=reloc_off+(uint32_t)im.relocs.size()*10u;

    std::vector<uint8_t> b;
    u16(b,machine);u16(b,2);u32(b,0);u32(b,sym_off);u32(b,(uint32_t)syms.size());u16(b,0);u16(b,0);
    put_name8(b,".text");u32(b,0);u32(b,0);u32(b,(uint32_t)im.text.size());u32(b,text_off);u32(b,reloc_off);u32(b,0);u16(b,(uint16_t)im.relocs.size());u16(b,0);u32(b,0x60000020);
    put_name8(b,".rdata");u32(b,0);u32(b,0);u32(b,(uint32_t)im.rodata.size());u32(b,rdata_off);u32(b,0);u32(b,0);u16(b,0);u16(b,0);u32(b,0x40000040);
    b.insert(b.end(),im.text.begin(),im.text.end());b.insert(b.end(),im.rodata.begin(),im.rodata.end());
    for(auto&r:im.relocs){
        auto it=sidx.find(r.symbol);if(it==sidx.end())throw std::runtime_error("COFF relocation symbol missing: "+r.symbol);
        u32(b,(uint32_t)r.offset);u32(b,it->second);u16(b,r.kind==RelocKind::Abs32?abs32:rel32);
    }
    for(auto&s:syms){
        std::string oname=s.name;
        if(!im.x64 && s.global && (oname.empty() || oname[0]!='_')) oname="_"+oname;
        coff_name(b,oname,strings);u32(b,s.value);u16(b,(uint16_t)s.section);u16(b,s.function?0x20:0);u8(b,s.global?2:3);u8(b,0);
    }
    uint32_t ssize=(uint32_t)strings.size();for(int i=0;i<4;++i)strings[i]=(uint8_t)(ssize>>(8*i));b.insert(b.end(),strings.begin(),strings.end());
    std::ofstream f(out,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("cannot write object: "+out);f.write((char*)b.data(),(std::streamsize)b.size());
    std::cout<<"jauas: wrote "<<out<<" ("<<(im.x64?"COFF x86-64":"COFF x86")<<", "<<b.size()<<" bytes)\n";return 0;
}

static int write_elf64(const AsmImage&im,const std::string&out){
    auto syms0=collect_symbols(im);
    std::vector<ObjSymbol> syms;
    for(auto&s:syms0)if(!s.global)syms.push_back(s);
    size_t local_count=1+syms.size();
    for(auto&s:syms0)if(s.global)syms.push_back(s);

    std::vector<uint8_t> strtab(1,0),symtab(24,0);std::unordered_map<std::string,uint32_t>sidx;
    for(uint32_t i=0;i<syms.size();++i){
        auto&s=syms[i];uint32_t no=stradd(strtab,s.name);sidx[s.name]=i+1;
        u32(symtab,no);u8(symtab,(uint8_t)(((s.global?1:0)<<4)|(s.function?2:1)));u8(symtab,0);u16(symtab,(uint16_t)(s.section==2?3:s.section));u64(symtab,s.value);u64(symtab,0);
    }
    std::vector<uint8_t> rela;
    for(auto&r:im.relocs){
        auto it=sidx.find(r.symbol);if(it==sidx.end())throw std::runtime_error("ELF relocation symbol missing: "+r.symbol);
        uint32_t type=r.call?4:2;u64(rela,r.offset);u64(rela,((uint64_t)it->second<<32)|type);i64(rela,-4);
    }
    std::vector<uint8_t> shstr(1,0);uint32_t n_text=stradd(shstr,".text"),n_rela=stradd(shstr,".rela.text"),n_ro=stradd(shstr,".rodata"),n_sym=stradd(shstr,".symtab"),n_str=stradd(shstr,".strtab"),n_sh=stradd(shstr,".shstrtab");
    std::vector<uint8_t> b(64,0);
    auto append=[&](const std::vector<uint8_t>&v,size_t align){align_to(b,align);uint64_t off=b.size();b.insert(b.end(),v.begin(),v.end());return off;};
    uint64_t off_text=append(im.text,16),off_rela=append(rela,8),off_ro=append(im.rodata,1),off_sym=append(symtab,8),off_str=append(strtab,1),off_shstr=append(shstr,1);
    align_to(b,8);uint64_t shoff=b.size();
    auto sh=[&](uint32_t name,uint32_t type,uint64_t flags,uint64_t off,uint64_t size,uint32_t link,uint32_t info,uint64_t al,uint64_t ents){
        u32(b,name);u32(b,type);u64(b,flags);u64(b,0);u64(b,off);u64(b,size);u32(b,link);u32(b,info);u64(b,al);u64(b,ents);
    };
    sh(0,0,0,0,0,0,0,0,0);
    sh(n_text,1,0x6,off_text,im.text.size(),0,0,16,0);
    sh(n_rela,4,0,off_rela,rela.size(),4,1,8,24);
    sh(n_ro,1,0x2,off_ro,im.rodata.size(),0,0,1,0);
    sh(n_sym,2,0,off_sym,symtab.size(),5,(uint32_t)local_count,8,24);
    sh(n_str,3,0,off_str,strtab.size(),0,0,1,0);
    sh(n_sh,3,0,off_shstr,shstr.size(),0,0,1,0);

    auto put16=[&](size_t p,uint16_t v){b[p]=v;b[p+1]=v>>8;};
    auto put32=[&](size_t p,uint32_t v){for(int i=0;i<4;++i)b[p+i]=(uint8_t)(v>>(8*i));};
    auto put64=[&](size_t p,uint64_t v){for(int i=0;i<8;++i)b[p+i]=(uint8_t)(v>>(8*i));};
    b[0]=0x7f;b[1]='E';b[2]='L';b[3]='F';b[4]=2;b[5]=1;b[6]=1;
    put16(16,1);put16(18,62);put32(20,1);put64(40,shoff);put16(52,64);put16(58,64);put16(60,7);put16(62,6);
    std::ofstream f(out,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("cannot write object: "+out);f.write((char*)b.data(),(std::streamsize)b.size());
    std::cout<<"jauas: wrote "<<out<<" (ELF64 relocatable, "<<b.size()<<" bytes)\n";return 0;
}

static int write_elf32(AsmImage im,const std::string&out){
    auto syms0=collect_symbols(im);
    std::vector<ObjSymbol> syms;
    for(auto&s:syms0)if(!s.global)syms.push_back(s);
    size_t local_count=1+syms.size();
    for(auto&s:syms0)if(s.global)syms.push_back(s);

    std::vector<uint8_t> strtab(1,0),symtab(16,0);std::unordered_map<std::string,uint32_t>sidx;
    for(uint32_t i=0;i<syms.size();++i){
        auto&s=syms[i];uint32_t no=stradd(strtab,s.name);sidx[s.name]=i+1;
        u32(symtab,no);u32(symtab,s.value);u32(symtab,0);u8(symtab,(uint8_t)(((s.global?1:0)<<4)|(s.function?2:1)));u8(symtab,0);u16(symtab,(uint16_t)(s.section==2?3:s.section));
    }
    std::vector<uint8_t> rel;
    for(auto&r:im.relocs){
        auto it=sidx.find(r.symbol);if(it==sidx.end())throw std::runtime_error("ELF32 relocation symbol missing: "+r.symbol);
        uint32_t type=r.kind==RelocKind::Abs32?1:2;
        if(type==2)patch32(im.text,r.offset,0xfffffffcU);
        u32(rel,(uint32_t)r.offset);u32(rel,(it->second<<8)|type);
    }
    std::vector<uint8_t> shstr(1,0);uint32_t n_text=stradd(shstr,".text"),n_rel=stradd(shstr,".rel.text"),n_ro=stradd(shstr,".rodata"),n_sym=stradd(shstr,".symtab"),n_str=stradd(shstr,".strtab"),n_sh=stradd(shstr,".shstrtab");
    std::vector<uint8_t> b(52,0);
    auto append=[&](const std::vector<uint8_t>&v,size_t align){align_to(b,align);uint32_t off=(uint32_t)b.size();b.insert(b.end(),v.begin(),v.end());return off;};
    uint32_t off_text=append(im.text,16),off_rel=append(rel,4),off_ro=append(im.rodata,1),off_sym=append(symtab,4),off_str=append(strtab,1),off_shstr=append(shstr,1);
    align_to(b,4);uint32_t shoff=(uint32_t)b.size();
    auto sh=[&](uint32_t name,uint32_t type,uint32_t flags,uint32_t off,uint32_t size,uint32_t link,uint32_t info,uint32_t al,uint32_t ents){
        u32(b,name);u32(b,type);u32(b,flags);u32(b,0);u32(b,off);u32(b,size);u32(b,link);u32(b,info);u32(b,al);u32(b,ents);
    };
    sh(0,0,0,0,0,0,0,0,0);sh(n_text,1,0x6,off_text,im.text.size(),0,0,16,0);sh(n_rel,9,0,off_rel,rel.size(),4,1,4,8);sh(n_ro,1,0x2,off_ro,im.rodata.size(),0,0,1,0);sh(n_sym,2,0,off_sym,symtab.size(),5,(uint32_t)local_count,4,16);sh(n_str,3,0,off_str,strtab.size(),0,0,1,0);sh(n_sh,3,0,off_shstr,shstr.size(),0,0,1,0);
    auto put16=[&](size_t p,uint16_t v){b[p]=v;b[p+1]=v>>8;};auto put32=[&](size_t p,uint32_t v){for(int i=0;i<4;++i)b[p+i]=(uint8_t)(v>>(8*i));};
    b[0]=0x7f;b[1]='E';b[2]='L';b[3]='F';b[4]=1;b[5]=1;b[6]=1;put16(16,1);put16(18,3);put32(20,1);put32(32,shoff);put16(40,52);put16(46,40);put16(48,7);put16(50,6);
    std::ofstream f(out,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("cannot write object: "+out);f.write((char*)b.data(),(std::streamsize)b.size());
    std::cout<<"jauas: wrote "<<out<<" (ELF32 relocatable, "<<b.size()<<" bytes)\n";return 0;
}

static int build_object(const std::string&in,const std::string&out,const std::string&target){
    auto im=assemble_object_text(in,target);
    if(im.windows)return write_coff(im,out);
    return im.x64?write_elf64(im,out):write_elf32(im,out);
}

int main(int argc,char**argv){
    try{
        if(argc==1){
            std::cout<<"jauas 0.6\n"
                     <<"usage: jauas <input.s> -o <output> --target <linux-x86_64|linux-x86|windows-x86_64|windows-x86> [--object]\n"
                     <<"  Linux without --object: freestanding ELF executable for bootstrap subset\n"
                     <<"  --object: ELF .o on Linux, COFF .obj on Windows targets\n";
            return 0;
        }
        if(argc<4)throw std::runtime_error("missing arguments; run jauas with no arguments for help");
        std::string in=argv[1],out,target="linux-x86_64";bool object=false;
        for(int i=2;i<argc;++i){
            std::string a=argv[i];
            if(a=="-o"&&i+1<argc)out=argv[++i];
            else if(a=="--target"&&i+1<argc)target=argv[++i];
            else if(a=="--object"||a=="--format=object")object=true;
        }
        if(out.empty())throw std::runtime_error("missing -o");
        if(starts(target,"windows-"))object=true;
        return object?build_object(in,out,target):build_legacy_elf(in,out,target);
    }catch(const std::exception&e){
        std::cerr<<"jauas: "<<e.what()<<"\n";return 1;
    }
}
