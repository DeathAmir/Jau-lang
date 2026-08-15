#include "jau/jau.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace jau {
namespace fs = std::filesystem;

static constexpr char MAGIC_V1[8] = {'J','A','U','P','K','G','1','\n'};
static constexpr char MAGIC_V2[8] = {'J','A','U','P','K','G','2','\n'};
static constexpr uint64_t KEY_MANIFEST = 0x8d4f6a13c2e9b751ull;
static constexpr uint64_t KEY_PATH     = 0x41a7d38ef519c60bull;
static constexpr uint64_t KEY_DATA     = 0xd36b24a9f17e8055ull;

struct PackageHeader { uint32_t version=0; uint64_t salt=0; };

static std::string read_all(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + p.string());
    return std::string((std::istreambuf_iterator<char>(f)), {});
}
static std::string trim(std::string x) {
    size_t a=0,b=x.size();
    while(a<b && std::isspace((unsigned char)x[a])) ++a;
    while(b>a && std::isspace((unsigned char)x[b-1])) --b;
    return x.substr(a,b-a);
}
static std::string manifest_value_local(const std::string& text,const std::string& key) {
    std::istringstream in(text); std::string line;
    while(std::getline(in,line)) {
        line=trim(line); if(line.empty()||line[0]=='#') continue;
        auto q=line.find('='); if(q==std::string::npos) continue;
        if(trim(line.substr(0,q))==key) {
            auto v=trim(line.substr(q+1));
            if(v.size()>=2 && v.front()=='"' && v.back()=='"') v=v.substr(1,v.size()-2);
            return v;
        }
    }
    return "";
}
static uint64_t hash_bytes(const char* data,size_t n) {
    uint64_t h=1469598103934665603ull;
    for(size_t i=0;i<n;++i){h^=(unsigned char)data[i];h*=1099511628211ull;}
    return h;
}
static uint64_t hash_string(const std::string& s){ return hash_bytes(s.data(),s.size()); }
static void w32(std::ostream& o,uint32_t v){o.write(reinterpret_cast<const char*>(&v),4);}
static void w64(std::ostream& o,uint64_t v){o.write(reinterpret_cast<const char*>(&v),8);}
static uint32_t r32(std::istream& i){uint32_t v=0;i.read(reinterpret_cast<char*>(&v),4);if(!i)throw std::runtime_error("truncated .jaup");return v;}
static uint64_t r64(std::istream& i){uint64_t v=0;i.read(reinterpret_cast<char*>(&v),8);if(!i)throw std::runtime_error("truncated .jaup");return v;}
static void wstr_plain(std::ostream&o,const std::string&s){w32(o,(uint32_t)s.size());o.write(s.data(),(std::streamsize)s.size());}
static std::string rstr_plain(std::istream&i,uint32_t max=16u*1024u*1024u){auto n=r32(i);if(n>max)throw std::runtime_error("oversized .jaup string");std::string s(n,'\0');i.read(s.data(),n);if(!i)throw std::runtime_error("truncated .jaup string");return s;}

// JAUPKG2 uses a deterministic stream transform so source text is not stored
// as readable plaintext in package archives. This is source obfuscation, not a
// substitute for cryptographic signing or secret-key encryption.
static uint64_t stream_word(uint64_t& s) {
    if(!s) s=0x9e3779b97f4a7c15ull;
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return s * 0x2545f4914f6cdd1dull;
}
static void crypt_in_place(char* data,size_t n,uint64_t seed) {
    uint64_t state=seed^0xa5b35705c1d2e48full,word=0;
    for(size_t i=0;i<n;++i){if((i&7u)==0)word=stream_word(state);data[i]=(char)((unsigned char)data[i]^((word>>((i&7u)*8u))&0xffu));}
}
static std::string crypt_copy(std::string s,uint64_t seed){if(!s.empty())crypt_in_place(s.data(),s.size(),seed);return s;}
static void wstr_crypt(std::ostream&o,const std::string&s,uint64_t seed){auto x=crypt_copy(s,seed);w32(o,(uint32_t)x.size());o.write(x.data(),(std::streamsize)x.size());}
static std::string rstr_crypt(std::istream&i,uint64_t seed,uint32_t max=16u*1024u*1024u){auto s=rstr_plain(i,max);if(!s.empty())crypt_in_place(s.data(),s.size(),seed);return s;}
static void write_crypt(std::ostream& out,const std::string& data,uint64_t seed){auto x=crypt_copy(data,seed);out.write(x.data(),(std::streamsize)x.size());}
static std::string read_crypt(std::istream& in,uint64_t n,uint64_t seed){if(n>256ull*1024ull*1024ull)throw std::runtime_error("package file too large");std::string x((size_t)n,'\0');in.read(x.data(),(std::streamsize)n);if(!in)throw std::runtime_error("truncated package payload");if(!x.empty())crypt_in_place(x.data(),x.size(),seed);return x;}

