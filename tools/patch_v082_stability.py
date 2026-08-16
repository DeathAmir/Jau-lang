from pathlib import Path
import re

COPYRIGHT = 'DeathAmir Jau @ DeathAmir 2026 (C)'


def replace_once(path, old, new, label):
    p = Path(path)
    s = p.read_text(encoding='utf-8')
    if old not in s:
        raise SystemExit(f'{label} anchor missing in {path}')
    p.write_text(s.replace(old, new, 1), encoding='utf-8')

# Correctness first: optimization is opt-in for bytecode and disabled for AOT
# until every optimization has semantic regression coverage.
replace_once('include/jau/jau.hpp', '    int optimize = 2;', '    int optimize = 0;', 'default optimizer')
replace_once('include/jau/jau.hpp', 'std::string version();', 'std::string version();\nstd::string copyright_notice();', 'copyright declaration')

p = Path('src/jau_part03.inc')
s = p.read_text(encoding='utf-8')

old = '    std::unordered_map<std::string,Stmt*> funcs; std::unordered_set<std::string> externs; std::vector<std::string> native_symbols;'
new = '''    std::unordered_map<std::string,Stmt*> funcs; std::unordered_set<std::string> externs; std::vector<std::string> native_symbols;
    // AOT string literals are pooled and emitted after code generation. Keeping
    // ownership here prevents temporary pointers from leaking into generated code.
    std::vector<std::pair<std::string,std::string>> string_literals; int string_serial=0;'''
if old not in s: raise SystemExit('AOT fields anchor missing')
s=s.replace(old,new,1)

anchor = '    bool has_native_symbol(const std::string&n)const{std::string wanted=canonical_foreign(clean(n));for(auto&actual:native_symbols)if(canonical_foreign(actual)==wanted)return true;return false;}\n'
insert = r'''    bool has_native_symbol(const std::string&n)const{std::string wanted=canonical_foreign(clean(n));for(auto&actual:native_symbols)if(canonical_foreign(actual)==wanted)return true;return false;}
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
'''
if anchor not in s: raise SystemExit('native symbol anchor missing')
s=s.replace(anchor,insert,1)

old = '''            if(n=="print"&&e->args.size()==1){
                ex(e->args[0].get());pop(is64?"rax":"eax");'''
new = '''            if(n=="print"&&e->args.size()==1){
                // Literal strings are a native AOT primitive. Other string/array
                // operations still fail explicitly instead of silently miscompiling.
                if(e->args[0]&&e->args[0]->k==Expr::Lit){if(auto sv=std::get_if<std::string>(&e->args[0]->v.data)){emit_puts_literal(*sv);return;}}
                ex(e->args[0].get());pop(is64?"rax":"eax");'''
if old not in s: raise SystemExit('print AOT anchor missing')
s=s.replace(old,new,1)

old = '    AOT(std::string t,bool lib=false,int opt=0,const std::vector<std::string>&ns={}):target(std::move(t)),library(lib),optimize(opt),native_symbols(ns){\n'
new = '''    AOT(std::string t,bool lib=false,int opt=0,const std::vector<std::string>&ns={}):target(std::move(t)),library(lib),optimize(0),native_symbols(ns){
        // v0.8.2 safety gate: AOT optimizations are intentionally disabled until
        // each transformation has side-effect and control-flow regression tests.
        // Accept -O flags for CLI compatibility, but never trade correctness for speed.
        (void)opt;
'''
if old not in s: raise SystemExit('AOT constructor anchor missing')
s=s.replace(old,new,1)

old = '''    std::string gen(std::vector<std::unique_ptr<Stmt>>&ast){
        if(optimize>0)optimize_stmts(ast);
        for(auto&s:ast)if(s->k==Stmt::FuncS){if(s->external)externs.insert(s->name);else funcs[s->name]=s.get();}
        o<<".intel_syntax noprefix\\n.section .rodata\\nfmt_i: .asciz \\\""<<(is64?"%lld":"%d")<<"\\\\n\\\"\\n.text\\n.extern printf\\n";'''
