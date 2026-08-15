    return as_double(a)*as_double(b);
}

static std::string read_text(const fs::path& p) { std::ifstream f(p, std::ios::binary); if(!f) throw Error("cannot open " + p.string()); std::ostringstream s; s<<f.rdbuf(); return s.str(); }
static void write_text(const fs::path& p, const std::string& s) { if(p.has_parent_path()) fs::create_directories(p.parent_path()); std::ofstream f(p, std::ios::binary); if(!f) throw Error("cannot write " + p.string()); f<<s; }

static std::string shell_quote(const std::string& x) {
#ifdef _WIN32
    std::string r="\""; for(char c:x){ if(c=='\"') r+="\\\""; else r+=c; } return r+"\"";
#else
    std::string r="'"; for(char c:x){ if(c=='\'') r+="'\\''"; else r+=c; } return r+"'";
#endif
}
static std::string trim_copy(std::string x){size_t a=0,b=x.size();while(a<b&&std::isspace((unsigned char)x[a]))++a;while(b>a&&std::isspace((unsigned char)x[b-1]))--b;return x.substr(a,b-a);}
static std::string lower_copy(std::string x){for(char&c:x)c=(char)std::tolower((unsigned char)c);return x;}
static std::string upper_copy(std::string x){for(char&c:x)c=(char)std::toupper((unsigned char)c);return x;}
static std::string replace_all(std::string s,const std::string&a,const std::string&b){if(a.empty())return s;size_t p=0;while((p=s.find(a,p))!=std::string::npos){s.replace(p,a.size(),b);p+=b.size();}return s;}
static uint64_t fnv1a64(const std::string&s){uint64_t h=1469598103934665603ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}return h;}
static std::string manifest_value(const std::string& text,const std::string& key){std::istringstream in(text);std::string line;while(std::getline(in,line)){line=trim_copy(line);if(line.empty()||line[0]=='#')continue;auto q=line.find('=');if(q==std::string::npos)continue;if(trim_copy(line.substr(0,q))==key){auto v=trim_copy(line.substr(q+1));if(v.size()>=2&&v.front()=='\"'&&v.back()=='\"')v=v.substr(1,v.size()-2);return v;}}return "";}
#ifndef _WIN32
static std::string decode_chunked(const std::string& body){std::string out;size_t p=0;while(p<body.size()){auto e=body.find("\r\n",p);if(e==std::string::npos)break;std::string hs=body.substr(p,e-p);size_t semi=hs.find(';');if(semi!=std::string::npos)hs=hs.substr(0,semi);size_t n=0;std::stringstream ss;ss<<std::hex<<hs;ss>>n;p=e+2;if(n==0)break;if(p+n>body.size())throw Error("truncated chunked HTTP body");out.append(body,p,n);p+=n;if(body.substr(p,2)=="\r\n")p+=2;}return out;}
static std::string http_get_plain_posix(const std::string& url,int redirects=0){
    if(redirects>5)throw Error("too many HTTP redirects");
    if(url.rfind("http://",0)!=0)throw Error("plain HTTP transport requires http:// URL");
    std::string u=url.substr(7),hostport,path="/";auto slash=u.find('/');if(slash!=std::string::npos){hostport=u.substr(0,slash);path=u.substr(slash);}else hostport=u;
    std::string host=hostport,port="80";auto colon=hostport.rfind(':');if(colon!=std::string::npos&&hostport.find(':')==colon){host=hostport.substr(0,colon);port=hostport.substr(colon+1);}
    addrinfo hints{},*res=nullptr;hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;if(getaddrinfo(host.c_str(),port.c_str(),&hints,&res)!=0)throw Error("DNS lookup failed: "+host);
    int fd=-1;for(auto*r=res;r;r=r->ai_next){fd=::socket(r->ai_family,r->ai_socktype,r->ai_protocol);if(fd<0)continue;if(::connect(fd,r->ai_addr,r->ai_addrlen)==0)break;::close(fd);fd=-1;}freeaddrinfo(res);if(fd<0)throw Error("TCP connect failed: "+host+":"+port);
    std::string req="GET "+path+" HTTP/1.1\r\nHost: "+host+"\r\nUser-Agent: Jau/0.3\r\nAccept: */*\r\nConnection: close\r\n\r\n";size_t sent=0;while(sent<req.size()){ssize_t n=::send(fd,req.data()+sent,req.size()-sent,0);if(n<=0){::close(fd);throw Error("HTTP send failed");}sent+=(size_t)n;}
    std::string raw;std::array<char,8192>buf{};for(;;){ssize_t n=::recv(fd,buf.data(),buf.size(),0);if(n<0){::close(fd);throw Error("HTTP receive failed");}if(n==0)break;raw.append(buf.data(),(size_t)n);}::close(fd);
    auto split=raw.find("\r\n\r\n");if(split==std::string::npos)throw Error("malformed HTTP response");std::string head=raw.substr(0,split),body=raw.substr(split+4);auto line=head.find("\r\n");std::string status=head.substr(0,line);int code=0;{std::istringstream is(status);std::string proto;is>>proto>>code;}
    if(code>=300&&code<400){std::istringstream hs(head);std::string ln;while(std::getline(hs,ln)){if(!ln.empty()&&ln.back()=='\r')ln.pop_back();auto low=lower_copy(ln);if(low.rfind("location:",0)==0){auto loc=trim_copy(ln.substr(9));if(loc.rfind("http://",0)==0)return http_get_plain_posix(loc,redirects+1);throw Error("redirect requires HTTPS transport: "+loc);}}}
    if(code<200||code>=300)throw Error("HTTP status "+std::to_string(code));
    if(lower_copy(head).find("transfer-encoding: chunked")!=std::string::npos)body=decode_chunked(body);
    return body;
}
#endif
static std::string http_get_external(const std::string& url){
#ifndef _WIN32
    if(url.rfind("http://",0)==0)return http_get_plain_posix(url);
#endif
    fs::path tmp=fs::temp_directory_path()/("jau_http_"+std::to_string(fnv1a64(url))+".tmp");
#ifdef _WIN32
    std::string cmd="powershell -NoProfile -ExecutionPolicy Bypass -Command \"$ProgressPreference='SilentlyContinue';Invoke-WebRequest -UseBasicParsing -Uri '"+replace_all(url,"'","''")+"' -OutFile '"+replace_all(tmp.string(),"'","''")+"'\"";
#else
    std::string cmd="curl -LfsS --connect-timeout 15 --max-time 60 "+shell_quote(url)+" -o "+shell_quote(tmp.string());
#endif
    int rc=std::system(cmd.c_str()); if(rc!=0) throw Error("HTTP download failed for "+url+" (platform HTTPS transport unavailable or request failed)");
    auto out=read_text(tmp); std::error_code ec;fs::remove(tmp,ec);return out;
}


