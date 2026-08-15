#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
namespace fs=std::filesystem;
static std::string trim(std::string s){size_t a=0,b=s.size();while(a<b&&std::isspace((unsigned char)s[a]))++a;while(b>a&&std::isspace((unsigned char)s[b-1]))--b;return s.substr(a,b-a);}
static void u16(std::vector<uint8_t>&o,uint16_t v){o.push_back(v);o.push_back(v>>8);}static void u32(std::vector<uint8_t>&o,uint32_t v){for(int i=0;i<4;++i)o.push_back(v>>(8*i));}static void u64(std::vector<uint8_t>&o,uint64_t v){for(int i=0;i<8;++i)o.push_back(v>>(8*i));}
static void patch32(std::vector<uint8_t>&o,size_t p,uint32_t v){for(int i=0;i<4;++i)o[p+i]=(uint8_t)(v>>(8*i));}
static std::string unquote(const std::string&s){auto a=s.find('"'),b=s.rfind('"');if(a==std::string::npos||b<=a)throw std::runtime_error("bad .ascii string");std::string r;for(size_t i=a+1;i<b;++i){char c=s[i];if(c=='\\'&&i+1<b){char e=s[++i];if(e=='n')r+='\n';else if(e=='r')r+='\r';else if(e=='t')r+='\t';else if(e=='0')r+='\0';else r+=e;}else r+=c;}return r;}
struct Fix{size_t pos;std::string label;bool rip;};
int main(int argc,char**argv){try{
    if(argc<4){std::cout<<"jauas 0.3\nusage: jauas <input.s> -o <output> --target <linux-x86_64|linux-x86>\n";return argc==1?0:2;}
    std::string in=argv[1],out,target="linux-x86_64";for(int i=2;i<argc;++i){std::string a=argv[i];if(a=="-o"&&i+1<argc)out=argv[++i];else if(a=="--target"&&i+1<argc)target=argv[++i];}
    if(out.empty())throw std::runtime_error("missing -o");bool x64=target=="linux-x86_64";if(!x64&&target!="linux-x86")throw std::runtime_error("jauas currently emits dependency-free Linux ELF x86/x86_64");
    std::ifstream f(in);if(!f)throw std::runtime_error("cannot open input");std::vector<uint8_t> code,data;std::unordered_map<std::string,size_t> clabel,dlabel;std::vector<Fix> fix;bool text=true;std::string line;
    auto imm=[](const std::string&s)->uint64_t{return (uint64_t)std::stoll(trim(s),nullptr,0);};
    while(std::getline(f,line)){auto hash=line.find('#');if(hash!=std::string::npos)line=line.substr(0,hash);line=trim(line);if(line.empty())continue;if(line==".text"){text=true;continue;}if(line.rfind(".section",0)==0){text=line.find(".rodata")==std::string::npos;continue;}if(line[0]=='.')continue;
        std::string label;auto colon=line.find(':');if(colon!=std::string::npos){label=trim(line.substr(0,colon));(text?clabel:dlabel)[label]=text?code.size():data.size();line=trim(line.substr(colon+1));if(line.empty())continue;}
        if(!text){if(line.rfind(".ascii",0)==0){auto q=unquote(line);data.insert(data.end(),q.begin(),q.end());continue;}throw std::runtime_error("unsupported data directive: "+line);}
        if(line=="syscall"){code.push_back(0x0f);code.push_back(0x05);continue;}if(line=="int 0x80"){code.push_back(0xcd);code.push_back(0x80);continue;}
        if(x64&&line.rfind("lea rsi, [rip+",0)==0&&line.back()==']'){std::string l=line.substr(14,line.size()-15);code.insert(code.end(),{0x48,0x8d,0x35});size_t p=code.size();u32(code,0);fix.push_back({p,l,true});continue;}
        if(!x64&&line.rfind("mov ecx,",0)==0){std::string rhs=trim(line.substr(8));if(!rhs.empty()&&!std::isdigit((unsigned char)rhs[0])&&rhs[0]!='-'){code.push_back(0xb9);size_t p=code.size();u32(code,0);fix.push_back({p,rhs,false});continue;}}
        if(line.rfind("mov ",0)==0){auto c=line.find(',');if(c==std::string::npos)throw std::runtime_error("bad mov");std::string r=trim(line.substr(4,c-4)),v=trim(line.substr(c+1));uint64_t n=imm(v);if(x64){uint8_t op=0;if(r=="rax")op=0xb8;else if(r=="rdx")op=0xba;else if(r=="rsi")op=0xbe;else if(r=="rdi")op=0xbf;else throw std::runtime_error("unsupported x64 register: "+r);code.push_back(0x48);code.push_back(op);u64(code,n);}else{uint8_t op=0;if(r=="eax")op=0xb8;else if(r=="ecx")op=0xb9;else if(r=="edx")op=0xba;else if(r=="ebx")op=0xbb;else throw std::runtime_error("unsupported x86 register: "+r);code.push_back(op);u32(code,(uint32_t)n);}continue;}
        throw std::runtime_error("unsupported instruction: "+line);
    }
    size_t hdr=x64?120:84;uint64_t base=x64?0x400000ull:0x08048000ull;if(!clabel.count("_start"))throw std::runtime_error("missing _start label");
    for(auto&x:fix){auto it=dlabel.find(x.label);if(it==dlabel.end())throw std::runtime_error("unknown data label: "+x.label);uint64_t target_addr=base+hdr+code.size()+it->second;if(x.rip){uint64_t next=base+hdr+x.pos+4;int64_t d=(int64_t)target_addr-(int64_t)next;patch32(code,x.pos,(uint32_t)(int32_t)d);}else patch32(code,x.pos,(uint32_t)target_addr);}
    uint64_t filesz=hdr+code.size()+data.size(),entry=base+hdr+clabel["_start"];std::vector<uint8_t> bin;
    bin.insert(bin.end(),{0x7f,'E','L','F',(uint8_t)(x64?2:1),1,1,0});while(bin.size()<16)bin.push_back(0);
    if(x64){u16(bin,2);u16(bin,62);u32(bin,1);u64(bin,entry);u64(bin,64);u64(bin,0);u32(bin,0);u16(bin,64);u16(bin,56);u16(bin,1);u16(bin,0);u16(bin,0);u16(bin,0);u32(bin,1);u32(bin,5);u64(bin,0);u64(bin,base);u64(bin,base);u64(bin,filesz);u64(bin,filesz);u64(bin,0x1000);}else{u16(bin,2);u16(bin,3);u32(bin,1);u32(bin,(uint32_t)entry);u32(bin,52);u32(bin,0);u32(bin,0);u16(bin,52);u16(bin,32);u16(bin,1);u16(bin,0);u16(bin,0);u16(bin,0);u32(bin,1);u32(bin,0);u32(bin,(uint32_t)base);u32(bin,(uint32_t)base);u32(bin,(uint32_t)filesz);u32(bin,(uint32_t)filesz);u32(bin,5);u32(bin,0x1000);}
    if(bin.size()!=hdr)throw std::runtime_error("internal ELF header size error");bin.insert(bin.end(),code.begin(),code.end());bin.insert(bin.end(),data.begin(),data.end());std::ofstream of(out,std::ios::binary);of.write((char*)bin.data(),(std::streamsize)bin.size());of.close();
#ifndef _WIN32
    fs::permissions(out,fs::perms::owner_exec|fs::perms::group_exec|fs::perms::others_exec,fs::perm_options::add);
#endif
    std::cout<<"jauas: wrote "<<out<<" ("<<target<<", "<<bin.size()<<" bytes)\n";return 0;
}catch(const std::exception&e){std::cerr<<"jauas: "<<e.what()<<"\n";return 1;}}