new = '''    std::string gen(std::vector<std::unique_ptr<Stmt>>&ast){
        // Do not mutate the AST here. The old optimizer could erase observable
        // work before native codegen. Native AOT remains deliberately unoptimized
        // until transformations are proven semantics-preserving by tests.
        for(auto&s:ast)if(s->k==Stmt::FuncS){if(s->external)externs.insert(s->name);else funcs[s->name]=s.get();}
        o<<".intel_syntax noprefix\\n# DeathAmir Jau @ DeathAmir 2026 (C)\\n.section .rodata\\nfmt_i: .asciz \\\""<<(is64?"%lld":"%d")<<"\\\\n\\\"\\njau_copyright: .asciz \\\"DeathAmir Jau @ DeathAmir 2026 (C)\\\"\\n.text\\n.extern printf\\n.extern puts\\n";'''
if old not in s: raise SystemExit('AOT gen header anchor missing')
s=s.replace(old,new,1)

old = '''        if(!library){
            c=Ctx{};c.end=L();o<<".globl main\\nmain:\\n"<<(is64?"  push rbp\\n  mov rbp,rsp\\n  sub rsp, 1024\\n":"  push ebp\\n  mov ebp,esp\\n  sub esp, 512\\n");
            for(auto&s:ast)if(s->k!=Stmt::FuncS)st(s.get());
            o<<"  xor eax,eax\\n"<<c.end<<":\\n  leave\\n  ret\\n";
        }
        return peephole(o.str());'''
new = '''        if(!library){
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
            c=Ctx{};c.end=L();o<<".globl main\\nmain:\\n"<<(is64?"  push rbp\\n  mov rbp,rsp\\n  sub rsp, 1024\\n":"  push ebp\\n  mov ebp,esp\\n  sub esp, 512\\n");
            for(auto&s:ast)if(s->k!=Stmt::FuncS)st(s.get());
            if(user_main&&!explicit_main){
                if(is64){if(win)o<<"  sub rsp, 32\\n  call jau_fn_main\\n  add rsp, 32\\n";else o<<"  call jau_fn_main\\n";}
                else o<<"  call jau_fn_main\\n";
            }else o<<"  xor eax,eax\\n";
            o<<c.end<<":\\n  leave\\n  ret\\n";
        }
        if(!string_literals.empty()){o<<".section .rodata\\n";for(auto&kv:string_literals)o<<kv.first<<": .asciz \\\""<<asm_string(kv.second)<<"\\\"\\n";}
        return o.str();'''
if old not in s: raise SystemExit('native entry block anchor missing')
s=s.replace(old,new,1)

old = 'Result emit_assembly(const std::string&input,const std::string&output,const std::string&target,const CompileOptions&o){try{auto ast=parse_file(input,o);AOT a(target,o.library_mode,o.optimize,o.native_symbols);write_text(output,a.gen(ast));return {true,"assembly emitted: "+output+" ("+target+", O"+std::to_string(o.optimize)+")"};}catch(const std::exception&e){return {false,e.what()};}}'
new = 'Result emit_assembly(const std::string&input,const std::string&output,const std::string&target,const CompileOptions&o){try{auto ast=parse_file(input,o);AOT a(target,o.library_mode,o.optimize,o.native_symbols);write_text(output,a.gen(ast));return {true,"assembly emitted: "+output+" ("+target+", safe-AOT/O0)"};}catch(const std::exception&e){return {false,e.what()};}}'
if old not in s: raise SystemExit('emit_assembly result anchor missing')
s=s.replace(old,new,1)

if 'std::string version(){return "0.8.1";}' not in s: raise SystemExit('version anchor missing')
s=s.replace('std::string version(){return "0.8.1";}','std::string version(){return "0.8.2";}\nstd::string copyright_notice(){return "DeathAmir Jau @ DeathAmir 2026 (C)";}',1)
p.write_text(s,encoding='utf-8')

# Native builds used to force O3 even when the user did not request it. Keep
# native default at O0 until the optimizer is re-certified by regression tests.
p=Path('src/jauc_main.cpp');s=p.read_text(encoding='utf-8')
old='if(cmd=="native"&&!optimize_set)opt.optimize=3;'
if old not in s: raise SystemExit('native O3 default anchor missing')
s=s.replace(old,'if(cmd=="native"&&!optimize_set)opt.optimize=0; // v0.8.2 correctness-first native default',1)
old='int main(int argc,char**argv){try{return jauc_main_impl(argc,argv);}'
if old not in s: raise SystemExit('jauc main anchor missing')
s=s.replace(old,'int main(int argc,char**argv){std::cerr<<jau::copyright_notice()<<"\\n";try{return jauc_main_impl(argc,argv);}',1)
p.write_text(s,encoding='utf-8')

