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
static void wstr(std::ostream&o,const std::string&s){w32(o,(uint32_t)s.size());o.write(s.data(),(std::streamsize)s.size());}
static std::string rstr(std::istream&i,uint32_t max=16u*1024u*1024u){auto n=r32(i);if(n>max)throw std::runtime_error("oversized .jaup string");std::string s(n,'\0');i.read(s.data(),n);if(!i)throw std::runtime_error("truncated .jaup string");return s;}
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
static void header(std::istream& in) {
    char magic[8]{}; in.read(magic,8);
    if(!in||std::memcmp(magic,"JAUPKG1\n",8)!=0) throw std::runtime_error("not a Jau package (.jaup)");
    if(r32(in)!=1) throw std::runtime_error("unsupported .jaup version");
}

std::string package_manifest(const std::string& archive) {
    std::ifstream in(archive,std::ios::binary); if(!in) throw std::runtime_error("cannot open package: "+archive);
    header(in); return rstr(in,4u*1024u*1024u);
}

bool package_verify(const std::string& archive) {
    std::ifstream in(archive,std::ios::binary); if(!in) throw std::runtime_error("cannot open package: "+archive);
    header(in); auto manifest=rstr(in,4u*1024u*1024u);
    auto name=manifest_value_local(manifest,"name"),ver=manifest_value_local(manifest,"version"),main=manifest_value_local(manifest,"main");
    if(!valid_name(name)||ver.empty()||main.empty()||!safe_rel(main)) throw std::runtime_error("invalid jau.pkg metadata");
    auto count=r32(in); if(count>10000) throw std::runtime_error("package contains too many files");
    uint64_t total=0; std::unordered_set<std::string> seen; std::array<char,65536> buf{};
    for(uint32_t q=0;q<count;++q) {
        auto rel=rstr(in,4096); if(!safe_rel(rel)||!seen.insert(rel).second) throw std::runtime_error("unsafe or duplicate package path: "+rel);
        auto size=r64(in), want=r64(in);
        if(size>256ull*1024ull*1024ull||total+size>1024ull*1024ull*1024ull) throw std::runtime_error("package size limit exceeded"); total+=size;
        uint64_t h=1469598103934665603ull,left=size;
        while(left){auto n=(std::streamsize)std::min<uint64_t>(left,buf.size());in.read(buf.data(),n);if(!in)throw std::runtime_error("truncated package payload");for(std::streamsize k=0;k<n;++k){h^=(unsigned char)buf[(size_t)k];h*=1099511628211ull;}left-=(uint64_t)n;}
        if(h!=want) throw std::runtime_error("package hash mismatch: "+rel);
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
    out.write("JAUPKG1\n",8); w32(out,1); wstr(out,manifest); w32(out,(uint32_t)files.size()); uint64_t total=0;
    for(auto&item:files){auto data=read_all(item.second);total+=data.size();if(total>1024ull*1024ull*1024ull)throw std::runtime_error("package size limit exceeded");wstr(out,item.first);w64(out,(uint64_t)data.size());w64(out,hash_string(data));out.write(data.data(),(std::streamsize)data.size());}
    out.close(); package_verify(output); return name+"@"+ver;
}

std::string package_extract(const std::string& archive,const std::string& destination) {
    package_verify(archive); std::ifstream in(archive,std::ios::binary); header(in); auto manifest=rstr(in,4u*1024u*1024u); auto count=r32(in);
    fs::path base=fs::absolute(destination).lexically_normal(); fs::create_directories(base); std::array<char,65536> buf{};
    for(uint32_t q=0;q<count;++q) {
        auto rel=rstr(in,4096); auto size=r64(in); (void)r64(in); fs::path out=(base/fs::path(rel)).lexically_normal();
        auto b=base.generic_string(),a=out.generic_string(); if(a.size()<b.size()||a.compare(0,b.size(),b)!=0||(a.size()>b.size()&&a[b.size()]!='/')) throw std::runtime_error("package path escaped destination");
        if(out.has_parent_path()) fs::create_directories(out.parent_path()); std::ofstream f(out,std::ios::binary|std::ios::trunc); if(!f) throw std::runtime_error("cannot extract: "+out.string());
        uint64_t left=size; while(left){auto n=(std::streamsize)std::min<uint64_t>(left,buf.size());in.read(buf.data(),n);if(!in)throw std::runtime_error("truncated package payload");f.write(buf.data(),n);left-=(uint64_t)n;}
    }
    return manifest;
}
}
