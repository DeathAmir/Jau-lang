    }}return Value();}
};

static std::vector<std::unique_ptr<Stmt>> parse_file(const std::string&input,const CompileOptions&o){Lexer l(expand_imports(input,o));Parser p(l.scan());return p.parse();}
static Program compile_program(const std::string&input,const CompileOptions&o){auto ast=parse_file(input,o);BC b;return b.compile(ast,o.optimize);}

class AOT {
    std::string target; bool is64=false, win=false; int label=0; std::ostringstream o; std::unordered_map<std::string,Stmt*> funcs;
    struct Ctx{std::unordered_map<std::string,int> locals,params;int next=0;std::string end;std::vector<std::string> breaks,continues;}; Ctx c;
    std::string L(){return ".L"+std::to_string(label++);} int word()const{return is64?8:4;}
    std::string sym(const std::string&n)const{std::string r="jau_fn_";for(unsigned char c:n)r+=(std::isalnum(c)||c=='_')?(char)c:'_';return r;}
    int slot(const std::string&n){auto it=c.locals.find(n);if(it!=c.locals.end())return it->second;int q=(++c.next)*word();c.locals[n]=q;return q;}
    void push(const char*r){o<<"  push "<<r<<"\n";} void pop(const char*r){o<<"  pop "<<r<<"\n";}
    void load_var(const std::string&n){auto p=c.params.find(n);if(p!=c.params.end())o<<"  mov "<<(is64?"rax, QWORD PTR [rbp+":"eax, DWORD PTR [ebp+")<<p->second<<"]\n";else{o<<"  mov "<<(is64?"rax, QWORD PTR [rbp-":"eax, DWORD PTR [ebp-")<<slot(n)<<"]\n";}push(is64?"rax":"eax");}
    void ex(Expr*e){
        if(!e){o<<"  xor eax,eax\n";push(is64?"rax":"eax");return;}
        if(e->k==Expr::Lit){if(auto p=std::get_if<int64_t>(&e->v.data)){o<<"  mov "<<(is64?"rax":"eax")<<", "<<*p<<"\n";push(is64?"rax":"eax");return;}if(auto p=std::get_if<bool>(&e->v.data)){o<<"  mov eax, "<<(*p?1:0)<<"\n";push(is64?"rax":"eax");return;}throw Error("AOT supports integer/bool values; strings remain VM-only");}
        if(e->k==Expr::Var){load_var(e->text);return;}
        if(e->k==Expr::Assign){ex(e->a.get());pop(is64?"rax":"eax");if(c.params.count(e->text))o<<"  mov "<<(is64?"QWORD PTR [rbp+":"DWORD PTR [ebp+")<<c.params[e->text]<<"], "<<(is64?"rax":"eax")<<"\n";else o<<"  mov "<<(is64?"QWORD PTR [rbp-":"DWORD PTR [ebp-")<<slot(e->text)<<"], "<<(is64?"rax":"eax")<<"\n";push(is64?"rax":"eax");return;}
        if(e->k==Expr::Unary){ex(e->a.get());pop(is64?"rax":"eax");if(e->text=="-")o<<"  neg "<<(is64?"rax":"eax")<<"\n";else{o<<"  cmp "<<(is64?"rax":"eax")<<",0\n  sete al\n  movzx eax,al\n";}push(is64?"rax":"eax");return;}
        if(e->k==Expr::Binary){ex(e->a.get());ex(e->b.get());pop(is64?"rcx":"ecx");pop(is64?"rax":"eax");std::string op=e->text;if(op=="+")o<<"  add "<<(is64?"rax, rcx":"eax, ecx")<<"\n";else if(op=="-")o<<"  sub "<<(is64?"rax, rcx":"eax, ecx")<<"\n";else if(op=="*")o<<"  imul "<<(is64?"rax, rcx":"eax, ecx")<<"\n";else if(op=="/"||op=="%"){if(is64)o<<"  cqo\n  idiv rcx\n";else o<<"  cdq\n  idiv ecx\n";if(op=="%")o<<"  mov "<<(is64?"rax, rdx":"eax, edx")<<"\n";}else if(op=="&&"){o<<"  test "<<(is64?"rax, rax":"eax, eax")<<"\n  setne al\n  movzx eax,al\n  test "<<(is64?"rcx, rcx":"ecx, ecx")<<"\n  setne cl\n  and al,cl\n  movzx eax,al\n";}else if(op=="||"){o<<"  or "<<(is64?"rax, rcx":"eax, ecx")<<"\n  setne al\n  movzx eax,al\n";}else{std::string cc=op=="=="?"e":op=="!="?"ne":op=="<"?"l":op=="<="?"le":op==">"?"g":"ge";o<<"  cmp "<<(is64?"rax, rcx":"eax, ecx")<<"\n  set"<<cc<<" al\n  movzx eax, al\n";}push(is64?"rax":"eax");return;}
        if(e->k==Expr::Call&&e->a&&e->a->k==Expr::Var){std::string n=e->a->text;if(n=="print"&&e->args.size()==1){ex(e->args[0].get());pop(is64?"rax":"eax");if(is64){if(win)o<<"  mov rdx, rax\n  lea rcx, [rip+fmt_i]\n  sub rsp, 40\n  call printf\n  add rsp, 40\n";else o<<"  mov rsi, rax\n  lea rdi, [rip+fmt_i]\n  xor eax,eax\n  call printf@PLT\n";}else{o<<"  push eax\n  push OFFSET FLAT:fmt_i\n  call printf\n  add esp, 8\n";}o<<"  xor eax,eax\n";push(is64?"rax":"eax");return;}auto it=funcs.find(n);if(it==funcs.end())throw Error("AOT unknown function: "+n);for(auto q=e->args.rbegin();q!=e->args.rend();++q)ex(q->get());o<<"  call "<<sym(n)<<"\n";if(!e->args.empty())o<<"  add "<<(is64?"rsp":"esp")<<", "<<(e->args.size()*word())<<"\n";push(is64?"rax":"eax");return;}
        throw Error("AOT unsupported expression");
    }
    void st(Stmt*s){
        if(s->k==Stmt::ExprS){ex(s->expr.get());o<<"  add "<<(is64?"rsp,8":"esp,4")<<"\n";}
        else if(s->k==Stmt::VarS){ex(s->expr.get());pop(is64?"rax":"eax");o<<"  mov "<<(is64?"QWORD PTR [rbp-":"DWORD PTR [ebp-")<<slot(s->name)<<"], "<<(is64?"rax":"eax")<<"\n";}
        else if(s->k==Stmt::IfS){std::string el=L(),en=L();ex(s->cond.get());pop(is64?"rax":"eax");o<<"  cmp "<<(is64?"rax":"eax")<<",0\n  je "<<el<<"\n";for(auto&x:s->body)st(x.get());o<<"  jmp "<<en<<"\n"<<el<<":\n";for(auto&x:s->else_body)st(x.get());o<<en<<":\n";}
        else if(s->k==Stmt::WhileS){std::string a=L(),b=L();c.continues.push_back(a);c.breaks.push_back(b);o<<a<<":\n";ex(s->cond.get());pop(is64?"rax":"eax");o<<"  cmp "<<(is64?"rax":"eax")<<",0\n  je "<<b<<"\n";for(auto&x:s->body)st(x.get());o<<"  jmp "<<a<<"\n"<<b<<":\n";c.continues.pop_back();c.breaks.pop_back();}
        else if(s->k==Stmt::Block){for(auto&x:s->body)st(x.get());}
        else if(s->k==Stmt::ReturnS){if(s->expr){ex(s->expr.get());pop(is64?"rax":"eax");}else o<<"  xor eax,eax\n";o<<"  jmp "<<c.end<<"\n";}
        else if(s->k==Stmt::BreakS){if(c.breaks.empty())throw Error("break outside AOT loop");o<<"  jmp "<<c.breaks.back()<<"\n";}
        else if(s->k==Stmt::ContinueS){if(c.continues.empty())throw Error("continue outside AOT loop");o<<"  jmp "<<c.continues.back()<<"\n";}
    }
    void emit_func(Stmt*s){c=Ctx{};c.end=L();for(size_t i=0;i<s->params.size();++i)c.params[s->params[i]]=2*word()+(int)i*word();o<<sym(s->name)<<":\n"<<(is64?"  push rbp\n  mov rbp,rsp\n  sub rsp, 1024\n":"  push ebp\n  mov ebp,esp\n  sub esp, 512\n");for(auto&x:s->body)st(x.get());o<<"  xor eax,eax\n"<<c.end<<":\n  leave\n  ret\n";}
public:
    AOT(std::string t):target(std::move(t)){win=target.rfind("windows",0)==0;is64=target.find("x86_64")!=std::string::npos;if(target!="linux-x86_64"&&target!="linux-x86"&&target!="windows-x86_64"&&target!="windows-x86")throw Error("unknown target: "+target);}
    std::string gen(std::vector<std::unique_ptr<Stmt>>&ast){optimize_stmts(ast);for(auto&s:ast)if(s->k==Stmt::FuncS)funcs[s->name]=s.get();o<<".intel_syntax noprefix\n.section .rodata\nfmt_i: .asciz \""<<(is64?"%lld":"%d")<<"\\n\"\n.text\n.extern printf\n";for(auto&s:ast)if(s->k==Stmt::FuncS)emit_func(s.get());c=Ctx{};c.end=L();o<<".globl main\nmain:\n"<<(is64?"  push rbp\n  mov rbp,rsp\n  sub rsp, 1024\n":"  push ebp\n  mov ebp,esp\n  sub esp, 512\n");for(auto&s:ast)if(s->k!=Stmt::FuncS)st(s.get());o<<"  xor eax,eax\n"<<c.end<<":\n  leave\n  ret\n";return o.str();}
};
Result compile_file(const std::string&input,const std::string&output,const CompileOptions&o){try{auto p=compile_program(input,o);save(p,output);return {true,"built "+output};}catch(const std::exception&e){return {false,e.what()};}}
Result run_source(const std::string&input,const CompileOptions&o,const std::vector<std::string>&args){try{g_runtime_args=args;VM vm(compile_program(input,o));vm.run();return {true,""};}catch(const std::exception&e){return {false,e.what()};}}
Result run_bytecode(const std::string&input,const std::vector<std::string>&args){try{g_runtime_args=args;VM vm(load(input));vm.run();return {true,""};}catch(const std::exception&e){return {false,e.what()};}}
Result emit_assembly(const std::string&input,const std::string&output,const std::string&target,const CompileOptions&o){try{auto ast=parse_file(input,o);AOT a(target);write_text(output,a.gen(ast));return {true,"assembly emitted: "+output+" ("+target+")"};}catch(const std::exception&e){return {false,e.what()};}}
Result native_build(const std::string&input,const std::string&output,const std::string&target,const CompileOptions&o){try{auto tmp=fs::temp_directory_path()/"jau_native.s";auto r=emit_assembly(input,tmp.string(),target,o);if(!r.ok)return r;std::string cc;if(target=="linux-x86_64")cc="cc -no-pie "+tmp.string()+" -o "+output;else if(target=="linux-x86")cc="cc -m32 -no-pie "+tmp.string()+" -o "+output;else if(target=="windows-x86_64")cc="x86_64-w64-mingw32-gcc -no-pie "+tmp.string()+" -o "+output+".exe";else cc="i686-w64-mingw32-gcc -no-pie "+tmp.string()+" -o "+output+".exe";int rc=std::system(cc.c_str());if(rc!=0)return {false,"native linker failed; install the target GCC/MinGW toolchain. command: "+cc};return {true,"native executable built"};}catch(const std::exception&e){return {false,e.what()};}}
Result bundle_executable(const std::string&input,const std::string&output,const std::string&runtime_path,const CompileOptions&o){try{
    fs::path tmp=fs::temp_directory_path()/("jau_bundle_"+std::to_string(fnv1a64(input))+".jbc");auto r=compile_file(input,tmp.string(),o);if(!r.ok)return r;
    std::ifstream runtime(runtime_path,std::ios::binary),bc(tmp,std::ios::binary);if(!runtime)throw Error("cannot open runtime: "+runtime_path);if(!bc)throw Error("cannot open temporary bytecode");
    std::ofstream out(output,std::ios::binary);if(!out)throw Error("cannot write bundle: "+output);out<<runtime.rdbuf();std::streampos before=out.tellp();out<<bc.rdbuf();uint64_t n=(uint64_t)((std::streampos)out.tellp()-before);out.write((char*)&n,8);out.write("JAUEXEC1",8);out.close();
#ifndef _WIN32
    fs::permissions(output,fs::perms::owner_exec|fs::perms::group_exec|fs::perms::others_exec,fs::perm_options::add);
#endif
    std::error_code ec;fs::remove(tmp,ec);return {true,"standalone executable built: "+output};
}catch(const std::exception&e){return {false,e.what()};}}
Result run_embedded_executable(const std::string&exe,const std::vector<std::string>&args){try{
    std::ifstream in(exe,std::ios::binary);if(!in)throw Error("cannot open executable");in.seekg(0,std::ios::end);auto end=in.tellg();if(end<(std::streamoff)16)throw Error("no embedded Jau payload");in.seekg(end-(std::streamoff)16);uint64_t n=0;char magic[8];in.read((char*)&n,8);in.read(magic,8);if(std::memcmp(magic,"JAUEXEC1",8))throw Error("no embedded Jau payload");if(n>(uint64_t)(end-(std::streamoff)16))throw Error("corrupt embedded payload");in.seekg(end-(std::streamoff)16-(std::streamoff)n);std::vector<char> data((size_t)n);in.read(data.data(),(std::streamsize)n);
    fs::path tmp=fs::temp_directory_path()/("jau_embedded_"+std::to_string(fnv1a64(exe))+".jbc");{std::ofstream o2(tmp,std::ios::binary);o2.write(data.data(),(std::streamsize)data.size());}auto rr=run_bytecode(tmp.string(),args);std::error_code ec;fs::remove(tmp,ec);return rr;
}catch(const std::exception&e){return {false,e.what()};}}
std::string version(){return "0.5.0";}
}
