from pathlib import Path

p=Path('src/jau_part02b.inc')
s=p.read_text(encoding='utf-8')
old='''public:\n    Program compile(std::vector<std::unique_ptr<Stmt>>&ast,int opt){if(opt>0)optimize_stmts(ast);\n        p.funcs.push_back({"<main>",0,0,{}});fidx["<main>"]=0;'''
new='''public:\n    Program compile(std::vector<std::unique_ptr<Stmt>>&ast,int opt){\n        // v0.8.2 correctness gate: bytecode optimization is disabled together\n        // with AOT optimization. -O flags stay accepted for CLI compatibility,\n        // but no AST rewrite is allowed until side effects and control flow are\n        // covered by transformation-specific regression tests.\n        (void)opt;\n        p.funcs.push_back({"<main>",0,0,{}});fidx["<main>"]=0;'''
if old not in s: raise SystemExit('BC optimizer anchor missing')
s=s.replace(old,new,1)
old='''        f=&p.funcs[0];top=true;for(auto&s:ast)if(s->k!=Stmt::FuncS)st(s.get());emit(Op::Null);emit(Op::Ret);'''
new='''        // Keep VM and native entry semantics identical. Top-level statements\n        // execute first. A zero-argument user func main() is then invoked once,\n        // unless source explicitly contains a top-level main() call. This makes\n        // normal Jau programs behave consistently across run/build/native.\n        bool explicit_main=false;\n        for(auto&s:ast)if(s->k==Stmt::ExprS&&s->expr&&s->expr->k==Expr::Call&&s->expr->a&&s->expr->a->k==Expr::Var&&s->expr->a->text=="main")explicit_main=true;\n        f=&p.funcs[0];top=true;for(auto&s:ast)if(s->k!=Stmt::FuncS)st(s.get());\n        auto main_it=fidx.find("main");\n        if(main_it!=fidx.end()&&!explicit_main){if(p.funcs[main_it->second].arity!=0)throw Error("entry func main() must not take parameters");emit(Op::Call,main_it->second);emit(Op::Pop);}\n        emit(Op::Null);emit(Op::Ret);'''
if old not in s: raise SystemExit('BC entry anchor missing')
s=s.replace(old,new,1)
p.write_text(s,encoding='utf-8')

p=Path('docs/MAINTAINERS.md')
if p.exists():
    s=p.read_text(encoding='utf-8')
    s=s.replace('AOT optimization is disabled in v0.8.2.', 'VM and AOT optimization are disabled in v0.8.2.')
    s=s.replace('Native executable codegen must call `jau_fn_main` exactly once', 'VM and native executable codegen must invoke the user `main` exactly once')
    p.write_text(s,encoding='utf-8')
print('Jau 0.8.2 VM entry/optimizer patch applied')
