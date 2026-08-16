from pathlib import Path


def read(p): return Path(p).read_text(encoding='utf-8')
def write(p,s): Path(p).write_text(s,encoding='utf-8')
def rep(s,a,b,label):
    if a not in s: raise SystemExit('anchor not found: '+label)
    return s.replace(a,b,1)

# -----------------------------------------------------------------------------
# Small self-contained JSON reader. Jau's public Value ABI intentionally remains
# simple; JSON is parsed internally and queried by path so HTTP/API code can use
# objects today without prematurely exposing a half-designed map/object runtime.
# -----------------------------------------------------------------------------
p='src/jau_part01.inc'; s=read(p)
anchor='''static uint64_t fnv1a64(const std::string&s){uint64_t h=1469598103934665603ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}return h;}\n'''
json=r'''static uint64_t fnv1a64(const std::string&s){uint64_t h=1469598103934665603ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}return h;}

struct JsonValue {
    enum Kind { JNull, JBool, JNumber, JString, JArray, JObject } kind=JNull;
    bool boolean=false; double number=0; std::string text;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string,JsonValue>> object;
};
static std::string json_escape_impl(const std::string&s){std::string o;for(unsigned char c:s){switch(c){case '"':o+="\\\"";break;case '\\':o+="\\\\";break;case '\b':o+="\\b";break;case '\f':o+="\\f";break;case '\n':o+="\\n";break;case '\r':o+="\\r";break;case '\t':o+="\\t";break;default:if(c<0x20){const char*h="0123456789abcdef";o+="\\u00";o+=h[c>>4];o+=h[c&15];}else o+=(char)c;}}return o;}
static void json_utf8(std::string&o,uint32_t cp){if(cp<=0x7f)o+=(char)cp;else if(cp<=0x7ff){o+=(char)(0xc0|(cp>>6));o+=(char)(0x80|(cp&63));}else if(cp<=0xffff){o+=(char)(0xe0|(cp>>12));o+=(char)(0x80|((cp>>6)&63));o+=(char)(0x80|(cp&63));}else{o+=(char)(0xf0|(cp>>18));o+=(char)(0x80|((cp>>12)&63));o+=(char)(0x80|((cp>>6)&63));o+=(char)(0x80|(cp&63));}}
class JsonParser {
    const std::string&s;size_t p=0;
    void ws(){while(p<s.size()&&std::isspace((unsigned char)s[p]))++p;}
    [[noreturn]] void bad(const std::string&m)const{throw Error("JSON "+m+" at byte "+std::to_string(p));}
    uint32_t hex4(){if(p+4>s.size())bad("truncated unicode escape");uint32_t v=0;for(int i=0;i<4;++i){char c=s[p++];v<<=4;if(c>='0'&&c<='9')v+=c-'0';else if(c>='a'&&c<='f')v+=10+c-'a';else if(c>='A'&&c<='F')v+=10+c-'A';else bad("invalid unicode escape");}return v;}
    std::string string(){if(p>=s.size()||s[p++]!='"')bad("expected string");std::string o;while(p<s.size()){unsigned char c=(unsigned char)s[p++];if(c=='"')return o;if(c<0x20)bad("control character in string");if(c!='\\'){o+=(char)c;continue;}if(p>=s.size())bad("truncated escape");char e=s[p++];switch(e){case '"':o+='"';break;case '\\':o+='\\';break;case '/':o+='/';break;case 'b':o+='\b';break;case 'f':o+='\f';break;case 'n':o+='\n';break;case 'r':o+='\r';break;case 't':o+='\t';break;case 'u':{uint32_t cp=hex4();if(cp>=0xd800&&cp<=0xdbff){if(p+2>s.size()||s[p]!='\\'||s[p+1]!='u')bad("missing low surrogate");p+=2;uint32_t lo=hex4();if(lo<0xdc00||lo>0xdfff)bad("invalid low surrogate");cp=0x10000+((cp-0xd800)<<10)+(lo-0xdc00);}else if(cp>=0xdc00&&cp<=0xdfff)bad("unexpected low surrogate");json_utf8(o,cp);break;}default:bad("invalid escape");}}bad("unterminated string");}
    JsonValue value(){ws();if(p>=s.size())bad("expected value");if(s[p]=='"'){JsonValue v;v.kind=JsonValue::JString;v.text=string();return v;}if(s[p]=='{'){++p;JsonValue v;v.kind=JsonValue::JObject;ws();if(p<s.size()&&s[p]=='}'){++p;return v;}for(;;){ws();auto k=string();ws();if(p>=s.size()||s[p++]!=':')bad("expected :");v.object.push_back({k,value()});ws();if(p<s.size()&&s[p]=='}'){++p;return v;}if(p>=s.size()||s[p++]!=',')bad("expected , or }");}}
        if(s[p]=='['){++p;JsonValue v;v.kind=JsonValue::JArray;ws();if(p<s.size()&&s[p]==']'){++p;return v;}for(;;){v.array.push_back(value());ws();if(p<s.size()&&s[p]==']'){++p;return v;}if(p>=s.size()||s[p++]!=',')bad("expected , or ]");}}
        if(s.compare(p,4,"true")==0){p+=4;JsonValue v;v.kind=JsonValue::JBool;v.boolean=true;return v;}if(s.compare(p,5,"false")==0){p+=5;JsonValue v;v.kind=JsonValue::JBool;return v;}if(s.compare(p,4,"null")==0){p+=4;return JsonValue();}
        size_t a=p;if(s[p]=='-')++p;if(p>=s.size())bad("bad number");if(s[p]=='0')++p;else{if(!std::isdigit((unsigned char)s[p]))bad("bad number");while(p<s.size()&&std::isdigit((unsigned char)s[p]))++p;}if(p<s.size()&&s[p]=='.'){++p;if(p>=s.size()||!std::isdigit((unsigned char)s[p]))bad("bad fraction");while(p<s.size()&&std::isdigit((unsigned char)s[p]))++p;}if(p<s.size()&&(s[p]=='e'||s[p]=='E')){++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;if(p>=s.size()||!std::isdigit((unsigned char)s[p]))bad("bad exponent");while(p<s.size()&&std::isdigit((unsigned char)s[p]))++p;}JsonValue v;v.kind=JsonValue::JNumber;try{v.number=std::stod(s.substr(a,p-a));}catch(...){bad("bad number");}return v;}
public:explicit JsonParser(const std::string&x):s(x){}JsonValue parse(){auto v=value();ws();if(p!=s.size())bad("trailing data");return v;}
};
static std::string json_dump(const JsonValue&v){switch(v.kind){case JsonValue::JNull:return "null";case JsonValue::JBool:return v.boolean?"true":"false";case JsonValue::JNumber:{std::ostringstream o;o<<std::setprecision(15)<<v.number;return o.str();}case JsonValue::JString:return "\""+json_escape_impl(v.text)+"\"";case JsonValue::JArray:{std::string o="[";for(size_t i=0;i<v.array.size();++i){if(i)o+=",";o+=json_dump(v.array[i]);}return o+"]";}case JsonValue::JObject:{std::string o="{";for(size_t i=0;i<v.object.size();++i){if(i)o+=",";o+="\""+json_escape_impl(v.object[i].first)+"\":"+json_dump(v.object[i].second);}return o+"}";}}return "null";}
static const JsonValue* json_path(const JsonValue&root,const std::string&path){const JsonValue*cur=&root;size_t p=0;if(path.empty())return cur;while(p<path.size()){if(path[p]=='.'){++p;continue;}if(path[p]=='['){size_t e=path.find(']',p+1);if(e==std::string::npos)return nullptr;long long idx=-1;try{idx=std::stoll(path.substr(p+1,e-p-1));}catch(...){return nullptr;}if(cur->kind!=JsonValue::JArray||idx<0||(size_t)idx>=cur->array.size())return nullptr;cur=&cur->array[(size_t)idx];p=e+1;continue;}size_t e=p;while(e<path.size()&&path[e]!='.'&&path[e]!='[')++e;std::string key=path.substr(p,e-p);if(cur->kind!=JsonValue::JObject)return nullptr;const JsonValue*next=nullptr;for(auto&kv:cur->object)if(kv.first==key){next=&kv.second;break;}if(!next)return nullptr;cur=next;p=e;}return cur;}
static std::string json_scalar_or_dump(const JsonValue&v){if(v.kind==JsonValue::JString)return v.text;return json_dump(v);}
'''
s=rep(s,anchor,json,'JSON runtime')

