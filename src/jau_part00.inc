#include "jau/jau.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
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
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace fs = std::filesystem;
namespace jau {

#ifdef JAU_HAVE_ASM_FASTOPS
extern "C" long long jau_fast_add_i64(long long, long long);
extern "C" long long jau_fast_sub_i64(long long, long long);
extern "C" long long jau_fast_mul_i64(long long, long long);
#endif

struct Error : std::runtime_error { using std::runtime_error::runtime_error; };

using ValueData = std::variant<std::monostate, int64_t, double, bool, std::string>;
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
        return false;
    }
    std::string str() const {
        if (std::holds_alternative<std::monostate>(data)) return "null";
        if (auto p = std::get_if<bool>(&data)) return *p ? "true" : "false";
        if (auto p = std::get_if<int64_t>(&data)) return std::to_string(*p);
        if (auto p = std::get_if<double>(&data)) { std::ostringstream o; o << std::setprecision(15) << *p; return o.str(); }
        return std::get<std::string>(data);
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
