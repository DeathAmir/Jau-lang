from pathlib import Path


def replace(path, old, new, count=1):
    p = Path(path)
    s = p.read_text(encoding="utf-8")
    if old not in s:
        raise SystemExit(f"anchor missing in {path}: {old[:120]!r}")
    s = s.replace(old, new, count)
    p.write_text(s, encoding="utf-8")


# ---- parser / lexer -------------------------------------------------------
p = Path("src/jau_part01.inc")
s = p.read_text(encoding="utf-8")

s = s.replace(
'''enum class TK { End, Id, Num, Str, LParen,RParen,LBrace,RBrace,LBracket,RBracket,Comma,Semi,Colon,Dot,Plus,Minus,Star,Slash,Percent,Bang,Eq,Lt,Gt,EqEq,Ne,Le,Ge,AndAnd,OrOr,\n    PlusEq,MinusEq,StarEq,SlashEq,PercentEq,Inc,Dec,Func,Extern,Namespace,If,Else,While,For,Return,Break,Continue,Let,Const,True,False,Null };''',
'''enum class TK { End, Id, Num, Str, LParen,RParen,LBrace,RBrace,LBracket,RBracket,Comma,Semi,Colon,Dot,Plus,Minus,Star,Slash,Percent,Bang,Eq,Lt,Gt,EqEq,Ne,Le,Ge,AndAnd,OrOr,Amp,Pipe,Caret,Tilde,Shl,Shr,\n    PlusEq,MinusEq,StarEq,SlashEq,PercentEq,Inc,Dec,Func,Extern,Namespace,If,Else,While,For,Return,Break,Continue,Let,Const,True,False,Null };''',
1)

old_kw = '''static const std::unordered_map<std::string,TK> kw={{"func",TK::Func},{"fn",TK::Func},{"extern",TK::Extern},{"namespace",TK::Namespace},{"if",TK::If},{"else",TK::Else},{"while",TK::While},{"for",TK::For},{"return",TK::Return},{"break",TK::Break},{"continue",TK::Continue},{"let",TK::Let},{"var",TK::Let},{"const",TK::Const},{"true",TK::True},{"false",TK::False},{"null",TK::Null}};'''
new_kw = '''static const std::unordered_map<std::string,TK> kw={{"func",TK::Func},{"fn",TK::Func},{"function",TK::Func},{"def",TK::Func},{"extern",TK::Extern},{"namespace",TK::Namespace},{"if",TK::If},{"else",TK::Else},{"while",TK::While},{"for",TK::For},{"return",TK::Return},{"break",TK::Break},{"continue",TK::Continue},{"let",TK::Let},{"var",TK::Let},{"const",TK::Const},{"true",TK::True},{"false",TK::False},{"null",TK::Null},{"nil",TK::Null},{"and",TK::AndAnd},{"or",TK::OrOr},{"not",TK::Bang}};'''
if old_kw not in s:
    raise SystemExit("keyword map anchor missing")
s = s.replace(old_kw, new_kw, 1)

old_num = '''if(std::isdigit((unsigned char)c)){std::string x;bool dot=false;while(std::isdigit((unsigned char)peek())||(!dot&&peek()=='.')){if(peek()=='.')dot=true;x+=get();}t.push_back({TK::Num,x,ln});continue;}'''
new_num = '''if(std::isdigit((unsigned char)c)){std::string x;bool dot=false;if(c=='0'&&(peek(1)=='x'||peek(1)=='X')){x+=get();x+=get();while(std::isxdigit((unsigned char)peek())||peek()=='_')x+=get();}else if(c=='0'&&(peek(1)=='b'||peek(1)=='B')){x+=get();x+=get();while(peek()=='0'||peek()=='1'||peek()=='_')x+=get();}else if(c=='0'&&(peek(1)=='o'||peek(1)=='O')){x+=get();x+=get();while((peek()>='0'&&peek()<='7')||peek()=='_')x+=get();}else{while(std::isdigit((unsigned char)peek())||peek()=='_'||(!dot&&peek()=='.')){if(peek()=='.')dot=true;x+=get();}}t.push_back({TK::Num,x,ln});continue;}'''
if old_num not in s:
    raise SystemExit("numeric lexer anchor missing")