# AST supports an indexed mutation node. Compound index assignment is rejected
# for now rather than being silently rewritten with duplicate side effects.
s=rep(s,
'''struct Expr { enum K{Lit,Var,Assign,Binary,Unary,Call,ArrayLit,Index} k;''',
'''struct Expr { enum K{Lit,Var,Assign,IndexAssign,Binary,Unary,Call,ArrayLit,Index} k;''','IndexAssign AST')
old='''    std::unique_ptr<Expr> assign(){auto e=lor();if(at(TK::Eq)||at(TK::PlusEq)||at(TK::MinusEq)||at(TK::StarEq)||at(TK::SlashEq)||at(TK::PercentEq)){TK op=cur().k;++i;if(e->k!=Expr::Var)throw Error("assignment target must be a variable at line "+std::to_string(cur().line));std::string n=e->text;auto rhs=assign();auto a=std::make_unique<Expr>();a->k=Expr::Assign;a->text=n;if(op==TK::Eq)a->a=std::move(rhs);else{auto lhs=std::make_unique<Expr>();lhs->k=Expr::Var;lhs->text=n;auto b=std::make_unique<Expr>();b->k=Expr::Binary;b->text=op==TK::PlusEq?"+":op==TK::MinusEq?"-":op==TK::StarEq?"*":op==TK::SlashEq?"/":"%";b->a=std::move(lhs);b->b=std::move(rhs);a->a=std::move(b);}return a;}return e;}\n'''
new='''    std::unique_ptr<Expr> assign(){auto e=lor();if(at(TK::Eq)||at(TK::PlusEq)||at(TK::MinusEq)||at(TK::StarEq)||at(TK::SlashEq)||at(TK::PercentEq)){TK op=cur().k;++i;auto rhs=assign();if(e->k==Expr::Index){if(op!=TK::Eq)throw Error("compound indexed assignment is not supported yet; use a[i] = a[i] op value");auto a=std::make_unique<Expr>();a->k=Expr::IndexAssign;a->a=std::move(e->a);a->b=std::move(e->b);a->args.push_back(std::move(rhs));return a;}if(e->k!=Expr::Var)throw Error("assignment target must be a variable or array index at line "+std::to_string(cur().line));std::string n=e->text;auto a=std::make_unique<Expr>();a->k=Expr::Assign;a->text=n;if(op==TK::Eq)a->a=std::move(rhs);else{auto lhs=std::make_unique<Expr>();lhs->k=Expr::Var;lhs->text=n;auto b=std::make_unique<Expr>();b->k=Expr::Binary;b->text=op==TK::PlusEq?"+":op==TK::MinusEq?"-":op==TK::StarEq?"*":op==TK::SlashEq?"/":"%";b->a=std::move(lhs);b->b=std::move(rhs);a->a=std::move(b);}return a;}return e;}\n'''
s=rep(s,old,new,'indexed assignment parser')
write(p,s)

