        if(eat(TK::If)){auto s=std::make_unique<Stmt>();s->k=Stmt::IfS;need(TK::LParen,"expected (");s->cond=expr();need(TK::RParen,"expected )");s->body=block_body();if(eat(TK::Else)){if(at(TK::If)){s->else_body.push_back(stmt());}else s->else_body=block_body();}return s;}
        if(eat(TK::While)){auto s=std::make_unique<Stmt>();s->k=Stmt::WhileS;need(TK::LParen,"expected (");s->cond=expr();need(TK::RParen,"expected )");s->body=block_body();return s;}
        if(eat(TK::Return)){auto s=std::make_unique<Stmt>();s->k=Stmt::ReturnS;if(!at(TK::Semi)&&!at(TK::RBrace))s->expr=expr();eat(TK::Semi);return s;}
        if(at(TK::LBrace)){auto s=std::make_unique<Stmt>();s->k=Stmt::Block;s->body=block_body();return s;}
        auto s=std::make_unique<Stmt>();s->k=Stmt::ExprS;s->expr=expr();eat(TK::Semi);return s;
    }
public:
    explicit Parser(std::vector<Tok>x):t(std::move(x)){}
    std::vector<std::unique_ptr<Stmt>> parse(){std::vector<std::unique_ptr<Stmt>> v;while(!at(TK::End))v.push_back(stmt());return v;}
};

enum class Op:uint8_t { Const,Null,LoadG,StoreG,LoadL,StoreL,Pop,Add,Sub,Mul,Div,Mod,Neg,Not,Eq,Ne,Lt,Le,Gt,Ge,And,Or,Jump,JumpFalse,Call,Ret,Halt };
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
static void optimize_expr(Expr*e){if(!e)return;optimize_expr(e->a.get());optimize_expr(e->b.get());for(auto&x:e->args)optimize_expr(x.get());if(e->k==Expr::Binary&&e->a&&e->b&&e->a->k==Expr::Lit&&e->b->k==Expr::Lit){auto v=fold_binary(e->text,e->a->v,e->b->v);if(v){e->k=Expr::Lit;e->v=*v;e->a.reset();e->b.reset();e->text.clear();}}}
static void optimize_stmts(std::vector<std::unique_ptr<Stmt>>&v){for(auto&s:v){optimize_expr(s->expr.get());optimize_expr(s->cond.get());optimize_stmts(s->body);optimize_stmts(s->else_body);}}

class BC {
    Program p; std::unordered_map<std::string,int> fidx,gidx; Fun* f=nullptr; bool top=true; std::unordered_map<std::string,int> locals; int nextlocal=0;
    int c(Value v){p.consts.push_back(std::move(v));return (int)p.consts.size()-1;}
    int g(const std::string&n){auto it=gidx.find(n);if(it!=gidx.end())return it->second;int x=(int)p.globals.size();p.globals.push_back(n);gidx[n]=x;return x;}
    void emit(Op o,int a=0){f->code.push_back({o,a});}
    int jump(Op o){int x=(int)f->code.size();emit(o,-1);return x;} void patch(int x){f->code[x].a=(int)f->code.size();}
    void ex(Expr*e){if(!e){emit(Op::Null);return;}switch(e->k){
        case Expr::Lit: if(std::holds_alternative<std::monostate>(e->v.data))emit(Op::Null);else emit(Op::Const,c(e->v));break;
        case Expr::Var:{auto it=locals.find(e->text);if(!top&&it!=locals.end())emit(Op::LoadL,it->second);else emit(Op::LoadG,g(e->text));break;}