static fs::path jau_home_path(){if(const char* home=std::getenv("JAU_HOME"))return fs::path(home);return fs::current_path()/".jau";}
static std::string expand_imports_impl(const fs::path& file,const CompileOptions& opts,std::unordered_set<std::string>& seen);
static std::string expand_package_impl(const fs::path& archive,const std::string& rel,const CompileOptions& opts,std::unordered_set<std::string>& seen);

static std::string expand_package_spec(const std::string& spec,const CompileOptions& opts,std::unordered_set<std::string>& seen){
    auto slash=spec.find('/');std::string pkg=slash==std::string::npos?spec:spec.substr(0,slash);std::string sub=slash==std::string::npos?"":spec.substr(slash+1);
    fs::path dir=jau_home_path()/"packages"/pkg,archive=dir/"package.jaup";
    if(fs::exists(archive)){
        if(sub.empty()){auto mf=package_manifest(archive.string());sub=manifest_value(mf,"main");if(sub.empty())sub="main.jau";}
        return expand_package_impl(archive,sub,opts,seen);
    }
    // Compatibility with source-installed JauPM 0.3/0.4 packages.
    if(sub.empty())sub="main.jau";fs::path legacy=dir/sub;if(fs::exists(legacy))return expand_imports_impl(legacy,opts,seen);
    throw Error("package not installed: "+pkg+" (expected "+archive.string()+")");
}
static fs::path resolve_external_import(const fs::path& origin,const fs::path& rel,const CompileOptions& opts){
    std::vector<fs::path> cands;cands.push_back(origin.parent_path()/rel);for(auto&ip:opts.import_paths)cands.push_back(fs::path(ip)/rel);cands.push_back(jau_home_path()/"stdlib"/rel);cands.push_back(fs::current_path()/"stdlib"/rel);
    for(auto&c:cands)if(fs::exists(c))return c;return {};
}
static std::string expand_package_impl(const fs::path& archive,const std::string& rel,const CompileOptions& opts,std::unordered_set<std::string>& seen){
    fs::path normalized=fs::path(rel).lexically_normal();std::string rs=normalized.generic_string();std::string key="jaup:"+fs::absolute(archive).lexically_normal().string()+"#"+rs;if(seen.count(key))return "";seen.insert(key);
    std::string src=package_read_file(archive.string(),rs),out;std::regex re(R"(^\s*import\s+\"([^\"]+)\"\s*;?\s*$)");std::istringstream in(src);std::string line;
    while(std::getline(in,line)){std::smatch m;if(std::regex_match(line,m,re)){std::string imp=m[1].str();if(imp.rfind("pkg:",0)==0){out+=expand_package_spec(imp.substr(4),opts,seen);out+='\n';continue;}
            fs::path inside=(normalized.parent_path()/fs::path(imp)).lexically_normal();bool found_inside=false;try{out+=expand_package_impl(archive,inside.generic_string(),opts,seen);found_inside=true;}catch(const std::exception&e){std::string msg=e.what();if(msg.find("package file not found:")==std::string::npos)throw;}
            if(found_inside){out+='\n';continue;}fs::path found=resolve_external_import(fs::current_path()/"__package_virtual__.jau",imp,opts);if(found.empty())throw Error("import not found in package "+archive.filename().string()+": "+imp);out+=expand_imports_impl(found,opts,seen);out+='\n';
        }else{out+=line;out+='\n';}}
    return out;
}
static std::string expand_imports_impl(const fs::path& file,const CompileOptions& opts,std::unordered_set<std::string>& seen){
    auto abs=fs::weakly_canonical(file).string();if(seen.count(abs))return "";seen.insert(abs);std::string src=read_text(file),out;std::regex re(R"(^\s*import\s+\"([^\"]+)\"\s*;?\s*$)");std::istringstream in(src);std::string line;
    while(std::getline(in,line)){std::smatch m;if(std::regex_match(line,m,re)){std::string imp=m[1].str();if(imp.rfind("pkg:",0)==0){out+=expand_package_spec(imp.substr(4),opts,seen);out+='\n';continue;}fs::path found=resolve_external_import(file,imp,opts);if(found.empty())throw Error("import not found: "+imp);out+=expand_imports_impl(found,opts,seen);out+='\n';}else{out+=line;out+='\n';}}
    return out;
}
static std::string expand_imports(const fs::path& f,const CompileOptions&o){std::unordered_set<std::string>s;return expand_imports_impl(f,o,s);}