# Tool banners go to stderr so compiler/program stdout stays script-friendly.
p=Path('src/jur_main.cpp');s=p.read_text(encoding='utf-8')
old='int main(int argc, char** argv) {\n'
if old not in s: raise SystemExit('jur main anchor missing')
s=s.replace(old,'int main(int argc, char** argv) {\n    std::cerr << jau::copyright_notice() << "\\n";\n',1)
p.write_text(s,encoding='utf-8')

for path in ['src/jauas_main.cpp','src/jau_setup.cpp']:
    p=Path(path);s=p.read_text(encoding='utf-8')
    m=re.search(r'int main\s*\(([^)]*)\)\s*\{',s)
    if not m: raise SystemExit(f'main anchor missing in {path}')
    pos=m.end();s=s[:pos]+'\n    std::cerr << "'+COPYRIGHT+'\\n";'+s[pos:]
    p.write_text(s,encoding='utf-8')

# jauld main lives in the split linker implementation.
p=Path('src/jauld_part03.inc');s=p.read_text(encoding='utf-8')
m=re.search(r'int main\s*\(([^)]*)\)\s*\{',s)
if not m: raise SystemExit('jauld main anchor missing')
pos=m.end();s=s[:pos]+'std::cerr<<"'+COPYRIGHT+'\\n";'+s[pos:]
p.write_text(s,encoding='utf-8')

# Regression fixtures deliberately avoid external packages for the entry test.
Path('tests/v082_native_main.jau').write_text('''func main()\n{\n    let a = 10;\n    let b = 20;\n    print(a + b);\n    print(a * b);\n    print(2 * 2 * 2 * 2 * 2);\n    return 0;\n}\n''',encoding='utf-8')
Path('tests/v082_native_string.jau').write_text('''func main()\n{\n    print("Add:");\n    print(30);\n    print("Multiply:");\n    print(42);\n    return 0;\n}\n''',encoding='utf-8')
Path('tests/v082_control_flow.jau').write_text('''func main()\n{\n    let total = 0;\n    for (let i = 0; i < 5; i++) { total += i; }\n    let bits = (0x55 | 0x0A) ^ 0x03;\n    if (total == 10 and bits == 92) { print(1); } else { print(0); }\n    return 0;\n}\n''',encoding='utf-8')

Path('docs/MAINTAINERS.md').write_text('''# Jau Maintainer Notes\n\nCopyright: **DeathAmir Jau @ DeathAmir 2026 (C)**\n\n## Native correctness rules\n\n1. `func main()` is a language entry function. Native executable codegen must call `jau_fn_main` exactly once when there is no explicit top-level `main()` call. Never rely on users adding `main();` manually.\n2. `.jaux` object files are binary linker inputs. They must never enter the lexer/parser. Package wrapper `.jau` source is parsed; target `.obj/.o` files are only scanned for symbols and handed to the linker.\n3. Native C ABI `extern func` declarations inside namespaces retain their raw external symbol names. Namespace qualification belongs to Jau wrapper functions, not C ABI symbols.\n4. AOT optimization is disabled in v0.8.2. Re-enable transformations only after side-effect, entry-point, loop, namespace, native-call, string-print and error-path regressions exist for each transformation. Correct code is more important than smaller/faster code.\n5. Never silently drop unsupported syntax. Return a stage-specific compiler error. All public compiler entry points must catch exceptions and convert them into diagnostics rather than process crashes.\n6. AOT literal string `print` is supported directly; broader dynamic string/array AOT support still requires a stable native runtime ABI. Do not pretend VM-only builtins are native-capable until they have an implementation and tests.\n\n## Required release smoke tests\n\n- `func main()` with no top-level `main();` prints from a native PE.\n- Literal string and integer printing work in native PE32+ and PE32.\n- `for`, `if`, compound assignment, boolean aliases, bitwise operators and numeric literals survive AOT.\n- MathX `.jaux` resolves C ABI symbols and executes on x64/x86.\n- Invalid source returns an error code and diagnostic, never an access violation or uncaught exception.\n- `jauc`, `jur`, `jauas`, `jauld`, setup and bundled JauPM retain the DeathAmir copyright banner on stderr; generated AOT assembly embeds the copyright notice.\n''',encoding='utf-8')

print('Jau 0.8.2 stability patch applied')
