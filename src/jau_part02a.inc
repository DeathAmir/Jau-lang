        if(eat(TK::If)){auto s=std::make_unique<Stmt>();s->k=Stmt::IfS;need(TK::LParen,"expected (");s->cond=expr();need(TK::RParen,"expected )");s->body=block_body();if(eat(TK::Else)){if(at(TK::If)){s->else_body.push_back(stmt());}else s->else_body=block_body();}return s;}
        if(eat(TK::While)){auto s=std::make_unique<Stmt>();s->k=Stmt::WhileS;need(TK::LParen,"expected (");s->cond=expr();need(TK::RParen,"expected )");s->body=block_body();return s;}
        if(eat(TK::For)){auto s=std::make_unique<Stmt>();s->k=Stmt::ForS;need(TK::LParen,"expected ( after for");if(eat(TK::Semi)){}else if(at(TK::Let)||at(TK::Const)){bool con=false;if(eat(TK::Const))con=true;else need(TK::Let,"expected var/let/const");auto q=std::make_unique<Stmt>();q->k=Stmt::VarS;q->constant=con;q->name=need(TK::Id,"expected for variable").text;skip_type_annotation();if(eat(TK::Eq))q->expr=expr();else{q->expr=std::make_unique<Expr>();q->expr->k=Expr::Lit;}need(TK::Semi,"expected ; after for initializer");s->init=std::move(q);}else{auto q=std::make_unique<Stmt>();q->k=Stmt::ExprS;q->expr=expr();need(TK::Semi,"expected ; after for initializer");s->init=std::move(q);}if(at(TK::Semi)){s->cond=std::make_unique<Expr>();s->cond->k=Expr::Lit;s->cond->v=true;}else s->cond=expr();need(TK::Semi,"expected ; after for condition");if(!at(TK::RParen))s->step=expr();need(TK::RParen,"expected ) after for clauses");s->body=block_body();return s;}
        if(eat(TK::Return)){auto s=std::make_unique<Stmt>();s->k=Stmt::ReturnS;if(!at(TK::Semi)&&!at(TK::RBrace))s->expr=expr();eat(TK::Semi);return s;}
        if(eat(TK::Break)){auto s=std::make_unique<Stmt>();s->k=Stmt::BreakS;eat(TK::Semi);return s;}
        if(eat(TK::Continue)){auto s=std::make_unique<Stmt>();s->k=Stmt::ContinueS;eat(TK::Semi);return s;}
        if(at(TK::LBrace)){auto s=std::make_unique<Stmt>();s->k=Stmt::Block;s->body=block_body();return s;}
        auto s=std::make_unique<Stmt>();s->k=Stmt::ExprS;s->expr=expr();eat(TK::Semi);return s;
    }
    void parse_items(std::vector<std::unique_ptr<Stmt>>&v,bool until_brace){while(!at(TK::End)&&!(until_brace&&at(TK::RBrace))){if(eat(TK::Namespace)){std::string n=qualified_id();need(TK::LBrace,"expected { after namespace");std::string old=current_namespace;current_namespace=old.empty()?n:old+"."+n;parse_items(v,true);need(TK::RBrace,"expected } after namespace");eat(TK::Semi);current_namespace=old;}else v.push_back(stmt());}}
public:
    explicit Parser(std::vector<Tok>x):t(std::move(x)){}
    std::vector<std::unique_ptr<Stmt>> parse(){std::vector<std::unique_ptr<Stmt>> v;parse_items(v,false);return v;}
};

enum class Op:uint8_t { Const,Null,LoadG,StoreG,LoadL,StoreL,Pop,Add,Sub,Mul,Div,Mod,Neg,Not,Eq,Ne,Lt,Le,Gt,Ge,And,Or,Jump,JumpFalse,MakeArray,Index,Call,Ret,Halt };
struct Ins { Op op; int32_t a=0; };
struct Fun { std::string name; int32_t arity=0, locals=0; std::vector<Ins> code; };
struct Program { std::vector<Value> consts; std::vector<std::string> globals; std::vector<Fun> funcs; };