s = s.replace(old_num, new_num, 1)

old_two = '''if(two('=','=',TK::EqEq)||two('!','=',TK::Ne)||two('<','=',TK::Le)||two('>','=',TK::Ge)||two('&','&',TK::AndAnd)||two('|','|',TK::OrOr)||two('+','=',TK::PlusEq)||two('-','=',TK::MinusEq)||two('*','=',TK::StarEq)||two('/','=',TK::SlashEq)||two('%','=',TK::PercentEq)||two('+','+',TK::Inc)||two('-','-',TK::Dec))continue;'''
new_two = '''if(two('=','=',TK::EqEq)||two('!','=',TK::Ne)||two('<','=',TK::Le)||two('>','=',TK::Ge)||two('<','<',TK::Shl)||two('>','>',TK::Shr)||two('&','&',TK::AndAnd)||two('|','|',TK::OrOr)||two('+','=',TK::PlusEq)||two('-','=',TK::MinusEq)||two('*','=',TK::StarEq)||two('/','=',TK::SlashEq)||two('%','=',TK::PercentEq)||two('+','+',TK::Inc)||two('-','-',TK::Dec))continue;'''
if old_two not in s:
    raise SystemExit("two-char lexer anchor missing")
s = s.replace(old_two, new_two, 1)

old_switch = '''case '%':k=TK::Percent;break;case '!':k=TK::Bang;break;case '=':k=TK::Eq;break;case '<':k=TK::Lt;break;case '>':k=TK::Gt;break;default:'''
new_switch = '''case '%':k=TK::Percent;break;case '!':k=TK::Bang;break;case '&':k=TK::Amp;break;case '|':k=TK::Pipe;break;case '^':k=TK::Caret;break;case '~':k=TK::Tilde;break;case '=':k=TK::Eq;break;case '<':k=TK::Lt;break;case '>':k=TK::Gt;break;default:'''
if old_switch not in s:
    raise SystemExit("single-char lexer anchor missing")
s = s.replace(old_switch, new_switch, 1)

old_primary = '''if(at(TK::Num)){auto z=cur();++i;auto e=std::make_unique<Expr>();e->k=Expr::Lit;e->v=z.text.find('.')==std::string::npos?Value((int64_t)std::stoll(z.text)):Value(std::stod(z.text));return e;}'''
new_primary = '''if(at(TK::Num)){auto z=cur();++i;auto e=std::make_unique<Expr>();e->k=Expr::Lit;try{std::string raw=z.text;raw.erase(std::remove(raw.begin(),raw.end(),'_'),raw.end());if(raw.find('.')!=std::string::npos)e->v=Value(std::stod(raw));else{int base=10;size_t off=0;if(raw.size()>2&&raw[0]=='0'&&(raw[1]=='x'||raw[1]=='X')){base=16;off=2;}else if(raw.size()>2&&raw[0]=='0'&&(raw[1]=='b'||raw[1]=='B')){base=2;off=2;}else if(raw.size()>2&&raw[0]=='0'&&(raw[1]=='o'||raw[1]=='O')){base=8;off=2;}e->v=Value((int64_t)std::stoll(raw.substr(off),nullptr,base));}}catch(...){throw Error("invalid numeric literal '"+z.text+"' at line "+std::to_string(z.line));}return e;}'''
if old_primary not in s:
    raise SystemExit("numeric parser anchor missing")
s = s.replace(old_primary, new_primary, 1)

old_unary = '''if(at(TK::Bang)||at(TK::Minus)){std::string op=cur().text;++i;auto e=std::make_unique<Expr>();e->k=Expr::Unary;e->text=op;e->a=unary();return e;}return call();}'''
new_unary = '''if(at(TK::Bang)||at(TK::Minus)||at(TK::Tilde)){std::string op=cur().text;++i;auto e=std::make_unique<Expr>();e->k=Expr::Unary;e->text=op;e->a=unary();return e;}return call();}'''
if old_unary not in s:
    raise SystemExit("unary parser anchor missing")
