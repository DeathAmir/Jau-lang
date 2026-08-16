    }}return Value();}
};

static std::vector<std::unique_ptr<Stmt>> parse_file(const std::string&input,const CompileOptions&o){Lexer l(expand_imports(input,o));Parser p(l.scan());return p.parse();}
static Program compile_program(const std::string&input,const CompileOptions&o){auto ast=parse_file(input,o);BC b;return b.compile(ast,o.optimize);}

class AOT {
    std::string target; bool is64=false, win=false, library=false; int optimize=0,label=0; std::ostringstream o;
    std::unordered_map<std::string,Stmt*> funcs; std::unordered_set<std::string> externs; std::vector<std::string> native_symbols;
    // AOT string literals are pooled and emitted after code generation. Keeping
    // ownership here prevents temporary pointers from leaking into generated code.
    std::vector<std::pair<std::string,std::string>> string_literals; int string_serial=0;
    struct Ctx{std::unordered_map<std::string,int> locals,params;int next=0;std::string end,ns;std::vector<std::string> breaks,continues;}; Ctx c;
    std::string L(){return ".L"+std::to_string(label++);} int word()const{return is64?8:4;}
    std::string clean(const std::string&n)const{std::string r;for(unsigned char ch:n)r+=(std::isalnum(ch)||ch=='_')?(char)ch:'_';return r;}
    std::string sym(const std::string&n)const{return "jau_fn_"+clean(n);}
    static std::string canonical_foreign(std::string n){
        if(n.rfind("__imp_",0)==0)n=n.substr(6);else if(n.rfind("_imp__",0)==0)n=n.substr(6);
        while(n.size()>1&&n[0]=='_')n.erase(n.begin());
        auto at=n.find('@');if(at!=std::string::npos){bool digits=at+1<n.size();for(size_t i=at+1;i<n.size();++i)digits=digits&&std::isdigit((unsigned char)n[i]);if(digits)n=n.substr(0,at);}
        return n;
    }
    std::string foreign_sym(const std::string&n)const{
        std::string wanted=canonical_foreign(clean(n));
        for(auto&actual:native_symbols)if(actual==clean(n))return actual;
        std::string match;
        for(auto&actual:native_symbols)if(canonical_foreign(actual)==wanted){if(!match.empty()&&match!=actual)return clean(n);match=actual;}
        return match.empty()?clean(n):match;
    }
    bool has_native_symbol(const std::string&n)const{std::string wanted=canonical_foreign(clean(n));for(auto&actual:native_symbols)if(canonical_foreign(actual)==wanted)return true;return false;}
    static std::string asm_string(std::string v){
        std::string r; r.reserve(v.size()+8);
        for(unsigned char c:v){if(c=='\\')r+="\\\\";else if(c=='\"')r+="\\\"";else if(c=='\n')r+="\\n";else if(c=='\r')r+="\\r";else if(c=='\t')r+="\\t";else if(c==0)r+="\\0";else r+=(char)c;}
        return r;
    }
    std::string intern_string(const std::string&v){std::string lab=".Ljau_str_"+std::to_string(string_serial++);string_literals.push_back({lab,v});return lab;}
    void emit_puts_literal(const std::string&v){
        std::string lab=intern_string(v);
        if(is64){
            if(win)o<<"  lea rcx, [rip+"<<lab<<"]\n  sub rsp, 32\n  call puts\n  add rsp, 32\n";
            else o<<"  lea rdi, [rip+"<<lab<<"]\n  call puts@PLT\n";
        }else o<<"  push OFFSET FLAT:"<<lab<<"\n  call puts\n  add esp, 4\n";
        o<<"  xor eax,eax\n";push(is64?"rax":"eax");
    }
    int slot(const std::string&n){auto it=c.locals.find(n);if(it!=c.locals.end())return it->second;int q=(++c.next)*word();c.locals[n]=q;return q;}
    void push(const std::string&r){o<<"  push "<<r<<"\n";} void pop(const std::string&r){o<<"  pop "<<r<<"\n";}
    void load_var(const std::string&n){
        auto p=c.params.find(n);
        if(p!=c.params.end())o<<"  mov eax, DWORD PTR [ebp+"<<p->second<<"]\n";
        else o<<"  mov "<<(is64?"rax, QWORD PTR [rbp-":"eax, DWORD PTR [ebp-")<<slot(n)<<"]\n";
        push(is64?"rax":"eax");
    }
    bool simple_atom(Expr*e)const{
        if(!e)return false;if(e->k==Expr::Var)return true;if(e->k!=Expr::Lit)return false;return std::holds_alternative<int64_t>(e->v.data)||std::holds_alternative<bool>(e->v.data);
    }
    void atom_to(Expr*e,const std::string&reg){
        if(e->k==Expr::Lit){if(auto p=std::get_if<int64_t>(&e->v.data)){o<<"  mov "<<reg<<", "<<*p<<"\n";return;}if(auto p=std::get_if<bool>(&e->v.data)){o<<"  mov "<<reg<<", "<<(*p?1:0)<<"\n";return;}}
        if(e->k==Expr::Var){auto p=c.params.find(e->text);if(p!=c.params.end())o<<"  mov "<<reg<<", DWORD PTR [ebp+"<<p->second<<"]\n";else o<<"  mov "<<reg<<", "<<(is64?"QWORD PTR [rbp-":"DWORD PTR [ebp-")<<slot(e->text)<<"]\n";return;}
        throw Error("internal AOT atom error");
    }
    void emit_args64(const std::vector<std::unique_ptr<Expr>>&args){
        static const std::vector<std::string> wregs={"rcx","rdx","r8","r9"};
        static const std::vector<std::string> lregs={"rdi","rsi","rdx","rcx","r8","r9"};
        const auto&regs=win?wregs:lregs;
        if(args.size()>regs.size())throw Error("AOT C ABI currently supports at most "+std::to_string(regs.size())+" integer arguments on "+target);
        for(auto q=args.rbegin();q!=args.rend();++q)ex(q->get());
        for(size_t i=0;i<args.size();++i)pop(regs[i]);
    }
    void emit_named_call(const std::string&name,const std::vector<std::unique_ptr<Expr>>&args,bool foreign){
        if(is64){
            emit_args64(args);
            if(win)o<<"  sub rsp, 32\n";
            o<<"  call "<<(foreign?foreign_sym(name):sym(name))<<"\n";
            if(win)o<<"  add rsp, 32\n";
        }else{
            for(auto q=args.rbegin();q!=args.rend();++q)ex(q->get());
            o<<"  call "<<(foreign?foreign_sym(name):sym(name))<<"\n";
            if(!args.empty())o<<"  add esp, "<<(args.size()*4)<<"\n";
        }
        push(is64?"rax":"eax");
    }
    void emit_time_ms(bool nanos){
        if(!win)throw Error("time.now_ms/time.now_ns AOT intrinsic is currently implemented for Windows targets; VM mode is cross-platform");
        // Do not emit unconditional .extern directives in the AOT header. jauas
        // promotes unresolved call targets to extern symbols only when a timing
        // intrinsic is actually referenced. This keeps ordinary C-ABI objects
        // free of unused GetTickCount/GetTickCount64 undefined symbols.
        if(is64){o<<"  sub rsp, 32\n  call GetTickCount64\n  add rsp, 32\n";if(nanos)o<<"  imul rax, rax, 1000000\n";push("rax");}
        else{o<<"  call GetTickCount\n";if(nanos)o<<"  imul eax, eax, 1000000\n";push("eax");}
    }
    void ex(Expr*e){
        if(!e){o<<"  xor eax,eax\n";push(is64?"rax":"eax");return;}
        if(e->k==Expr::Lit){
            if(auto p=std::get_if<int64_t>(&e->v.data)){o<<"  mov "<<(is64?"rax":"eax")<<", "<<*p<<"\n";push(is64?"rax":"eax");return;}
            if(auto p=std::get_if<bool>(&e->v.data)){o<<"  mov eax, "<<(*p?1:0)<<"\n";push(is64?"rax":"eax");return;}
            if(auto p=std::get_if<std::string>(&e->v.data)){
                // Native string literals are emitted in .rodata and passed as a
                // borrowed const char*. This makes C ABI calls such as Win32 A
                // APIs practical without pretending Jau has a native owned-string ABI.
                auto lab=intern_string(*p);
                if(is64)o<<"  lea rax, [rip+"<<lab<<"]\n";
                else o<<"  mov eax, OFFSET FLAT:"<<lab<<"\n";
                push(is64?"rax":"eax");return;
            }
            throw Error("AOT supports integer/bool and string-literal pointer values; dynamic strings/arrays remain VM-only");
        }
        if(e->k==Expr::Var){load_var(e->text);return;}
        if(e->k==Expr::Assign){
            ex(e->a.get());pop(is64?"rax":"eax");
            auto p=c.params.find(e->text);
            if(p!=c.params.end())o<<"  mov DWORD PTR [ebp+"<<p->second<<"], eax\n";
            else o<<"  mov "<<(is64?"QWORD PTR [rbp-":"DWORD PTR [ebp-")<<slot(e->text)<<"], "<<(is64?"rax":"eax")<<"\n";
            push(is64?"rax":"eax");return;
        }
        if(e->k==Expr::Unary){
            ex(e->a.get());pop(is64?"rax":"eax");
            if(e->text=="-")o<<"  neg "<<(is64?"rax":"eax")<<"\n";
            else if(e->text=="~")o<<"  not "<<(is64?"rax":"eax")<<"\n";
            else o<<"  cmp "<<(is64?"rax":"eax")<<",0\n  sete al\n  movzx eax,al\n";
            push(is64?"rax":"eax");return;
        }
        if(e->k==Expr::Binary){
            if(optimize>=2&&simple_atom(e->a.get())&&simple_atom(e->b.get())){atom_to(e->a.get(),is64?"rax":"eax");atom_to(e->b.get(),is64?"rcx":"ecx");}
            else{ex(e->a.get());ex(e->b.get());pop(is64?"rcx":"ecx");pop(is64?"rax":"eax");}
            std::string op=e->text;
            if(op=="+")o<<"  add "<<(is64?"rax, rcx":"eax, ecx")<<"\n";
            else if(op=="-")o<<"  sub "<<(is64?"rax, rcx":"eax, ecx")<<"\n";
            else if(op=="*")o<<"  imul "<<(is64?"rax, rcx":"eax, ecx")<<"\n";
            else if(op=="&")o<<"  and "<<(is64?"rax, rcx":"eax, ecx")<<"\n";
            else if(op=="|")o<<"  or "<<(is64?"rax, rcx":"eax, ecx")<<"\n";
            else if(op=="^")o<<"  xor "<<(is64?"rax, rcx":"eax, ecx")<<"\n";
            else if(op=="<<")o<<"  shl "<<(is64?"rax, cl":"eax, cl")<<"\n";
            else if(op==">>")o<<"  sar "<<(is64?"rax, cl":"eax, cl")<<"\n";
            else if(op=="/"||op=="%"){if(is64)o<<"  cqo\n  idiv rcx\n";else o<<"  cdq\n  idiv ecx\n";if(op=="%")o<<"  mov "<<(is64?"rax, rdx":"eax, edx")<<"\n";}
            else if(op=="&&")o<<"  test "<<(is64?"rax, rax":"eax, eax")<<"\n  setne al\n  movzx eax,al\n  test "<<(is64?"rcx, rcx":"ecx, ecx")<<"\n  setne cl\n  and al,cl\n  movzx eax,al\n";
            else if(op=="||")o<<"  or "<<(is64?"rax, rcx":"eax, ecx")<<"\n  setne al\n  movzx eax,al\n";
            else{std::string cc=op=="=="?"e":op=="!="?"ne":op=="<"?"l":op=="<="?"le":op==">"?"g":"ge";o<<"  cmp "<<(is64?"rax, rcx":"eax, ecx")<<"\n  set"<<cc<<" al\n  movzx eax, al\n";}
            push(is64?"rax":"eax");return;
        }
        if(e->k==Expr::Call&&e->a&&e->a->k==Expr::Var){
            std::string n=e->a->text;
            if((n=="time.now_ms"||n=="clock_ms")&&e->args.empty()){emit_time_ms(false);return;}
            if((n=="time.now_ns"||n=="clock_ns")&&e->args.empty()){emit_time_ms(true);return;}
            if(n=="print"&&e->args.size()==1){
                // Literal strings are a native AOT primitive. Other string/array
                // operations still fail explicitly instead of silently miscompiling.
                if(e->args[0]&&e->args[0]->k==Expr::Lit){if(auto sv=std::get_if<std::string>(&e->args[0]->v.data)){emit_puts_literal(*sv);return;}}
                ex(e->args[0].get());pop(is64?"rax":"eax");
                if(is64){
                    if(win)o<<"  mov rdx, rax\n  lea rcx, [rip+fmt_i]\n  sub rsp, 32\n  call printf\n  add rsp, 32\n";
                    else o<<"  mov rsi, rax\n  lea rdi, [rip+fmt_i]\n  xor eax,eax\n  call printf@PLT\n";
                }else o<<"  push eax\n  push OFFSET FLAT:fmt_i\n  call printf\n  add esp, 8\n";
                o<<"  xor eax,eax\n";push(is64?"rax":"eax");return;
            }
            auto it=funcs.find(n);
            if(it==funcs.end()&&n.find('.')==std::string::npos&&!c.ns.empty()){std::string q=c.ns+"."+n;it=funcs.find(q);if(it!=funcs.end()){emit_named_call(q,e->args,false);return;}}
            if(it!=funcs.end()){emit_named_call(n,e->args,false);return;}
            if(externs.count(n)||has_native_symbol(n)){emit_named_call(n,e->args,true);return;}
            throw Error("AOT unknown function: "+n+" (not a Jau function, extern declaration, or discovered native object symbol)");
        }
        throw Error("AOT unsupported expression");
    }
    void st(Stmt*s){
        if(s->k==Stmt::ExprS){ex(s->expr.get());o<<"  add "<<(is64?"rsp,8":"esp,4")<<"\n";}
        else if(s->k==Stmt::VarS){ex(s->expr.get());pop(is64?"rax":"eax");o<<"  mov "<<(is64?"QWORD PTR [rbp-":"DWORD PTR [ebp-")<<slot(s->name)<<"], "<<(is64?"rax":"eax")<<"\n";}
        else if(s->k==Stmt::IfS){std::string el=L(),en=L();ex(s->cond.get());pop(is64?"rax":"eax");o<<"  cmp "<<(is64?"rax":"eax")<<",0\n  je "<<el<<"\n";for(auto&x:s->body)st(x.get());o<<"  jmp "<<en<<"\n"<<el<<":\n";for(auto&x:s->else_body)st(x.get());o<<en<<":\n";}
        else if(s->k==Stmt::WhileS){std::string a=L(),b=L();c.continues.push_back(a);c.breaks.push_back(b);o<<a<<":\n";ex(s->cond.get());pop(is64?"rax":"eax");o<<"  cmp "<<(is64?"rax":"eax")<<",0\n  je "<<b<<"\n";for(auto&x:s->body)st(x.get());o<<"  jmp "<<a<<"\n"<<b<<":\n";c.continues.pop_back();c.breaks.pop_back();}
        else if(s->k==Stmt::ForS){if(s->init)st(s->init.get());std::string a=L(),step=L(),b=L();c.continues.push_back(step);c.breaks.push_back(b);o<<a<<":\n";ex(s->cond.get());pop(is64?"rax":"eax");o<<"  cmp "<<(is64?"rax":"eax")<<",0\n  je "<<b<<"\n";for(auto&x:s->body)st(x.get());o<<step<<":\n";if(s->step){ex(s->step.get());o<<"  add "<<(is64?"rsp,8":"esp,4")<<"\n";}o<<"  jmp "<<a<<"\n"<<b<<":\n";c.continues.pop_back();c.breaks.pop_back();}
        else if(s->k==Stmt::Block){for(auto&x:s->body)st(x.get());}
        else if(s->k==Stmt::ReturnS){if(s->expr){ex(s->expr.get());pop(is64?"rax":"eax");}else o<<"  xor eax,eax\n";o<<"  jmp "<<c.end<<"\n";}
        else if(s->k==Stmt::BreakS){if(c.breaks.empty())throw Error("break outside AOT loop");o<<"  jmp "<<c.breaks.back()<<"\n";}
        else if(s->k==Stmt::ContinueS){if(c.continues.empty())throw Error("continue outside AOT loop");o<<"  jmp "<<c.continues.back()<<"\n";}
    }
    void emit_func(Stmt*s){
        c=Ctx{};c.end=L();auto dot=s->name.rfind('.');c.ns=dot==std::string::npos?std::string():s->name.substr(0,dot);
        std::string sn=sym(s->name);o<<".globl "<<sn<<"\n"<<sn<<":\n";
        o<<(is64?"  push rbp\n  mov rbp,rsp\n  sub rsp, 1024\n":"  push ebp\n  mov ebp,esp\n  sub esp, 512\n");
        if(is64){
            static const std::vector<std::string> wregs={"rcx","rdx","r8","r9"};
            static const std::vector<std::string> lregs={"rdi","rsi","rdx","rcx","r8","r9"};
            const auto&regs=win?wregs:lregs;
            if(s->params.size()>regs.size())throw Error("AOT function "+s->name+" exceeds current C ABI register-argument limit");
            for(size_t i=0;i<s->params.size();++i){int off=slot(s->params[i]);o<<"  mov QWORD PTR [rbp-"<<off<<"], "<<regs[i]<<"\n";}
        }else{
            for(size_t i=0;i<s->params.size();++i)c.params[s->params[i]]=8+(int)i*4;
        }
        for(auto&x:s->body)st(x.get());
        o<<"  xor eax,eax\n"<<c.end<<":\n  leave\n  ret\n";
    }
    std::string peephole(std::string s)const{
        if(optimize<2)return s;
        const std::vector<std::pair<std::string,std::string>> rules={
            {"  push rax\n  pop rax\n",""},{"  push eax\n  pop eax\n",""},
            {"  push rcx\n  pop rcx\n",""},{"  push ecx\n  pop ecx\n",""},
            {"  push rax\n  add rsp,8\n",""},{"  push eax\n  add esp,4\n",""}
        };
        for(auto&r:rules){for(size_t p=0;(p=s.find(r.first,p))!=std::string::npos;)s.replace(p,r.first.size(),r.second);}
        return s;
    }
public:
    AOT(std::string t,bool lib=false,int opt=0,const std::vector<std::string>&ns={}):target(std::move(t)),library(lib),optimize(0),native_symbols(ns){
        // v0.8.2 safety gate: AOT optimizations are intentionally disabled until
        // each transformation has side-effect and control-flow regression tests.
        // Accept -O flags for CLI compatibility, but never trade correctness for speed.
        (void)opt;
        win=target.rfind("windows",0)==0;is64=target.find("x86_64")!=std::string::npos;
        if(target!="linux-x86_64"&&target!="linux-x86"&&target!="windows-x86_64"&&target!="windows-x86")throw Error("unknown target: "+target);
    }
    std::string gen(std::vector<std::unique_ptr<Stmt>>&ast){
        // Do not mutate the AST here. The old optimizer could erase observable
        // work before native codegen. Native AOT remains deliberately unoptimized
        // until transformations are proven semantics-preserving by tests.
        for(auto&s:ast)if(s->k==Stmt::FuncS){if(s->external)externs.insert(s->name);else funcs[s->name]=s.get();}
        // Generated assembly is a user artifact. Keep tool branding in CLI
        // stderr only; never inject it into source/object/executable output.
        o<<".intel_syntax noprefix\n.section .rodata\nfmt_i: .asciz \""<<(is64?"%lld":"%d")<<"\\n\"\n.text\n.extern printf\n.extern puts\n";
        for(auto&n:externs)o<<".extern "<<foreign_sym(n)<<"\n";
        for(auto&s:ast)if(s->k==Stmt::FuncS&&!s->external)emit_func(s.get());
        if(!library){
            // Native executable entry semantics:
            //   1) execute top-level statements once;
            //   2) if a zero-argument user func main() exists and top-level code
            //      did not explicitly call main(), invoke jau_fn_main exactly once.
            // This fixes the silent-empty-EXE regression where func main() was
            // emitted but the PE entry point never called it.
            Stmt* user_main=nullptr;auto mi=funcs.find("main");if(mi!=funcs.end())user_main=mi->second;
            if(user_main&&!user_main->params.empty())throw Error("native entry func main() must not take parameters");
            bool explicit_main=false;
            for(auto&s:ast)if(s->k==Stmt::ExprS&&s->expr&&s->expr->k==Expr::Call&&s->expr->a&&s->expr->a->k==Expr::Var&&s->expr->a->text=="main")explicit_main=true;
            c=Ctx{};c.end=L();o<<".globl main\nmain:\n"<<(is64?"  push rbp\n  mov rbp,rsp\n  sub rsp, 1024\n":"  push ebp\n  mov ebp,esp\n  sub esp, 512\n");
            for(auto&s:ast)if(s->k!=Stmt::FuncS)st(s.get());
            if(user_main&&!explicit_main){
                if(is64){if(win)o<<"  sub rsp, 32\n  call jau_fn_main\n  add rsp, 32\n";else o<<"  call jau_fn_main\n";}
                else o<<"  call jau_fn_main\n";
            }else o<<"  xor eax,eax\n";
            o<<c.end<<":\n  leave\n  ret\n";
        }
        if(!string_literals.empty()){o<<".section .rodata\n";for(auto&kv:string_literals)o<<kv.first<<": .asciz \""<<asm_string(kv.second)<<"\"\n";}
        return o.str();
    }
};
Result compile_file(const std::string&input,const std::string&output,const CompileOptions&o){try{auto p=compile_program(input,o);save(p,output);return {true,"built "+output};}catch(const std::exception&e){return {false,e.what()};}}
Result run_source(const std::string&input,const CompileOptions&o,const std::vector<std::string>&args){try{g_runtime_args=args;VM vm(compile_program(input,o),o.debug);vm.run();return {true,""};}catch(const std::exception&e){return {false,e.what()};}}
Result run_bytecode(const std::string&input,const std::vector<std::string>&args){try{g_runtime_args=args;VM vm(load(input));vm.run();return {true,""};}catch(const std::exception&e){return {false,e.what()};}}
Result emit_assembly(const std::string&input,const std::string&output,const std::string&target,const CompileOptions&o){try{auto ast=parse_file(input,o);AOT a(target,o.library_mode,o.optimize,o.native_symbols);write_text(output,a.gen(ast));return {true,"assembly emitted: "+output+" ("+target+", safe-AOT/O0)"};}catch(const std::exception&e){return {false,e.what()};}}
static bool shared_export_ident(const std::string&s){if(s.empty()||!(std::isalpha((unsigned char)s[0])||s[0]=='_'))return false;for(char c:s)if(!(std::isalnum((unsigned char)c)||c=='_'))return false;return true;}
static void append_shared_export_aliases(const fs::path&asm_path,const CompileOptions&o){if(!o.shared)return;std::ofstream f(asm_path,std::ios::app);if(!f)throw Error("cannot append shared export aliases");for(auto&spec:o.exports){auto q=spec.find('=');std::string pub=q==std::string::npos?spec:spec.substr(0,q),in=q==std::string::npos?spec:spec.substr(q+1);if(!shared_export_ident(pub)||!shared_export_ident(in))throw Error("--export expects identifier or public=internal identifiers");std::string target=in.rfind("jau_fn_",0)==0?in:"jau_fn_"+in;if(pub!=target)f<<"\n.globl "<<pub<<"\n.set "<<pub<<","<<target<<"\n";}}
Result native_build(const std::string&input,const std::string&output,const std::string&target,const CompileOptions&o){try{
    auto tmp=fs::temp_directory_path()/"jau_native.s";auto r=emit_assembly(input,tmp.string(),target,o);if(!r.ok)return r;append_shared_export_aliases(tmp,o);
    bool cpp=false;for(auto&x:o.native_inputs){auto e=lower_copy(fs::path(x).extension().string());if(e==".cpp"||e==".cxx"||e==".cc"||e==".c++")cpp=true;}
    std::string cc;
    if(target=="linux-x86_64")cc=(cpp?"c++":"cc")+std::string(o.shared?" -shared -fPIC ":" -no-pie ")+shell_quote(tmp.string());
    else if(target=="linux-x86")cc=(cpp?"c++":"cc")+std::string(o.shared?" -m32 -shared -fPIC ":" -m32 -no-pie ")+shell_quote(tmp.string());
    else if(target=="windows-x86_64")cc=(cpp?"x86_64-w64-mingw32-g++":"x86_64-w64-mingw32-gcc")+std::string(" -no-pie ")+shell_quote(tmp.string());
    else if(target=="windows-x86")cc=(cpp?"i686-w64-mingw32-g++":"i686-w64-mingw32-gcc")+std::string(" -no-pie ")+shell_quote(tmp.string());
    else return {false,"unknown target: "+target};
    for(auto&x:o.native_inputs)cc+=" "+shell_quote(x);
    // Linux uses the host/system linker for final ELF generation. Logical
    // library names become -lfoo; explicit .a/.so paths are passed verbatim.
    for(auto lib:o.system_libs){
        auto ext=lower_copy(fs::path(lib).extension().string());
        if(fs::exists(lib)||ext==".a"||ext==".so"||ext==".dylib")cc+=" "+shell_quote(lib);
        else{
            if(lib.rfind("lib",0)==0)lib=lib.substr(3);
            auto q=lib.find('.');if(q!=std::string::npos)lib=lib.substr(0,q);
            cc+=" -l"+lib;
        }
    }
    std::string final_out=output;if(target.rfind("windows-",0)==0&&lower_copy(fs::path(final_out).extension().string())!=".exe")final_out+=".exe";
    cc+=" -o "+shell_quote(final_out);
    int rc=std::system(cc.c_str());if(rc!=0)return {false,"native linker failed; install the target C/C++ toolchain. command: "+cc};return {true,"native executable built: "+final_out};
}catch(const std::exception&e){return {false,e.what()};}}
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
std::string version(){return "0.9.0";}
std::string copyright_notice(){return "DeathAmir Jau @ DeathAmir 2026 (C)";}
}