enum class TK { End, Id, Num, Str, LParen,RParen,LBrace,RBrace,LBracket,RBracket,Comma,Semi,Dot,Plus,Minus,Star,Slash,Percent,Bang,Eq,Lt,Gt,EqEq,Ne,Le,Ge,AndAnd,OrOr,
    Func,Namespace,If,Else,While,Return,Break,Continue,Let,Const,True,False,Null };
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
            static const std::unordered_map<std::string,TK> kw={{"func",TK::Func},{"namespace",TK::Namespace},{"if",TK::If},{"else",TK::Else},{"while",TK::While},{"return",TK::Return},{"break",TK::Break},{"continue",TK::Continue},{"let",TK::Let},{"const",TK::Const},{"true",TK::True},{"false",TK::False},{"null",TK::Null}};
            auto it=kw.find(x); t.push_back({it==kw.end()?TK::Id:it->second,x,ln}); continue;}
        if(std::isdigit((unsigned char)c)){std::string x;bool dot=false;while(std::isdigit((unsigned char)peek())||(!dot&&peek()=='.')){if(peek()=='.')dot=true;x+=get();}t.push_back({TK::Num,x,ln});continue;}
        if(c=='\"'){get();std::string x;while(peek()&&peek()!='\"'){char q=get();if(q=='\\'){char e=get();if(e=='n')x+='\n';else if(e=='t')x+='\t';else if(e=='r')x+='\r';else x+=e;}else x+=q;}if(get()!='\"')throw Error("unterminated string at line "+std::to_string(ln));t.push_back({TK::Str,x,ln});continue;}
        auto two=[&](char a,char b,TK kk){if(peek()==a&&peek(1)==b){get();get();t.push_back({kk,std::string()+a+b,ln});return true;}return false;};
        if(two('=','=',TK::EqEq)||two('!','=',TK::Ne)||two('<','=',TK::Le)||two('>','=',TK::Ge)||two('&','&',TK::AndAnd)||two('|','|',TK::OrOr))continue;
        get(); TK k; switch(c){case '(':k=TK::LParen;break;case ')':k=TK::RParen;break;case '{':k=TK::LBrace;break;case '}':k=TK::RBrace;break;case '[':k=TK::LBracket;break;case ']':k=TK::RBracket;break;case ',':k=TK::Comma;break;case ';':k=TK::Semi;break;case '.':k=TK::Dot;break;case '+':k=TK::Plus;break;case '-':k=TK::Minus;break;case '*':k=TK::Star;break;case '/':k=TK::Slash;break;case '%':k=TK::Percent;break;case '!':k=TK::Bang;break;case '=':k=TK::Eq;break;case '<':k=TK::Lt;break;case '>':k=TK::Gt;break;default:throw Error("unexpected character at line "+std::to_string(ln));}t.push_back({k,std::string(1,c),ln});
    }return t;}
};

