    return as_double(a)*as_double(b);
}

static std::string read_text(const fs::path& p) { std::ifstream f(p, std::ios::binary); if(!f) throw Error("cannot open " + p.string()); std::ostringstream s; s<<f.rdbuf(); return s.str(); }
static void write_text(const fs::path& p, const std::string& s) { if(p.has_parent_path()) fs::create_directories(p.parent_path()); std::ofstream f(p, std::ios::binary); if(!f) throw Error("cannot write " + p.string()); f<<s; }

static std::string expand_imports_impl(const fs::path& file, const CompileOptions& opts, std::unordered_set<std::string>& seen) {
    auto abs = fs::weakly_canonical(file).string();
    if (seen.count(abs)) return "";
    seen.insert(abs);
    std::string src = read_text(file), out;
    std::regex re(R"(^\s*import\s+\"([^\"]+)\"\s*;?\s*$)");
    std::istringstream in(src); std::string line;
    while(std::getline(in,line)) {
        std::smatch m;
        if(std::regex_match(line,m,re)) {
            fs::path rel = m[1].str(), found;
            std::vector<fs::path> cands = {file.parent_path()/rel};
            for(auto& ip:opts.import_paths) cands.push_back(fs::path(ip)/rel);
            if(const char* home=std::getenv("JAU_HOME")) cands.push_back(fs::path(home)/"stdlib"/rel);
            cands.push_back(fs::current_path()/"stdlib"/rel);
            for(auto& c:cands) if(fs::exists(c)){found=c;break;}
            if(found.empty()) throw Error("import not found: "+rel.string());
            out += expand_imports_impl(found,opts,seen); out += "\n";
        } else { out += line; out += '\n'; }
    }
    return out;
}
static std::string expand_imports(const fs::path& f, const CompileOptions&o){std::unordered_set<std::string>s;return expand_imports_impl(f,o,s);}

enum class TK { End, Id, Num, Str, LParen,RParen,LBrace,RBrace,Comma,Semi,Plus,Minus,Star,Slash,Percent,Bang,Eq,Lt,Gt,EqEq,Ne,Le,Ge,AndAnd,OrOr,
    Func,If,Else,While,Return,Let,Const,True,False,Null };
struct Tok { TK k; std::string text; int line; };
class Lexer {
    std::string s; size_t i=0; int line=1;
    char peek(size_t n=0)const{return i+n<s.size()?s[i+n]:'\0';}
    char get(){char c=peek(); if(c){++i;if(c=='\n')++line;}return c;}
public:
    explicit Lexer(std::string x):s(std::move(x)){}
    std::vector<Tok> scan(){std::vector<Tok> t; for(;;){
        while(std::isspace((unsigned char)peek()))get();
        if(peek()=='/'&&peek(1)=='/'){while(peek()&&peek()!='\n')get();continue;}
        if(peek()=='#'){while(peek()&&peek()!='\n')get();continue;}
        int ln=line; char c=peek(); if(!c){t.push_back({TK::End,"",ln});break;}
        if(std::isalpha((unsigned char)c)||c=='_'){std::string x;while(std::isalnum((unsigned char)peek())||peek()=='_')x+=get();
            static const std::unordered_map<std::string,TK> kw={{"func",TK::Func},{"if",TK::If},{"else",TK::Else},{"while",TK::While},{"return",TK::Return},{"let",TK::Let},{"const",TK::Const},{"true",TK::True},{"false",TK::False},{"null",TK::Null}};
            auto it=kw.find(x); t.push_back({it==kw.end()?TK::Id:it->second,x,ln}); continue;}
        if(std::isdigit((unsigned char)c)){std::string x;bool dot=false;while(std::isdigit((unsigned char)peek())||(!dot&&peek()=='.')){if(peek()=='.')dot=true;x+=get();}t.push_back({TK::Num,x,ln});continue;}
        if(c=='"'){get();std::string x;while(peek()&&peek()!='"'){char q=get();if(q=='\\'){char e=get();if(e=='n')x+='\n';else if(e=='t')x+='\t';else if(e=='r')x+='\r';else x+=e;}else x+=q;}if(get()!='"')throw Error("unterminated string at line "+std::to_string(ln));t.push_back({TK::Str,x,ln});continue;}
        auto two=[&](char a,char b,TK kk){if(peek()==a&&peek(1)==b){get();get();t.push_back({kk,std::string()+a+b,ln});return true;}return false;};
        if(two('=','=',TK::EqEq)||two('!','=',TK::Ne)||two('<','=',TK::Le)||two('>','=',TK::Ge)||two('&','&',TK::AndAnd)||two('|','|',TK::OrOr))continue;
        get(); TK k; switch(c){case '(':k=TK::LParen;break;case ')':k=TK::RParen;break;case '{':k=TK::LBrace;break;case '}':k=TK::RBrace;break;case ',':k=TK::Comma;break;case ';':k=TK::Semi;break;case '+':k=TK::Plus;break;case '-':k=TK::Minus;break;case '*':k=TK::Star;break;case '/':k=TK::Slash;break;case '%':k=TK::Percent;break;case '!':k=TK::Bang;break;case '=':k=TK::Eq;break;case '<':k=TK::Lt;break;case '>':k=TK::Gt;break;default:throw Error("unexpected character at line "+std::to_string(ln));}t.push_back({k,std::string(1,c),ln});
    }return t;}
};

