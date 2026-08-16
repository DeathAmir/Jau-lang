#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <cstdlib>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

static uint16_t r16(const std::vector<uint8_t>& b, size_t p){ if(p+2>b.size()) throw std::runtime_error("truncated input"); return (uint16_t)(b[p]|(b[p+1]<<8)); }
static uint32_t r32(const std::vector<uint8_t>& b, size_t p){ if(p+4>b.size()) throw std::runtime_error("truncated input"); return (uint32_t)b[p]|((uint32_t)b[p+1]<<8)|((uint32_t)b[p+2]<<16)|((uint32_t)b[p+3]<<24); }
static uint64_t r64(const std::vector<uint8_t>& b, size_t p){ uint64_t v=0; for(int i=0;i<8;++i)v|=(uint64_t)b.at(p+i)<<(8*i); return v; }
static void w16(std::vector<uint8_t>& b,uint16_t v){ b.push_back((uint8_t)v); b.push_back((uint8_t)(v>>8)); }
static void w32(std::vector<uint8_t>& b,uint32_t v){ for(int i=0;i<4;++i)b.push_back((uint8_t)(v>>(8*i))); }
static void w64(std::vector<uint8_t>& b,uint64_t v){ for(int i=0;i<8;++i)b.push_back((uint8_t)(v>>(8*i))); }
static void p16(std::vector<uint8_t>& b,size_t p,uint16_t v){ if(p+2>b.size())throw std::runtime_error("patch overflow");b[p]=(uint8_t)v;b[p+1]=(uint8_t)(v>>8); }
static void p32(std::vector<uint8_t>& b,size_t p,uint32_t v){ if(p+4>b.size())throw std::runtime_error("patch overflow");for(int i=0;i<4;++i)b[p+i]=(uint8_t)(v>>(8*i)); }
static void p64(std::vector<uint8_t>& b,size_t p,uint64_t v){ if(p+8>b.size())throw std::runtime_error("patch overflow");for(int i=0;i<8;++i)b[p+i]=(uint8_t)(v>>(8*i)); }
static uint32_t align32(uint32_t x,uint32_t a){ return (x+a-1u)&~(a-1u); }
static size_t alignsz(size_t x,size_t a){ return (x+a-1u)&~(a-1u); }
static std::vector<uint8_t> read_file(const fs::path& p){ std::ifstream f(p,std::ios::binary); if(!f)throw std::runtime_error("cannot open "+p.string()); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{}); }
static void write_file(const fs::path& p,const std::vector<uint8_t>& b){ std::ofstream f(p,std::ios::binary|std::ios::trunc); if(!f)throw std::runtime_error("cannot write "+p.string()); f.write((const char*)b.data(),(std::streamsize)b.size()); }
static std::string lower(std::string s){for(char&c:s)c=(char)std::tolower((unsigned char)c);return s;}

struct Reloc { uint32_t off=0,sym=0; uint16_t type=0; };
struct InSection {
    std::string name;
    std::vector<uint8_t> data;
    uint32_t virtual_size=0,chars=0;
    std::vector<Reloc> relocs;
    int out_group=-1;
    uint32_t out_off=0;
};
struct Symbol {
    std::string name;
    uint32_t value=0;
    int16_t section=0;
    uint16_t type=0;
    uint8_t storage=0,aux=0;
    bool valid=false,aux_entry=false;
};
struct Object {
    fs::path path;
    uint16_t machine=0;
    std::vector<InSection> sections;
    std::vector<Symbol> symbols;