struct Expr { enum K{Lit,Var,Assign,Binary,Unary,Call,ArrayLit,Index} k; Value v; std::string text; std::unique_ptr<Expr>a,b; std::vector<std::unique_ptr<Expr>> args; };
struct Stmt { enum K{ExprS,VarS,Block,IfS,WhileS,ReturnS,BreakS,ContinueS,FuncS} k; std::string name; bool constant=false; std::unique_ptr<Expr> expr,cond; std::vector<std::unique_ptr<Stmt>> body,else_body; std::vector<std::string> params; };

class Parser {
    std::vector<Tok> t; size_t i=0;
    Tok& cur(){return t[i];} bool at(TK k){return cur().k==k;} bool eat(TK k){if(at(k)){++i;return true;}return false;}
    Tok need(TK k,const char* m){if(!at(k))throw Error(std::string(m)+" at line "+std::to_string(cur().line));return t[i++];}
    std::string current_namespace;
    std::string qualified_id(){std::string n=need(TK::Id,"expected identifier").text;while(eat(TK::Dot))n+="."+need(TK::Id,"expected identifier after .").text;return n;}
    std::unique_ptr<Expr> primary(){
        if(eat(TK::True)){auto e=std::make_unique<Expr>();e->k=Expr::Lit;e->v=true;return e;} if(eat(TK::False)){auto e=std::make_unique<Expr>();e->k=Expr::Lit;e->v=false;return e;} if(eat(TK::Null)){auto e=std::make_unique<Expr>();e->k=Expr::Lit;return e;}
        if(at(TK::Num)){auto z=cur();++i;auto e=std::make_unique<Expr>();e->k=Expr::Lit;e->v=z.text.find('.')==std::string::npos?Value((int64_t)std::stoll(z.text)):Value(std::stod(z.text));return e;}
        if(at(TK::Str)){auto z=cur();++i;auto e=std::make_unique<Expr>();e->k=Expr::Lit;e->v=z.text;return e;}
        if(at(TK::Id)){auto e=std::make_unique<Expr>();e->k=Expr::Var;e->text=qualified_id();return e;}
        if(eat(TK::LBracket)){auto e=std::make_unique<Expr>();e->k=Expr::ArrayLit;if(!at(TK::RBracket)){do{e->args.push_back(expr());}while(eat(TK::Comma));}need(TK::RBracket,"expected ]");return e;}
        if(eat(TK::LParen)){auto e=expr();need(TK::RParen,"expected )");return e;}
        throw Error("expected expression at line "+std::to_string(cur().line));
    }
    std::unique_ptr<Expr> call(){auto e=primary();for(;;){if(eat(TK::LParen)){auto c=std::make_unique<Expr>();c->k=Expr::Call;c->a=std::move(e);if(!at(TK::RParen)){do{c->args.push_back(expr());}while(eat(TK::Comma));}need(TK::RParen,"expected )");e=std::move(c);continue;}if(eat(TK::LBracket)){if(e->k==Expr::Var&&e->text.find('.')!=std::string::npos){auto c=std::make_unique<Expr>();c->k=Expr::Call;c->a=std::move(e);if(!at(TK::RBracket)){do{c->args.push_back(expr());}while(eat(TK::Comma));}need(TK::RBracket,"expected ]");e=std::move(c);}else{auto n=std::make_unique<Expr>();n->k=Expr::Index;n->a=std::move(e);n->b=expr();need(TK::RBracket,"expected ]");e=std::move(n);}continue;}break;}return e;}
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
        if(eat(TK::Func)){auto s=std::make_unique<Stmt>();s->k=Stmt::FuncS;s->name=qualified_id();if(!current_namespace.empty()&&s->name.find('.')==std::string::npos)s->name=current_namespace+"."+s->name;need(TK::LParen,"expected (");if(!at(TK::RParen)){do{s->params.push_back(need(TK::Id,"expected parameter").text);}while(eat(TK::Comma));}need(TK::RParen,"expected )");s->body=block_body();return s;}
        if(eat(TK::Let)||eat(TK::Const)){bool con=t[i-1].k==TK::Const;auto s=std::make_unique<Stmt>();s->k=Stmt::VarS;s->constant=con;s->name=need(TK::Id,"expected variable name").text;if(eat(TK::Eq))s->expr=expr();else{s->expr=std::make_unique<Expr>();s->expr->k=Expr::Lit;}eat(TK::Semi);return s;}