s = s.replace(old_unary, new_unary, 1)

old_prec = '''    std::unique_ptr<Expr> factor(){return bin(&Parser::unary,{TK::Star,TK::Slash,TK::Percent});}\n    std::unique_ptr<Expr> term(){return bin(&Parser::factor,{TK::Plus,TK::Minus});}\n    std::unique_ptr<Expr> cmp(){return bin(&Parser::term,{TK::Lt,TK::Le,TK::Gt,TK::Ge});}\n    std::unique_ptr<Expr> equality(){return bin(&Parser::cmp,{TK::EqEq,TK::Ne});}\n    std::unique_ptr<Expr> land(){return bin(&Parser::equality,{TK::AndAnd});}\n    std::unique_ptr<Expr> lor(){return bin(&Parser::land,{TK::OrOr});}'''
new_prec = '''    std::unique_ptr<Expr> factor(){return bin(&Parser::unary,{TK::Star,TK::Slash,TK::Percent});}\n    std::unique_ptr<Expr> term(){return bin(&Parser::factor,{TK::Plus,TK::Minus});}\n    std::unique_ptr<Expr> shift(){return bin(&Parser::term,{TK::Shl,TK::Shr});}\n    std::unique_ptr<Expr> cmp(){return bin(&Parser::shift,{TK::Lt,TK::Le,TK::Gt,TK::Ge});}\n    std::unique_ptr<Expr> equality(){return bin(&Parser::cmp,{TK::EqEq,TK::Ne});}\n    std::unique_ptr<Expr> band(){return bin(&Parser::equality,{TK::Amp});}\n    std::unique_ptr<Expr> bxor(){return bin(&Parser::band,{TK::Caret});}\n    std::unique_ptr<Expr> bor(){return bin(&Parser::bxor,{TK::Pipe});}\n    std::unique_ptr<Expr> land(){return bin(&Parser::bor,{TK::AndAnd});}\n    std::unique_ptr<Expr> lor(){return bin(&Parser::land,{TK::OrOr});}'''
if old_prec not in s:
    raise SystemExit("precedence anchor missing")
s = s.replace(old_prec, new_prec, 1)

# extern declarations are C-ABI names. A namespace wraps the Jau facade, not the native symbol.
old_func = '''s->external=external;s->name=qualified_id();if(!current_namespace.empty()&&s->name.find('.')==std::string::npos)s->name=current_namespace+"."+s->name;need(TK::LParen,"expected (");'''
new_func = '''s->external=external;s->name=qualified_id();if(!external&&!current_namespace.empty()&&s->name.find('.')==std::string::npos)s->name=current_namespace+"."+s->name;need(TK::LParen,"expected (");'''
if old_func not in s:
    raise SystemExit("namespace extern anchor missing")
s = s.replace(old_func, new_func, 1)

p.write_text(s, encoding="utf-8")


# ---- bytecode / optimizer ------------------------------------------------
p = Path("src/jau_part02a.inc")
s = p.read_text(encoding="utf-8")

s = s.replace(
'''enum class Op:uint8_t { Const,Null,LoadG,StoreG,LoadL,StoreL,Pop,Add,Sub,Mul,Div,Mod,Neg,Not,Eq,Ne,Lt,Le,Gt,Ge,And,Or,Jump,JumpFalse,MakeArray,Index,Call,Ret,Halt };''',
'''enum class Op:uint8_t { Const,Null,LoadG,StoreG,LoadL,StoreL,Pop,Add,Sub,Mul,Div,Mod,Neg,Not,BitNot,Eq,Ne,Lt,Le,Gt,Ge,And,Or,BitAnd,BitOr,BitXor,Shl,Shr,Jump,JumpFalse,MakeArray,Index,Call,Ret,Halt };''',
1)

