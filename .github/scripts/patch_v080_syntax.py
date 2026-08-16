from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"anchor missing: {label}")
    return text.replace(old, new, 1)


p = Path("src/jau_part01.inc")
s = p.read_text(encoding="utf-8")

s = replace_once(
    s,
    'enum class TK { End, Id, Num, Str, LParen,RParen,LBrace,RBrace,LBracket,RBracket,Comma,Semi,Colon,Dot,Plus,Minus,Star,Slash,Percent,Bang,Eq,Lt,Gt,EqEq,Ne,Le,Ge,AndAnd,OrOr,\n    Func,Extern,Namespace,If,Else,While,Return,Break,Continue,Let,Const,True,False,Null };',
    'enum class TK { End, Id, Num, Str, LParen,RParen,LBrace,RBrace,LBracket,RBracket,Comma,Semi,Colon,Dot,Plus,Minus,Star,Slash,Percent,Bang,Eq,Lt,Gt,EqEq,Ne,Le,Ge,AndAnd,OrOr,\n    PlusEq,MinusEq,StarEq,SlashEq,PercentEq,Inc,Dec,Func,Extern,Namespace,If,Else,While,For,Return,Break,Continue,Let,Const,True,False,Null };',
    "token enum",
)

s = replace_once(
    s,
    "        if(peek()=='/'&&peek(1)=='/'){while(peek()&&peek()!='\\n')get();continue;}\n        if(peek()=='#'){while(peek()&&peek()!='\\n')get();continue;}",
    "        if(peek()=='/'&&peek(1)=='/'){while(peek()&&peek()!='\\n')get();continue;}\n        if(peek()=='/'&&peek(1)=='*'){int start=line;get();get();while(peek()&&!(peek()=='*'&&peek(1)=='/'))get();if(!peek())throw Error(\"unterminated block comment starting at line \"+std::to_string(start));get();get();continue;}\n        if(peek()=='#'){while(peek()&&peek()!='\\n')get();continue;}",
    "block comments",
)

s = replace_once(
    s,
    '            static const std::unordered_map<std::string,TK> kw={{"func",TK::Func},{"extern",TK::Extern},{"namespace",TK::Namespace},{"if",TK::If},{"else",TK::Else},{"while",TK::While},{"return",TK::Return},{"break",TK::Break},{"continue",TK::Continue},{"let",TK::Let},{"const",TK::Const},{"true",TK::True},{"false",TK::False},{"null",TK::Null}};',
    '            static const std::unordered_map<std::string,TK> kw={{"func",TK::Func},{"fn",TK::Func},{"extern",TK::Extern},{"namespace",TK::Namespace},{"if",TK::If},{"else",TK::Else},{"while",TK::While},{"for",TK::For},{"return",TK::Return},{"break",TK::Break},{"continue",TK::Continue},{"let",TK::Let},{"var",TK::Let},{"const",TK::Const},{"true",TK::True},{"false",TK::False},{"null",TK::Null}};',
    "keyword aliases",
)

s = replace_once(
    s,
    "        if(two('=','=',TK::EqEq)||two('!','=',TK::Ne)||two('<','=',TK::Le)||two('>','=',TK::Ge)||two('&','&',TK::AndAnd)||two('|','|',TK::OrOr))continue;",
    "        if(two('=','=',TK::EqEq)||two('!','=',TK::Ne)||two('<','=',TK::Le)||two('>','=',TK::Ge)||two('&','&',TK::AndAnd)||two('|','|',TK::OrOr)||two('+','=',TK::PlusEq)||two('-','=',TK::MinusEq)||two('*','=',TK::StarEq)||two('/','=',TK::SlashEq)||two('%','=',TK::PercentEq)||two('+','+',TK::Inc)||two('-','-',TK::Dec))continue;",
    "two-character operators",
)