static bool safe_rel(const std::string& x) {
    if(x.empty()) return false; fs::path p(x);
    if(p.is_absolute()||p.has_root_name()||p.has_root_directory()) return false;
    auto n=p.lexically_normal(); for(auto&part:n) if(part=="..") return false;
    return n.string()!=".";
}
static bool valid_name(const std::string& n) {
    if(n.empty()||n.size()>96) return false;
    for(unsigned char c:n) if(!(std::isalnum(c)||c=='_'||c=='-'||c=='.')) return false;
    return true;
}
static PackageHeader header(std::istream& in) {
    char magic[8]{}; in.read(magic,8); if(!in) throw std::runtime_error("truncated .jaup header");
    PackageHeader h;
    if(std::memcmp(magic,MAGIC_V1,8)==0){h.version=r32(in);if(h.version!=1)throw std::runtime_error("unsupported .jaup v1 version");return h;}
    if(std::memcmp(magic,MAGIC_V2,8)==0){h.version=r32(in);if(h.version!=2)throw std::runtime_error("unsupported .jaup v2 version");h.salt=r64(in);return h;}
    throw std::runtime_error("not a Jau package (.jaup)");
}
static std::string read_manifest_after_header(std::istream& in,const PackageHeader& h){return h.version==1?rstr_plain(in,4u*1024u*1024u):rstr_crypt(in,h.salt^KEY_MANIFEST,4u*1024u*1024u);}
static std::string read_path(std::istream& in,const PackageHeader& h,uint32_t index){return h.version==1?rstr_plain(in,4096):rstr_crypt(in,h.salt^KEY_PATH^((uint64_t)index*0x9e3779b97f4a7c15ull),4096);}
static uint64_t data_seed(const PackageHeader& h,uint32_t index,const std::string& rel,uint64_t size,uint64_t hash){return h.salt^KEY_DATA^hash_string(rel)^size^hash^((uint64_t)index*0xd6e8feb86659fd93ull);}

std::string package_manifest(const std::string& archive) {
    std::ifstream in(archive,std::ios::binary); if(!in) throw std::runtime_error("cannot open package: "+archive);
    auto h=header(in); return read_manifest_after_header(in,h);
}

bool package_verify(const std::string& archive) {
    std::ifstream in(archive,std::ios::binary); if(!in) throw std::runtime_error("cannot open package: "+archive);
    auto hd=header(in); auto manifest=read_manifest_after_header(in,hd);
    auto name=manifest_value_local(manifest,"name"),ver=manifest_value_local(manifest,"version"),main=manifest_value_local(manifest,"main");
    if(!valid_name(name)||ver.empty()||main.empty()||!safe_rel(main)) throw std::runtime_error("invalid jau.pkg metadata");
    auto count=r32(in); if(count>10000) throw std::runtime_error("package contains too many files");
    uint64_t total=0; std::unordered_set<std::string> seen;
    for(uint32_t q=0;q<count;++q) {
        auto rel=read_path(in,hd,q); if(!safe_rel(rel)||!seen.insert(rel).second) throw std::runtime_error("unsafe or duplicate package path: "+rel);
        auto size=r64(in),want=r64(in);
        if(size>256ull*1024ull*1024ull||total+size>1024ull*1024ull*1024ull) throw std::runtime_error("package size limit exceeded"); total+=size;
        std::string data;
        if(hd.version==1){data.resize((size_t)size);in.read(data.data(),(std::streamsize)size);if(!in)throw std::runtime_error("truncated package payload");}
        else data=read_crypt(in,size,data_seed(hd,q,rel,size,want));
        if(hash_string(data)!=want) throw std::runtime_error("package hash mismatch: "+rel);
    }
    char extra=0; if(in.read(&extra,1)) throw std::runtime_error("unexpected trailing package data");
    return true;
}