# Bytecode append-only opcode preserves existing numeric bytecode values.
p='src/jau_part02a.inc'; s=read(p)
s=rep(s,
'''Jump,JumpFalse,MakeArray,Index,Call,Ret,Halt };''',
'''Jump,JumpFalse,MakeArray,Index,Call,Ret,Halt,SetIndex };''','SetIndex opcode')
write(p,s)

p='src/jau_part02b.inc'; s=read(p)
s=rep(s,
'''        case Expr::Assign:{if((!top&&const_locals.count(e->text))||(top&&const_globals.count(e->text)))throw Error("cannot assign to const: "+e->text);ex(e->a.get());auto it=locals.find(e->text);if(!top&&it!=locals.end())emit(Op::StoreL,it->second);else emit(Op::StoreG,g(e->text));break;}\n''',
'''        case Expr::Assign:{if((!top&&const_locals.count(e->text))||(top&&const_globals.count(e->text)))throw Error("cannot assign to const: "+e->text);ex(e->a.get());auto it=locals.find(e->text);if(!top&&it!=locals.end())emit(Op::StoreL,it->second);else emit(Op::StoreG,g(e->text));break;}\n        case Expr::IndexAssign:{ex(e->a.get());ex(e->b.get());ex(e->args.at(0).get());emit(Op::SetIndex);break;}\n''','compile indexed assignment')
s=rep(s,
'''{"clock_ns",-47},{"time.now_ns",-47}};''',
'''{"clock_ns",-47},{"time.now_ns",-47},{"json_valid",-48},{"json_get",-49},{"json_string",-50},{"json_int",-51},{"json_bool",-52},{"json_escape",-53},{"json_type",-54},{"json_has",-55}};''','JSON builtin map')
s=rep(s,
'''        if(id==-47){auto n=std::chrono::steady_clock::now().time_since_epoch();return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(n).count();}\n        throw Error("bad builtin");''',
'''        if(id==-47){auto n=std::chrono::steady_clock::now().time_since_epoch();return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(n).count();}\n        if(id==-48){auto text=pop().str();try{JsonParser(text).parse();return true;}catch(...){return false;}}\n        if(id==-49){auto path=pop().str(),text=pop().str();auto root=JsonParser(text).parse();auto v=json_path(root,path);if(!v)throw Error("JSON path not found: "+path);return json_scalar_or_dump(*v);}\n        if(id==-50){auto path=pop().str(),text=pop().str();auto root=JsonParser(text).parse();auto v=json_path(root,path);if(!v)throw Error("JSON path not found: "+path);if(v->kind!=JsonValue::JString)throw Error("JSON value at "+path+" is not a string");return v->text;}\n        if(id==-51){auto path=pop().str(),text=pop().str();auto root=JsonParser(text).parse();auto v=json_path(root,path);if(!v)throw Error("JSON path not found: "+path);if(v->kind!=JsonValue::JNumber)throw Error("JSON value at "+path+" is not a number");return (int64_t)v->number;}\n        if(id==-52){auto path=pop().str(),text=pop().str();auto root=JsonParser(text).parse();auto v=json_path(root,path);if(!v)throw Error("JSON path not found: "+path);if(v->kind!=JsonValue::JBool)throw Error("JSON value at "+path+" is not a boolean");return v->boolean;}\n        if(id==-53)return json_escape_impl(pop().str());\n        if(id==-54){auto path=pop().str(),text=pop().str();auto root=JsonParser(text).parse();auto v=json_path(root,path);if(!v)return std::string("missing");static const char*names[]={"null","bool","number","string","array","object"};return std::string(names[(int)v->kind]);}\n        if(id==-55){auto path=pop().str(),text=pop().str();auto root=JsonParser(text).parse();return json_path(root,path)!=nullptr;}\n        throw Error("bad builtin");''','JSON VM builtins')
s=rep(s,
'''        case Op::Index:{auto idx=as_int(pop());auto arr=pop();auto a=std::get_if<ArrayPtr>(&arr.data);if(!a||!*a)throw Error("indexing expects array");if(idx<0||idx>=(int64_t)(*a)->size())throw Error("array index out of range");stack.push_back((*a)->at((size_t)idx));break;}\n''',
'''        case Op::Index:{auto idx=as_int(pop());auto arr=pop();auto a=std::get_if<ArrayPtr>(&arr.data);if(!a||!*a)throw Error("indexing expects array");if(idx<0||idx>=(int64_t)(*a)->size())throw Error("array index out of range");stack.push_back((*a)->at((size_t)idx));break;}\n        case Op::SetIndex:{auto value=pop();auto idx=as_int(pop());auto arr=pop();auto a=std::get_if<ArrayPtr>(&arr.data);if(!a||!*a)throw Error("indexed assignment expects array");if(idx<0||idx>=(int64_t)(*a)->size())throw Error("array index out of range");(*a)->at((size_t)idx)=value;stack.push_back(value);break;}\n''','VM indexed assignment')
write(p,s)

# Native AOT deliberately rejects VM container mutation with a clear stage error.
p='src/jau_part03.inc'; s=read(p)
s=rep(s,
'''        if(e->k==Expr::Assign){\n''',
'''        if(e->k==Expr::Index||e->k==Expr::IndexAssign)throw Error("array indexing and indexed assignment are VM/bytecode features; native array ABI is not implemented yet");\n        if(e->k==Expr::Assign){\n''','AOT array diagnostic')
write(p,s)

Path('tests/v090_json_index.jau').write_text(r'''func main() {
    let a = [10, 20, 30];
    a[1] = 42;
    print(a[1]);

    let data = "{\"name\":\"Jau\",\"version\":9,\"ok\":true,\"user\":{\"name\":\"DeathAmir\"},\"items\":[3,7]}";
    print(json_valid(data));
    print(json_string(data, "name"));
    print(json_int(data, "version"));
    print(json_bool(data, "ok"));
    print(json_string(data, "user.name"));
    print(json_get(data, "items"));
    print(json_get(data, "items[1]"));
    print(json_type(data, "items"));
    print(json_has(data, "missing"));
    print(json_escape("a\nb\"c"));
}
''',encoding='utf-8')
print('Jau 0.9 language/JSON patch applied')