static std::optional<Value> fold_binary(const std::string&op,const Value&a,const Value&b){try{
    if(op=="+") return addv(a,b);
    if(op=="-") return subv(a,b);
    if(op=="*") return mulv(a,b);
    if(op=="/") return as_double(a)/as_double(b);
    if(op=="%") return as_int(a)%as_int(b);
    if(op=="==") return a.str()==b.str();
    if(op=="!=") return a.str()!=b.str();
    if(op=="<") return as_double(a)<as_double(b);
    if(op=="<=") return as_double(a)<=as_double(b);
    if(op==">") return as_double(a)>as_double(b);
    if(op==">=") return as_double(a)>=as_double(b);
    if(op=="&&") return a.truthy()&&b.truthy();
    if(op=="||") return a.truthy()||b.truthy();
}catch(...){return std::nullopt;}return std::nullopt;}
static bool lit_i(Expr*e,int64_t v){return e&&e->k==Expr::Lit&&std::holds_alternative<int64_t>(e->v.data)&&std::get<int64_t>(e->v.data)==v;}
static void become_lit(Expr*e,Value v){e->k=Expr::Lit;e->v=std::move(v);e->a.reset();e->b.reset();e->args.clear();e->text.clear();}
static void optimize_expr(Expr*e){
    if(!e)return;
    optimize_expr(e->a.get()); optimize_expr(e->b.get()); for(auto&x:e->args)optimize_expr(x.get());
    if(e->k==Expr::Unary&&e->a&&e->a->k==Expr::Lit){try{if(e->text=="-"){if(std::holds_alternative<int64_t>(e->a->v.data))become_lit(e,Value(-std::get<int64_t>(e->a->v.data)));else become_lit(e,Value(-as_double(e->a->v)));}else become_lit(e,Value(!e->a->v.truthy()));}catch(...){}return;}
    if(e->k!=Expr::Binary||!e->a||!e->b)return;
    if(e->a->k==Expr::Lit&&e->b->k==Expr::Lit){auto v=fold_binary(e->text,e->a->v,e->b->v);if(v){become_lit(e,*v);return;}}
    if(e->text=="+"&&lit_i(e->b.get(),0)){auto n=std::move(e->a);*e=std::move(*n);return;}
    if(e->text=="-"&&lit_i(e->b.get(),0)){auto n=std::move(e->a);*e=std::move(*n);return;}
    if(e->text=="*"&&lit_i(e->b.get(),1)){auto n=std::move(e->a);*e=std::move(*n);return;}
    if(e->text=="*"&&lit_i(e->a.get(),1)){auto n=std::move(e->b);*e=std::move(*n);return;}
    if(e->text=="*"&&(lit_i(e->a.get(),0)||lit_i(e->b.get(),0))){become_lit(e,Value((int64_t)0));return;}
}
static void optimize_stmts(std::vector<std::unique_ptr<Stmt>>&v){
    for(auto&s:v){optimize_expr(s->expr.get());optimize_expr(s->cond.get());optimize_expr(s->step.get());if(s->init)optimize_expr(s->init->expr.get());optimize_stmts(s->body);optimize_stmts(s->else_body);
        if(s->k==Stmt::IfS&&s->cond&&s->cond->k==Expr::Lit){bool take=s->cond->v.truthy();s->k=Stmt::Block;s->body=take?std::move(s->body):std::move(s->else_body);s->else_body.clear();s->cond.reset();}
        else if(s->k==Stmt::WhileS&&s->cond&&s->cond->k==Expr::Lit&&!s->cond->v.truthy()){s->k=Stmt::Block;s->body.clear();s->cond.reset();}
    }
    for(size_t i=0;i<v.size();++i)if(v[i]->k==Stmt::ReturnS||v[i]->k==Stmt::BreakS||v[i]->k==Stmt::ContinueS){v.erase(v.begin()+i+1,v.end());break;}
}

class BC {
    Program p; std::unordered_map<std::string,int> fidx,gidx; std::unordered_set<std::string> externs; Fun* f=nullptr; bool top=true; std::unordered_map<std::string,int> locals; int nextlocal=0;
    std::unordered_set<std::string> const_globals,const_locals;
    std::vector<std::vector<int>> break_jumps, continue_jumps;
    int c(Value v){p.consts.push_back(std::move(v));return (int)p.consts.size()-1;}
    int g(const std::string&n){auto it=gidx.find(n);if(it!=gidx.end())return it->second;int x=(int)p.globals.size();p.globals.push_back(n);gidx[n]=x;return x;}
    void emit(Op o,int a=0){f->code.push_back({o,a});}
    int jump(Op o){int x=(int)f->code.size();emit(o,-1);return x;} void patch(int x){f->code[x].a=(int)f->code.size();}
    void ex(Expr*e){if(!e){emit(Op::Null);return;}switch(e->k){
        case Expr::Lit: if(std::holds_alternative<std::monostate>(e->v.data))emit(Op::Null);else emit(Op::Const,c(e->v));break;
        case Expr::Var:{auto it=locals.find(e->text);if(!top&&it!=locals.end())emit(Op::LoadL,it->second);else emit(Op::LoadG,g(e->text));break;}