std::string package_pack(const std::string& root,const std::string& output) {
    fs::path base=fs::weakly_canonical(root);
    if(!fs::exists(base)||!fs::is_directory(base)) throw std::runtime_error("package root is not a directory: "+root);
    auto manifest=read_all(base/"jau.pkg");
    auto name=manifest_value_local(manifest,"name"),ver=manifest_value_local(manifest,"version"),main=manifest_value_local(manifest,"main");
    if(!valid_name(name)) throw std::runtime_error("invalid package name");
    if(ver.empty()) throw std::runtime_error("jau.pkg requires version");
    if(main.empty()||!safe_rel(main)||!fs::exists(base/main)) throw std::runtime_error("jau.pkg main is missing or unsafe");
    std::vector<std::pair<std::string,fs::path>> files; std::error_code ec; auto out_abs=fs::absolute(output).lexically_normal();
    for(fs::recursive_directory_iterator it(base,fs::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)) {
        if(ec){ec.clear();continue;} auto rel=fs::relative(it->path(),base,ec); if(ec){ec.clear();continue;}
        auto first=rel.begin()!=rel.end()?(*rel.begin()).string():std::string();
        if(it->is_directory(ec)&&(first==".git"||first=="dist"||first==".jau"||first.rfind("build",0)==0)){it.disable_recursion_pending();continue;}
        if(it->is_symlink(ec)||!it->is_regular_file(ec)) continue;
        if(fs::absolute(it->path()).lexically_normal()==out_abs) continue;
        auto rs=rel.generic_string(); if(!safe_rel(rs)) throw std::runtime_error("unsafe package path: "+rs);
        files.push_back({rs,it->path()}); if(files.size()>10000) throw std::runtime_error("too many files in package");
    }
    std::sort(files.begin(),files.end(),[](auto&a,auto&b){return a.first<b.first;});
    fs::path outp(output); if(outp.has_parent_path()) fs::create_directories(outp.parent_path());
    std::ofstream out(outp,std::ios::binary|std::ios::trunc); if(!out) throw std::runtime_error("cannot create package: "+output);
    uint64_t salt=hash_string(manifest)^((uint64_t)files.size()*0x9e3779b97f4a7c15ull)^0x6a6175705f763200ull;if(!salt)salt=0x13579bdf2468ace0ull;
    out.write(MAGIC_V2,8); w32(out,2); w64(out,salt); wstr_crypt(out,manifest,salt^KEY_MANIFEST); w32(out,(uint32_t)files.size()); uint64_t total=0;
    PackageHeader hd{2,salt}; uint32_t index=0;
    for(auto&item:files){auto data=read_all(item.second);total+=data.size();if(total>1024ull*1024ull*1024ull)throw std::runtime_error("package size limit exceeded");auto h=hash_string(data);wstr_crypt(out,item.first,salt^KEY_PATH^((uint64_t)index*0x9e3779b97f4a7c15ull));w64(out,(uint64_t)data.size());w64(out,h);write_crypt(out,data,data_seed(hd,index,item.first,(uint64_t)data.size(),h));++index;}
    out.close(); package_verify(output); return name+"@"+ver;
}

std::string package_read_file(const std::string& archive,const std::string& relative_path) {
    if(!safe_rel(relative_path)) throw std::runtime_error("unsafe package path: "+relative_path);
    package_verify(archive);
    std::ifstream in(archive,std::ios::binary); auto hd=header(in); (void)read_manifest_after_header(in,hd); auto count=r32(in);
    for(uint32_t q=0;q<count;++q){auto rel=read_path(in,hd,q);auto size=r64(in),want=r64(in);if(rel==relative_path){std::string data;if(hd.version==1){data.resize((size_t)size);in.read(data.data(),(std::streamsize)size);if(!in)throw std::runtime_error("truncated package payload");}else data=read_crypt(in,size,data_seed(hd,q,rel,size,want));if(hash_string(data)!=want)throw std::runtime_error("package hash mismatch: "+rel);return data;}if(hd.version==1)in.seekg((std::streamoff)size,std::ios::cur);else{std::string skip((size_t)size,'\0');in.read(skip.data(),(std::streamsize)size);if(!in)throw std::runtime_error("truncated package payload");}}
    throw std::runtime_error("package file not found: "+relative_path);
}

std::string package_extract(const std::string& archive,const std::string& destination) {
    package_verify(archive); std::ifstream in(archive,std::ios::binary); auto hd=header(in); auto manifest=read_manifest_after_header(in,hd); auto count=r32(in);
    fs::path base=fs::absolute(destination).lexically_normal(); fs::create_directories(base);
    for(uint32_t q=0;q<count;++q) {
        auto rel=read_path(in,hd,q); auto size=r64(in); auto want=r64(in); fs::path out=(base/fs::path(rel)).lexically_normal();
        auto b=base.generic_string(),a=out.generic_string(); if(a.size()<b.size()||a.compare(0,b.size(),b)!=0||(a.size()>b.size()&&a[b.size()]!='/')) throw std::runtime_error("package path escaped destination");
        std::string data;if(hd.version==1){data.resize((size_t)size);in.read(data.data(),(std::streamsize)size);if(!in)throw std::runtime_error("truncated package payload");}else data=read_crypt(in,size,data_seed(hd,q,rel,size,want));
        if(hash_string(data)!=want)throw std::runtime_error("package hash mismatch: "+rel);
        if(out.has_parent_path()) fs::create_directories(out.parent_path()); std::ofstream f(out,std::ios::binary|std::ios::trunc); if(!f) throw std::runtime_error("cannot extract: "+out.string());f.write(data.data(),(std::streamsize)data.size());
    }
    return manifest;
}
}