struct Expr { enum K{Lit,Var,Assign,Binary,Unary,Call} k; Value v; std::string text; std::unique_ptr<Expr>a,b; std::vector<std::unique_ptr<Expr>> args; };
struct Stmt { enum K{ExprS,VarS,Block,IfS,WhileS,ReturnS,FuncS} k; std::string name; bool constant=false; std::unique_ptr<Expr> expr,cond; std::vector<std::unique_ptr<Stmt>> body,else_body; std::vector<std::string> params; };

class Parser {
    std::vector<Tok> t; size_t i=0;
    Tok& cur(){return t[i];} bool at(TK k){return cur().k==k;} bool eat(TK k){if(at(k)){++i;return true;}return false;}
    Tok need(TK k,const char* m){if(!at(k))throw Error(std::string(m)+" at line "+std::to_string(cur().line));return t[i++];}
    std::unique_ptr<Expr> primary(){
        if(eat(TK::True)){auto e=std::make_unique<Expr>();e->k=Expr::Lit;e->v=true;return e;} if(eat(TK::False)){auto e=std::make_unique<Expr>();e->k=Expr::Lit;e->v=false;return e;} if(eat(TK::Null)){auto e=std::make_unique<Expr>();e->k=Expr::Lit;return e;}
        if(at(TK::Num)){auto z=cur();++i;auto e=std::make_unique<Expr>();e->k=Expr::Lit;e->v=z.text.find('.')==std::string::npos?Value((int64_t)std::stoll(z.text)):Value(std::stod(z.text));return e;}
        if(at(TK::Str)){auto z=cur();++i;auto e=std::make_unique<Expr>();e->k=Expr::Lit;e->v=z.text;return e;}
        if(at(TK::Id)){auto z=cur();++i;auto e=std::make_unique<Expr>();e->k=Expr::Var;e->text=z.text;return e;}
        if(eat(TK::LParen)){auto e=expr();need(TK::RParen,"expected )");return e;}
        throw Error("expected expression at line "+std::to_string(cur().line));
    }
    std::unique_ptr<Expr> call(){auto e=primary();while(eat(TK::LParen)){auto c=std::make_unique<Expr>();c->k=Expr::Call;c->a=std::move(e);if(!at(TK::RParen)){do{c->args.push_back(expr());}while(eat(TK::Comma));}need(TK::RParen,"expected )");e=std::move(c);}return e;}
    std::unique_ptr<Expr> unary(){if(at(TK::Bang)||at(TK::Minus)){std::string op=cur().text;++i;auto e=std::make_unique<Expr>();e->k=Expr::Unary;e->text=op;e->a=unary();return e;}return call();}
    template<class Next> std::unique_ptr<Expr> bin(Next next,std::initializer_list<TK> ks){auto e=(this->*next)();for(;;){bool m=false;for(auto k:ks)if(at(k)){m=true;break;}if(!m)break;std::string op=cur().text;++i;auto n=std::make_unique<Expr>();n->k=Expr::Binary;n->text=op;n->a=std::move(e);n->b=(this->*next)();e=std::move(n);}return e;}
    std::unique_ptr<Expr> factor(){return bin(&Parser::unary,{TK::Star,TK::Slash,TK::Percent});}
    std::unique_ptr<Expr> term(){return bin(&Parser::factor,{TK::Plus,TK::Minus});}
    std::unique_ptr<Expr> cmp(){return bin(&Parser::term,{TK::Lt,TK::Le,TK::Gt,TK::Ge});}
    std::unique_ptr<Expr> equality(){return bin(&Parser::cmp,{TK::EqEq,TK::Ne});}
    std::unique_ptr<Expr> land(){return bin(&Parser::equality,{TK::AndAnd});}
    std::unique_ptr<Expr> lor(){return bin(&Parser::land,{TK::OrOr});}
    std::unique_ptr<Expr> assign(){auto e=lor();if(eat(TK::Eq)){if(e->k!=Expr::Var)throw Error("invalid assignment target");auto n=std::make_unique<Expr>();n->k=Expr::Assign;n->text=e->text;n->a=assign();return n;}return e;}
    std::unique_ptr<Expr> expr(){return assign();}
    std::vector<std::unique_ptr<Stmt>> block_body(){need(TK::LBrace,"expected {");std::vector<std::unique_ptr<Stmt>> v;while(!at(TK::RBrace)&&!at(TK::End))v.push_back(stmt());need(TK::RBrace,"expected }");return v;}
    std::unique_ptr<Stmt> stmt(){
        if(eat(TK::Func)){auto s=std::make_unique<Stmt>();s->k=Stmt::FuncS;s->name=need(TK::Id,"expected function name").text;need(TK::LParen,"expected (");if(!at(TK::RParen)){do{s->params.push_back(need(TK::Id,"expected parameter").text);}while(eat(TK::Comma));}need(TK::RParen,"expected )");s->body=block_body();return s;}
        if(eat(TK::Let)||eat(TK::Const)){bool con=t[i-1].k==TK::Const;auto s=std::make_unique<Stmt>();s->k=Stmt::VarS;s->constant=con;s->name=need(TK::Id,"expected variable name").text;if(eat(TK::Eq))s->expr=expr();else{s->expr=std::make_unique<Expr>();s->expr->k=Expr::Lit;}eat(TK::Semi);return s;}