old_fold = '''    if(op=="%") return as_int(a)%as_int(b);\n    if(op=="==") return a.str()==b.str();'''
new_fold = '''    if(op=="%") return as_int(a)%as_int(b);\n    if(op=="&") return as_int(a)&as_int(b);\n    if(op=="|") return as_int(a)|as_int(b);\n    if(op=="^") return as_int(a)^as_int(b);\n    if(op=="<<") return as_int(a)<<as_int(b);\n    if(op==">>") return as_int(a)>>as_int(b);\n    if(op=="==") return a.str()==b.str();'''
if old_fold not in s:
    raise SystemExit("constant fold anchor missing")
s = s.replace(old_fold, new_fold, 1)

old_opt_unary = '''if(e->k==Expr::Unary&&e->a&&e->a->k==Expr::Lit){try{if(e->text=="-"){if(std::holds_alternative<int64_t>(e->a->v.data))become_lit(e,Value(-std::get<int64_t>(e->a->v.data)));else become_lit(e,Value(-as_double(e->a->v)));}else become_lit(e,Value(!e->a->v.truthy()));}catch(...){}return;}'''
new_opt_unary = '''if(e->k==Expr::Unary&&e->a&&e->a->k==Expr::Lit){try{if(e->text=="-"){if(std::holds_alternative<int64_t>(e->a->v.data))become_lit(e,Value(-std::get<int64_t>(e->a->v.data)));else become_lit(e,Value(-as_double(e->a->v)));}else if(e->text=="~")become_lit(e,Value((int64_t)~as_int(e->a->v)));else become_lit(e,Value(!e->a->v.truthy()));}catch(...){}return;}'''
if old_opt_unary not in s:
    raise SystemExit("optimizer unary anchor missing")
s = s.replace(old_opt_unary, new_opt_unary, 1)

# Namespace-aware bytecode function calls.
s = s.replace(
'''Program p; std::unordered_map<std::string,int> fidx,gidx; std::unordered_set<std::string> externs; Fun* f=nullptr; bool top=true; std::unordered_map<std::string,int> locals; int nextlocal=0;''',
'''Program p; std::unordered_map<std::string,int> fidx,gidx; std::unordered_set<std::string> externs; Fun* f=nullptr; bool top=true; std::string current_ns; std::unordered_map<std::string,int> locals; int nextlocal=0;''',
1)

p.write_text(s, encoding="utf-8")


# ---- bytecode emitter + VM -----------------------------------------------
p = Path("src/jau_part02b.inc")
s = p.read_text(encoding="utf-8")

old_unary_bc = '''case Expr::Unary:ex(e->a.get());emit(e->text=="-"?Op::Neg:Op::Not);break;'''
new_unary_bc = '''case Expr::Unary:ex(e->a.get());emit(e->text=="-"?Op::Neg:e->text=="~"?Op::BitNot:Op::Not);break;'''
if old_unary_bc not in s:
    raise SystemExit("BC unary anchor missing")
s = s.replace(old_unary_bc, new_unary_bc, 1)

old_bin_map = '''{"&&",Op::And},{"||",Op::Or}};emit(m.at(e->text));}break;'''
new_bin_map = '''{"&&",Op::And},{"||",Op::Or},{"&",Op::BitAnd},{"|",Op::BitOr},{"^",Op::BitXor},{"<<",Op::Shl},{">>",Op::Shr}};emit(m.at(e->text));}break;'''
if old_bin_map not in s:
    raise SystemExit("BC binary map anchor missing")
s = s.replace(old_bin_map, new_bin_map, 1)

old_call = '''else{auto it=fidx.find(n);if(it==fidx.end()){if(externs.count(n))throw Error("extern function is AOT/object-only: "+n);throw Error("unknown function: "+n);}emit(Op::Call,it->second);}break;}'''
new_call = '''else{auto it=fidx.find(n);if(it==fidx.end()&&n.find('.')==std::string::npos&&!current_ns.empty())it=fidx.find(current_ns+"."+n);if(it==fidx.end()){if(externs.count(n))throw Error("extern function is AOT/object-only: "+n);throw Error("unknown function: "+n);}emit(Op::Call,it->second);}break;}'''
if old_call not in s:
    raise SystemExit("BC function resolution anchor missing")