s = replace_once(
    s,
    'struct Stmt { enum K{ExprS,VarS,Block,IfS,WhileS,ReturnS,BreakS,ContinueS,FuncS} k; std::string name; bool constant=false, external=false; std::unique_ptr<Expr> expr,cond; std::vector<std::unique_ptr<Stmt>> body,else_body; std::vector<std::string> params; };',
    'struct Stmt { enum K{ExprS,VarS,Block,IfS,WhileS,ForS,ReturnS,BreakS,ContinueS,FuncS} k; std::string name; bool constant=false, external=false; std::unique_ptr<Expr> expr,cond,step; std::unique_ptr<Stmt> init; std::vector<std::unique_ptr<Stmt>> body,else_body; std::vector<std::string> params; };',
    "ForS AST",
)

old_call = '''    std::unique_ptr<Expr> call(){auto e=primary();for(;;){if(eat(TK::LParen)){auto c=std::make_unique<Expr>();c->k=Expr::Call;c->a=std::move(e);if(!at(TK::RParen)){do{c->args.push_back(expr());}while(eat(TK::Comma));}need(TK::RParen,"expected )");e=std::move(c);continue;}if(eat(TK::LBracket)){if(e->k==Expr::Var&&e->text.find('.')!=std::string::npos){auto c=std::make_unique<Expr>();c->k=Expr::Call;c->a=std::move(e);if(!at(TK::RBracket)){do{c->args.push_back(expr());}while(eat(TK::Comma));}need(TK::RBracket,"expected ]");e=std::move(c);}else{auto n=std::make_unique<Expr>();n->k=Expr::Index;n->a=std::move(e);n->b=expr();need(TK::RBracket,"expected ]");e=std::move(n);}continue;}break;}return e;}\n    std::unique_ptr<Expr> unary(){if(at(TK::Bang)||at(TK::Minus)){std::string op=cur().text;++i;auto e=std::make_unique<Expr>();e->k=Expr::Unary;e->text=op;e->a=unary();return e;}return call();}'''
new_call = '''    std::unique_ptr<Expr> update_expr(std::unique_ptr<Expr> base,bool inc){if(!base||base->k!=Expr::Var)throw Error("++/-- requires a variable at line "+std::to_string(cur().line));std::string n=base->text;auto lhs=std::make_unique<Expr>();lhs->k=Expr::Var;lhs->text=n;auto one=std::make_unique<Expr>();one->k=Expr::Lit;one->v=(int64_t)1;auto b=std::make_unique<Expr>();b->k=Expr::Binary;b->text=inc?"+":"-";b->a=std::move(lhs);b->b=std::move(one);auto a=std::make_unique<Expr>();a->k=Expr::Assign;a->text=n;a->a=std::move(b);return a;}\n    std::unique_ptr<Expr> call(){auto e=primary();for(;;){if(eat(TK::LParen)){auto c=std::make_unique<Expr>();c->k=Expr::Call;c->a=std::move(e);if(!at(TK::RParen)){do{c->args.push_back(expr());}while(eat(TK::Comma));}need(TK::RParen,"expected )");e=std::move(c);continue;}if(eat(TK::LBracket)){if(e->k==Expr::Var&&e->text.find('.')!=std::string::npos){auto c=std::make_unique<Expr>();c->k=Expr::Call;c->a=std::move(e);if(!at(TK::RBracket)){do{c->args.push_back(expr());}while(eat(TK::Comma));}need(TK::RBracket,"expected ]");e=std::move(c);}else{auto n=std::make_unique<Expr>();n->k=Expr::Index;n->a=std::move(e);n->b=expr();need(TK::RBracket,"expected ]");e=std::move(n);}continue;}if(at(TK::Inc)||at(TK::Dec)){bool inc=eat(TK::Inc);if(!inc)need(TK::Dec,"expected --");e=update_expr(std::move(e),inc);continue;}break;}return e;}\n    std::unique_ptr<Expr> unary(){if(at(TK::Inc)||at(TK::Dec)){bool inc=eat(TK::Inc);if(!inc)need(TK::Dec,"expected --");return update_expr(call(),inc);}if(at(TK::Bang)||at(TK::Minus)){std::string op=cur().text;++i;auto e=std::make_unique<Expr>();e->k=Expr::Unary;e->text=op;e->a=unary();return e;}return call();}'''
s = replace_once(s, old_call, new_call, "increment/decrement parser")

