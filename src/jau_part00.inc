#include "jau/jau.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <random>
#include <thread>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <system_error>
#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace jau {

static std::vector<std::string> g_runtime_args;

#ifdef JAU_HAVE_ASM_FASTOPS
extern "C" long long jau_fast_add_i64(long long, long long);
extern "C" long long jau_fast_sub_i64(long long, long long);
extern "C" long long jau_fast_mul_i64(long long, long long);
#endif

struct Error : std::runtime_error { using std::runtime_error::runtime_error; };

struct Value;
using Array = std::vector<Value>;
using ArrayPtr = std::shared_ptr<Array>;
using ValueData = std::variant<std::monostate, int64_t, double, bool, std::string, ArrayPtr>;
struct Value {
    ValueData data;
    Value() = default;
    template<class T> Value(T v) : data(std::move(v)) {}
    bool truthy() const {
        if (std::holds_alternative<std::monostate>(data)) return false;
        if (auto p = std::get_if<bool>(&data)) return *p;
        if (auto p = std::get_if<int64_t>(&data)) return *p != 0;
        if (auto p = std::get_if<double>(&data)) return *p != 0.0;
        if (auto p = std::get_if<std::string>(&data)) return !p->empty();
        if (auto p = std::get_if<ArrayPtr>(&data)) return *p && !(*p)->empty();
        return false;
    }
    std::string str() const {
        if (std::holds_alternative<std::monostate>(data)) return "null";
        if (auto p = std::get_if<bool>(&data)) return *p ? "true" : "false";
        if (auto p = std::get_if<int64_t>(&data)) return std::to_string(*p);
        if (auto p = std::get_if<double>(&data)) { std::ostringstream o; o << std::setprecision(15) << *p; return o.str(); }
        if (auto p = std::get_if<std::string>(&data)) return *p;
        if (auto p = std::get_if<ArrayPtr>(&data)) { std::string out="["; if(*p){for(size_t i=0;i<(*p)->size();++i){if(i)out+=", ";out+=(*p)->at(i).str();}} return out+"]"; }
        return "null";
    }
};

static bool is_num(const Value& v) { return std::holds_alternative<int64_t>(v.data) || std::holds_alternative<double>(v.data); }
static double as_double(const Value& v) { if (auto p=std::get_if<double>(&v.data)) return *p; if (auto p=std::get_if<int64_t>(&v.data)) return (double)*p; throw Error("expected number"); }
static int64_t as_int(const Value& v) { if (auto p=std::get_if<int64_t>(&v.data)) return *p; if (auto p=std::get_if<double>(&v.data)) return (int64_t)*p; throw Error("expected integer"); }
static Value addv(const Value&a,const Value&b){
    if (std::holds_alternative<std::string>(a.data) || std::holds_alternative<std::string>(b.data)) return a.str()+b.str();
    if (std::holds_alternative<int64_t>(a.data)&&std::holds_alternative<int64_t>(b.data)) {
#ifdef JAU_HAVE_ASM_FASTOPS
        return (int64_t)jau_fast_add_i64(std::get<int64_t>(a.data),std::get<int64_t>(b.data));
#else
        return std::get<int64_t>(a.data)+std::get<int64_t>(b.data);
#endif
    }
    return as_double(a)+as_double(b);
}
static Value subv(const Value&a,const Value&b){
    if (std::holds_alternative<int64_t>(a.data)&&std::holds_alternative<int64_t>(b.data)) {
#ifdef JAU_HAVE_ASM_FASTOPS
        return (int64_t)jau_fast_sub_i64(std::get<int64_t>(a.data),std::get<int64_t>(b.data));
#else
        return std::get<int64_t>(a.data)-std::get<int64_t>(b.data);
#endif
    }
    return as_double(a)-as_double(b);
}
static Value mulv(const Value&a,const Value&b){
    if (std::holds_alternative<int64_t>(a.data)&&std::holds_alternative<int64_t>(b.data)) {
#ifdef JAU_HAVE_ASM_FASTOPS
        return (int64_t)jau_fast_mul_i64(std::get<int64_t>(a.data),std::get<int64_t>(b.data));
#else
        return std::get<int64_t>(a.data)*std::get<int64_t>(b.data);
#endif
    }
    return as_double(a)*as_double(b);
}
static bool eqv(const Value&a,const Value&b){
    if (a.data.index()==b.data.index()) {
        if(std::holds_alternative<std::monostate>(a.data)) return true;
        if(auto p=std::get_if<int64_t>(&a.data))return *p==std::get<int64_t>(b.data);
        if(auto p=std::get_if<double>(&a.data))return *p==std::get<double>(b.data);
        if(auto p=std::get_if<bool>(&a.data))return *p==std::get<bool>(b.data);
        if(auto p=std::get_if<std::string>(&a.data))return *p==std::get<std::string>(b.data);
        if(auto p=std::get_if<ArrayPtr>(&a.data))return p->get()==std::get<ArrayPtr>(b.data).get();
    }
    return is_num(a)&&is_num(b)&&as_double(a)==as_double(b);
}
static Value eval_binary(const std::string&op,const Value&a,const Value&b){
    if(op=="+")return addv(a,b); if(op=="-")return subv(a,b); if(op=="*")return mulv(a,b);
    if(op=="/"){double d=as_double(b);if(d==0)throw Error("division by zero");if(std::holds_alternative<int64_t>(a.data)&&std::holds_alternative<int64_t>(b.data))return as_int(a)/as_int(b);return as_double(a)/d;}
    if(op=="%"){auto d=as_int(b);if(!d)throw Error("modulo by zero");return as_int(a)%d;}
    if(op=="==")return eqv(a,b);if(op=="!=")return !eqv(a,b);if(op=="<")return as_double(a)<as_double(b);if(op=="<=")return as_double(a)<=as_double(b);if(op==">")return as_double(a)>as_double(b);if(op==">=")return as_double(a)>=as_double(b);if(op=="&&")return a.truthy()&&b.truthy();if(op=="||")return a.truthy()||b.truthy();throw Error("bad operator "+op);
}

static std::string read_text(const fs::path&p){std::ifstream f(p,std::ios::binary);if(!f)throw Error("cannot open "+p.string());return std::string((std::istreambuf_iterator<char>(f)),{});}
static void write_text(const fs::path&p,const std::string&s){if(p.has_parent_path())fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);if(!f)throw Error("cannot write "+p.string());f.write(s.data(),(std::streamsize)s.size());}
static std::string replace_all(std::string s,const std::string&a,const std::string&b){size_t p=0;while((p=s.find(a,p))!=std::string::npos){s.replace(p,a.size(),b);p+=b.size();}return s;}
static uint64_t fnv1a64(const std::string&s){uint64_t h=1469598103934665603ULL;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
static std::string shell_quote(const std::string&s){std::string o="'";for(char c:s){if(c=='\'')o+="'\\''";else o+=c;}return o+"'";}