s = s.replace(old_call, new_call, 1)

old_compile_func = '''for(auto&s:ast)if(s->k==Stmt::FuncS&&!s->external){f=&p.funcs[fidx[s->name]];top=false;locals.clear();const_locals.clear();nextlocal=0;for(auto&n:s->params)locals[n]=nextlocal++;for(auto&x:s->body)st(x.get());emit(Op::Null);emit(Op::Ret);f->locals=nextlocal;}'''
new_compile_func = '''for(auto&s:ast)if(s->k==Stmt::FuncS&&!s->external){f=&p.funcs[fidx[s->name]];top=false;locals.clear();const_locals.clear();nextlocal=0;auto dot=s->name.rfind('.');current_ns=dot==std::string::npos?std::string():s->name.substr(0,dot);for(auto&n:s->params)locals[n]=nextlocal++;for(auto&x:s->body)st(x.get());emit(Op::Null);emit(Op::Ret);f->locals=nextlocal;}'''
if old_compile_func not in s:
    raise SystemExit("BC compile namespace anchor missing")
s = s.replace(old_compile_func, new_compile_func, 1)

old_vm_unary = '''case Op::Neg:{auto a=pop();stack.push_back(std::holds_alternative<int64_t>(a.data)?Value(-std::get<int64_t>(a.data)):Value(-as_double(a)));break;}case Op::Not:{auto a=pop();stack.push_back(!a.truthy());break;}'''
new_vm_unary = '''case Op::Neg:{auto a=pop();stack.push_back(std::holds_alternative<int64_t>(a.data)?Value(-std::get<int64_t>(a.data)):Value(-as_double(a)));break;}case Op::Not:{auto a=pop();stack.push_back(!a.truthy());break;}case Op::BitNot:{auto a=pop();stack.push_back((int64_t)~as_int(a));break;}'''
if old_vm_unary not in s:
    raise SystemExit("VM unary anchor missing")
s = s.replace(old_vm_unary, new_vm_unary, 1)

old_vm_logic = '''case Op::Ge:{auto b=pop(),a=pop();stack.push_back(as_double(a)>=as_double(b));break;}case Op::And:{auto b=pop(),a=pop();stack.push_back(a.truthy()&&b.truthy());break;}case Op::Or:{auto b=pop(),a=pop();stack.push_back(a.truthy()||b.truthy());break;}'''
new_vm_logic = '''case Op::Ge:{auto b=pop(),a=pop();stack.push_back(as_double(a)>=as_double(b));break;}case Op::And:{auto b=pop(),a=pop();stack.push_back(a.truthy()&&b.truthy());break;}case Op::Or:{auto b=pop(),a=pop();stack.push_back(a.truthy()||b.truthy());break;}case Op::BitAnd:{auto b=pop(),a=pop();stack.push_back((int64_t)(as_int(a)&as_int(b)));break;}case Op::BitOr:{auto b=pop(),a=pop();stack.push_back((int64_t)(as_int(a)|as_int(b)));break;}case Op::BitXor:{auto b=pop(),a=pop();stack.push_back((int64_t)(as_int(a)^as_int(b)));break;}case Op::Shl:{auto b=pop(),a=pop();stack.push_back((int64_t)(as_int(a)<<as_int(b)));break;}case Op::Shr:{auto b=pop(),a=pop();stack.push_back((int64_t)(as_int(a)>>as_int(b)));break;}'''
if old_vm_logic not in s:
    raise SystemExit("VM bitwise anchor missing")
s = s.replace(old_vm_logic, new_vm_logic, 1)

p.write_text(s, encoding="utf-8")


# ---- AOT -----------------------------------------------------------------
p = Path("src/jau_part03.inc")
s = p.read_text(encoding="utf-8")

s = s.replace(
'''struct Ctx{std::unordered_map<std::string,int> locals,params;int next=0;std::string end;std::vector<std::string> breaks,continues;}; Ctx c;''',
'''struct Ctx{std::unordered_map<std::string,int> locals,params;int next=0;std::string end,ns;std::vector<std::string> breaks,continues;}; Ctx c;''',
1)