s = replace_once(
    s,
    '    std::unique_ptr<Expr> assign(){auto e=lor();if(eat(TK::Eq)){if(e->k!=Expr::Var)throw Error("invalid assignment target");auto n=std::make_unique<Expr>();n->k=Expr::Assign;n->text=e->text;n->a=assign();return n;}return e;}',
    '    std::unique_ptr<Expr> assign(){auto e=lor();if(at(TK::Eq)||at(TK::PlusEq)||at(TK::MinusEq)||at(TK::StarEq)||at(TK::SlashEq)||at(TK::PercentEq)){TK op=cur().k;++i;if(e->k!=Expr::Var)throw Error("assignment target must be a variable at line "+std::to_string(cur().line));std::string n=e->text;auto rhs=assign();auto a=std::make_unique<Expr>();a->k=Expr::Assign;a->text=n;if(op==TK::Eq)a->a=std::move(rhs);else{auto lhs=std::make_unique<Expr>();lhs->k=Expr::Var;lhs->text=n;auto b=std::make_unique<Expr>();b->k=Expr::Binary;b->text=op==TK::PlusEq?"+":op==TK::MinusEq?"-":op==TK::StarEq?"*":op==TK::SlashEq?"/":"%";b->a=std::move(lhs);b->b=std::move(rhs);a->a=std::move(b);}return a;}return e;}',
    "compound assignment parser",
)

p.write_text(s, encoding="utf-8")

p = Path("src/jau_part02a.inc")
s = p.read_text(encoding="utf-8")

s = replace_once(
    s,
    '        if(eat(TK::While)){auto s=std::make_unique<Stmt>();s->k=Stmt::WhileS;need(TK::LParen,"expected (");s->cond=expr();need(TK::RParen,"expected )");s->body=block_body();return s;}',
    '        if(eat(TK::While)){auto s=std::make_unique<Stmt>();s->k=Stmt::WhileS;need(TK::LParen,"expected (");s->cond=expr();need(TK::RParen,"expected )");s->body=block_body();return s;}\n        if(eat(TK::For)){auto s=std::make_unique<Stmt>();s->k=Stmt::ForS;need(TK::LParen,"expected ( after for");if(eat(TK::Semi)){}else if(at(TK::Let)||at(TK::Const)){bool con=false;if(eat(TK::Const))con=true;else need(TK::Let,"expected var/let/const");auto q=std::make_unique<Stmt>();q->k=Stmt::VarS;q->constant=con;q->name=need(TK::Id,"expected for variable").text;skip_type_annotation();if(eat(TK::Eq))q->expr=expr();else{q->expr=std::make_unique<Expr>();q->expr->k=Expr::Lit;}need(TK::Semi,"expected ; after for initializer");s->init=std::move(q);}else{auto q=std::make_unique<Stmt>();q->k=Stmt::ExprS;q->expr=expr();need(TK::Semi,"expected ; after for initializer");s->init=std::move(q);}if(at(TK::Semi)){s->cond=std::make_unique<Expr>();s->cond->k=Expr::Lit;s->cond->v=true;}else s->cond=expr();need(TK::Semi,"expected ; after for condition");if(!at(TK::RParen))s->step=expr();need(TK::RParen,"expected ) after for clauses");s->body=block_body();return s;}',
    "for parser",
)

s = replace_once(
    s,
    '    for(auto&s:v){optimize_expr(s->expr.get());optimize_expr(s->cond.get());optimize_stmts(s->body);optimize_stmts(s->else_body);',
    '    for(auto&s:v){optimize_expr(s->expr.get());optimize_expr(s->cond.get());optimize_expr(s->step.get());if(s->init)optimize_expr(s->init->expr.get());optimize_stmts(s->body);optimize_stmts(s->else_body);',
    "optimizer for step",
)

p.write_text(s, encoding="utf-8")