old_foreign_end = '''        return match.empty()?clean(n):match;\n    }\n    int slot'''
new_foreign_end = '''        return match.empty()?clean(n):match;\n    }\n    bool has_native_symbol(const std::string&n)const{std::string wanted=canonical_foreign(clean(n));for(auto&actual:native_symbols)if(canonical_foreign(actual)==wanted)return true;return false;}\n    int slot'''
if old_foreign_end not in s:
    raise SystemExit("AOT native symbol helper anchor missing")
s = s.replace(old_foreign_end, new_foreign_end, 1)

old_aot_unary = '''if(e->text=="-")o<<"  neg "<<(is64?"rax":"eax")<<"\\n";\n            else o<<"  cmp "<<(is64?"rax":"eax")<<",0\\n  sete al\\n  movzx eax,al\\n";'''
new_aot_unary = '''if(e->text=="-")o<<"  neg "<<(is64?"rax":"eax")<<"\\n";\n            else if(e->text=="~")o<<"  not "<<(is64?"rax":"eax")<<"\\n";\n            else o<<"  cmp "<<(is64?"rax":"eax")<<",0\\n  sete al\\n  movzx eax,al\\n";'''
if old_aot_unary not in s:
    raise SystemExit("AOT unary anchor missing")
s = s.replace(old_aot_unary, new_aot_unary, 1)

old_aot_math = '''else if(op=="*\")o<<"  imul "<<(is64?"rax, rcx":"eax, ecx")<<"\\n";\n            else if(op=="/"||op=="%")'''
new_aot_math = '''else if(op=="*\")o<<"  imul "<<(is64?"rax, rcx":"eax, ecx")<<"\\n";\n            else if(op=="&")o<<"  and "<<(is64?"rax, rcx":"eax, ecx")<<"\\n";\n            else if(op=="|")o<<"  or "<<(is64?"rax, rcx":"eax, ecx")<<"\\n";\n            else if(op=="^")o<<"  xor "<<(is64?"rax, rcx":"eax, ecx")<<"\\n";\n            else if(op=="<<")o<<"  shl "<<(is64?"rax, cl":"eax, cl")<<"\\n";\n            else if(op==">>")o<<"  sar "<<(is64?"rax, cl":"eax, cl")<<"\\n";\n            else if(op=="/"||op=="%")'''
if old_aot_math not in s:
    raise SystemExit("AOT binary anchor missing")
s = s.replace(old_aot_math, new_aot_math, 1)

old_aot_call = '''            auto it=funcs.find(n);\n            if(it!=funcs.end()){emit_named_call(n,e->args,false);return;}\n            if(externs.count(n)){emit_named_call(n,e->args,true);return;}\n            throw Error("AOT unknown function: "+n);'''
new_aot_call = '''            auto it=funcs.find(n);\n            if(it==funcs.end()&&n.find('.')==std::string::npos&&!c.ns.empty()){std::string q=c.ns+"."+n;it=funcs.find(q);if(it!=funcs.end()){emit_named_call(q,e->args,false);return;}}\n            if(it!=funcs.end()){emit_named_call(n,e->args,false);return;}\n            if(externs.count(n)||has_native_symbol(n)){emit_named_call(n,e->args,true);return;}\n            throw Error("AOT unknown function: "+n+" (not a Jau function, extern declaration, or discovered native object symbol)");'''
if old_aot_call not in s:
    raise SystemExit("AOT call resolution anchor missing")
s = s.replace(old_aot_call, new_aot_call, 1)

old_emit_func = '''    void emit_func(Stmt*s){\n        c=Ctx{};c.end=L();'''
new_emit_func = '''    void emit_func(Stmt*s){\n        c=Ctx{};c.end=L();auto dot=s->name.rfind('.');c.ns=dot==std::string::npos?std::string():s->name.substr(0,dot);'''
if old_emit_func not in s:
    raise SystemExit("AOT namespace context anchor missing")
s = s.replace(old_emit_func, new_emit_func, 1)

s = s.replace('std::string version(){return "0.8.0";}', 'std::string version(){return "0.8.1";}', 1)
p.write_text(s, encoding="utf-8")


# Project version.
p = Path("CMakeLists.txt")
s = p.read_text(encoding="utf-8")
if 'project(Jau VERSION 0.8.0 LANGUAGES CXX)' not in s:
    raise SystemExit("CMake version anchor missing")
s = s.replace('project(Jau VERSION 0.8.0 LANGUAGES CXX)', 'project(Jau VERSION 0.8.1 LANGUAGES CXX)', 1)
p.write_text(s, encoding="utf-8")

# Regression fixtures.
Path("tests/v081_syntax.jau").write_text('''def main() {\n    var a = 0x10;\n    var b = 0b1010;\n    var c = 1_000;\n    var x = (a & 7) | (b ^ 3);\n    var y = (c >> 3) + (1 << 5);\n    if (not false and true or false) { x += 1; }\n    print(x);\n    print(y);\n    print(~0);\n    return 0;\n}\nmain();\n''', encoding='utf-8')

Path("tests/v081_namespace.jau").write_text('''namespace Calc {\n    func helper(a) { return a + 1; }\n    func run(a) { return helper(a); }\n}\nprint(Calc.run[41]);\n''', encoding='utf-8')

Path("tests/native_mathx/src").mkdir(parents=True, exist_ok=True)
Path("tests/native_mathx/native/windows-x86_64").mkdir(parents=True, exist_ok=True)
Path("tests/native_mathx/native/windows-x86").mkdir(parents=True, exist_ok=True)
Path("tests/native_mathx/mathx.cpp").write_text('''extern "C" long long mx_add(long long a,long long b){return a+b;}\nextern "C" long long mx_mul(long long a,long long b){return a*b;}\nextern "C" long long mx_pow(long long base,long long exp){long long r=1;for(long long i=0;i<exp;++i)r*=base;return r;}\n''', encoding='utf-8')
Path("tests/native_mathx/src/main.jau").write_text('''namespace MathX {\n    extern func mx_add(a:int,b:int):int;\n    extern func mx_mul(a:int,b:int):int;\n    extern func mx_pow(base:int,exp:int):int;\n    func add(a,b) { return mx_add(a,b); }\n    func multiply(a,b) { return mx_mul(a,b); }\n    func power(a,b) { return mx_pow(a,b); }\n}\n''', encoding='utf-8')
Path("tests/native_mathx/jau.pkg").write_text('''name="MathX"\nversion="1.0.0"\nmain="src/main.jau"\ntype="native"\nnative_windows_x86_64="native/windows-x86_64/mathx.obj"\nnative_windows_x86="native/windows-x86/mathx.obj"\n''', encoding='utf-8')
Path("tests/native_mathx_consumer.jau").write_text('''import "pkg:MathX"\nprint(MathX.add[20,22]);\nprint(MathX.multiply[6,7]);\nprint(MathX.power[2,10]);\n''', encoding='utf-8')

Path("docs/V081.md").write_text('''# Jau 0.8.1\n\nJau 0.8.1 fixes native calls declared inside namespace facades and expands integer-oriented syntax.\n\n## Native package binding\n\n`extern func` declarations keep their C ABI symbol name even when written inside a Jau namespace. The AOT backend also accepts a call when a matching symbol was discovered directly from a package COFF object. Namespace-local Jau calls resolve relative to the current namespace in both VM and AOT modes.\n\n## Syntax additions\n\n- `function` and `def` aliases for `func`\n- `nil` alias for `null`\n- `and`, `or`, `not` keyword operators\n- bitwise `&`, `|`, `^`, `~`\n- shifts `<<`, `>>`\n- hexadecimal `0x`, binary `0b`, octal `0o` integer literals\n- numeric separators such as `1_000_000`\n\nExisting Jau 0.8 syntax such as `for`, `++`, `--`, compound assignments, `fn`, `var`, and block comments remains supported.\n''', encoding='utf-8')