p = Path("src/jau_part02b.inc")
s = p.read_text(encoding="utf-8")
s = replace_once(
    s,
    '        case Stmt::WhileS:{int start=(int)f->code.size();break_jumps.push_back({});continue_jumps.push_back({});ex(s->cond.get());int jf=jump(Op::JumpFalse);emit(Op::Pop);for(auto&x:s->body)st(x.get());for(int j:continue_jumps.back())f->code[j].a=start;emit(Op::Jump,start);patch(jf);emit(Op::Pop);int end=(int)f->code.size();for(int j:break_jumps.back())f->code[j].a=end;break_jumps.pop_back();continue_jumps.pop_back();break;}',
    '        case Stmt::WhileS:{int start=(int)f->code.size();break_jumps.push_back({});continue_jumps.push_back({});ex(s->cond.get());int jf=jump(Op::JumpFalse);emit(Op::Pop);for(auto&x:s->body)st(x.get());for(int j:continue_jumps.back())f->code[j].a=start;emit(Op::Jump,start);patch(jf);emit(Op::Pop);int end=(int)f->code.size();for(int j:break_jumps.back())f->code[j].a=end;break_jumps.pop_back();continue_jumps.pop_back();break;}\n        case Stmt::ForS:{if(s->init)st(s->init.get());int start=(int)f->code.size();break_jumps.push_back({});continue_jumps.push_back({});ex(s->cond.get());int jf=jump(Op::JumpFalse);emit(Op::Pop);for(auto&x:s->body)st(x.get());int step=(int)f->code.size();for(int j:continue_jumps.back())f->code[j].a=step;if(s->step){ex(s->step.get());emit(Op::Pop);}emit(Op::Jump,start);patch(jf);emit(Op::Pop);int end=(int)f->code.size();for(int j:break_jumps.back())f->code[j].a=end;break_jumps.pop_back();continue_jumps.pop_back();break;}',
    "bytecode ForS",
)
p.write_text(s, encoding="utf-8")

p = Path("src/jau_part03.inc")
s = p.read_text(encoding="utf-8")
s = replace_once(
    s,
    '        else if(s->k==Stmt::WhileS){std::string a=L(),b=L();c.continues.push_back(a);c.breaks.push_back(b);o<<a<<":\\n";ex(s->cond.get());pop(is64?"rax":"eax");o<<"  cmp "<<(is64?"rax":"eax")<<",0\\n  je "<<b<<"\\n";for(auto&x:s->body)st(x.get());o<<"  jmp "<<a<<"\\n"<<b<<":\\n";c.continues.pop_back();c.breaks.pop_back();}',
    '        else if(s->k==Stmt::WhileS){std::string a=L(),b=L();c.continues.push_back(a);c.breaks.push_back(b);o<<a<<":\\n";ex(s->cond.get());pop(is64?"rax":"eax");o<<"  cmp "<<(is64?"rax":"eax")<<",0\\n  je "<<b<<"\\n";for(auto&x:s->body)st(x.get());o<<"  jmp "<<a<<"\\n"<<b<<":\\n";c.continues.pop_back();c.breaks.pop_back();}\n        else if(s->k==Stmt::ForS){if(s->init)st(s->init.get());std::string a=L(),step=L(),b=L();c.continues.push_back(step);c.breaks.push_back(b);o<<a<<":\\n";ex(s->cond.get());pop(is64?"rax":"eax");o<<"  cmp "<<(is64?"rax":"eax")<<",0\\n  je "<<b<<"\\n";for(auto&x:s->body)st(x.get());o<<step<<":\\n";if(s->step){ex(s->step.get());o<<"  add "<<(is64?"rsp,8":"esp,4")<<"\\n";}o<<"  jmp "<<a<<"\\n"<<b<<":\\n";c.continues.pop_back();c.breaks.pop_back();}',
    "AOT ForS",
)
p.write_text(s, encoding="utf-8")

Path("tests/syntax_v08.jau").write_text(
    '''/* Jau 0.8 syntax smoke test */
fn sum_to(n:int):int {
    var total:int = 0;
    for (var i:int = 0; i < n; i++) {
        if (i == 3) {
            continue;
        }
        total += i;
    }
    total -= 1;
    total++;
    return total;
}

var answer:int = sum_to(6);
print(answer);
''',
    encoding="utf-8",
)
