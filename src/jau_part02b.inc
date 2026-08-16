        case Expr::Assign:{if((!top&&const_locals.count(e->text))||(top&&const_globals.count(e->text)))throw Error("cannot assign to const: "+e->text);ex(e->a.get());auto it=locals.find(e->text);if(!top&&it!=locals.end())emit(Op::StoreL,it->second);else emit(Op::StoreG,g(e->text));break;}
        case Expr::Unary:ex(e->a.get());emit(e->text=="-"?Op::Neg:e->text=="~"?Op::BitNot:Op::Not);break;
        case Expr::Binary:ex(e->a.get());ex(e->b.get());{static const std::unordered_map<std::string,Op>m={{"+",Op::Add},{"-",Op::Sub},{"*",Op::Mul},{"/",Op::Div},{"%",Op::Mod},{"==",Op::Eq},{"!=",Op::Ne},{"<",Op::Lt},{"<=",Op::Le},{">",Op::Gt},{">=",Op::Ge},{"&&",Op::And},{"||",Op::Or},{"&",Op::BitAnd},{"|",Op::BitOr},{"^",Op::BitXor},{"<<",Op::Shl},{">>",Op::Shr}};emit(m.at(e->text));}break;
        case Expr::ArrayLit:{for(auto&x:e->args)ex(x.get());emit(Op::MakeArray,(int)e->args.size());break;}
        case Expr::Index:ex(e->a.get());ex(e->b.get());emit(Op::Index);break;
        case Expr::Call:{if(!e->a||e->a->k!=Expr::Var)throw Error("only named calls are supported");for(auto&x:e->args)ex(x.get());std::string n=e->a->text;static const std::unordered_map<std::string,int> bi={{"print",-1},{"clock",-2},{"len",-3},{"str",-4},{"int",-5},{"read_file",-6},{"write_file",-7},{"contains",-8},{"starts_with",-9},{"argc",-10},{"arg",-11},{"getenv",-12},{"file_exists",-13},{"mkdir",-14},{"remove_file",-15},{"path_join",-16},{"cwd",-17},{"http_get",-18},{"download",-19},{"substr",-20},{"char_at",-21},{"find",-22},{"trim",-23},{"upper",-24},{"lower",-25},{"replace",-26},{"manifest_value",-27},{"hash64",-28},{"push",-29},{"pop",-30},{"join",-31},{"split",-32},{"read_line",-33},{"sleep_ms",-34},{"time.sleep_ms",-34},{"platform",-35},{"arch",-36},{"temp_dir",-37},{"random_int",-38},{"list_dir",-39},{"file_size",-40},{"remove_tree",-41},{"jaup_pack",-42},{"jaup_extract",-43},{"jaup_manifest",-44},{"jaup_verify",-45},{"clock_ms",-46},{"time.now_ms",-46},{"clock_ns",-47},{"time.now_ns",-47}};auto b=bi.find(n);if(b!=bi.end())emit(Op::Call,b->second);else{auto it=fidx.find(n);if(it==fidx.end()&&n.find('.')==std::string::npos&&!current_ns.empty())it=fidx.find(current_ns+"."+n);if(it==fidx.end()){if(externs.count(n))throw Error("extern function is AOT/object-only: "+n);throw Error("unknown function: "+n);}emit(Op::Call,it->second);}break;}
    }}
    void st(Stmt*s){switch(s->k){
        case Stmt::ExprS:ex(s->expr.get());emit(Op::Pop);break;
        case Stmt::VarS:{ex(s->expr.get());if(top){if(s->constant)const_globals.insert(s->name);emit(Op::StoreG,g(s->name));}else{int slot=nextlocal++;locals[s->name]=slot;if(s->constant)const_locals.insert(s->name);emit(Op::StoreL,slot);}emit(Op::Pop);break;}
        case Stmt::Block:for(auto&x:s->body)st(x.get());break;
        case Stmt::IfS:{ex(s->cond.get());int jf=jump(Op::JumpFalse);emit(Op::Pop);for(auto&x:s->body)st(x.get());int je=jump(Op::Jump);patch(jf);emit(Op::Pop);for(auto&x:s->else_body)st(x.get());patch(je);break;}
        case Stmt::WhileS:{int start=(int)f->code.size();break_jumps.push_back({});continue_jumps.push_back({});ex(s->cond.get());int jf=jump(Op::JumpFalse);emit(Op::Pop);for(auto&x:s->body)st(x.get());for(int j:continue_jumps.back())f->code[j].a=start;emit(Op::Jump,start);patch(jf);emit(Op::Pop);int end=(int)f->code.size();for(int j:break_jumps.back())f->code[j].a=end;break_jumps.pop_back();continue_jumps.pop_back();break;}
        case Stmt::ForS:{if(s->init)st(s->init.get());int start=(int)f->code.size();break_jumps.push_back({});continue_jumps.push_back({});ex(s->cond.get());int jf=jump(Op::JumpFalse);emit(Op::Pop);for(auto&x:s->body)st(x.get());int step=(int)f->code.size();for(int j:continue_jumps.back())f->code[j].a=step;if(s->step){ex(s->step.get());emit(Op::Pop);}emit(Op::Jump,start);patch(jf);emit(Op::Pop);int end=(int)f->code.size();for(int j:break_jumps.back())f->code[j].a=end;break_jumps.pop_back();continue_jumps.pop_back();break;}
        case Stmt::ReturnS:if(s->expr)ex(s->expr.get());else emit(Op::Null);emit(Op::Ret);break;
        case Stmt::BreakS:if(break_jumps.empty())throw Error("break outside loop");break_jumps.back().push_back((int)f->code.size());emit(Op::Jump,-1);break;
        case Stmt::ContinueS:if(continue_jumps.empty())throw Error("continue outside loop");continue_jumps.back().push_back((int)f->code.size());emit(Op::Jump,-1);break;
        case Stmt::FuncS:break;
    }}
public:
    Program compile(std::vector<std::unique_ptr<Stmt>>&ast,int opt){if(opt>0)optimize_stmts(ast);
        p.funcs.push_back({"<main>",0,0,{}});fidx["<main>"]=0;
        for(auto&s:ast)if(s->k==Stmt::FuncS&&s->external)externs.insert(s->name);
        for(auto&s:ast)if(s->k==Stmt::FuncS&&!s->external){if(fidx.count(s->name))throw Error("duplicate function: "+s->name);int id=(int)p.funcs.size();fidx[s->name]=id;p.funcs.push_back({s->name,(int)s->params.size(),0,{}});}
        f=&p.funcs[0];top=true;for(auto&s:ast)if(s->k!=Stmt::FuncS)st(s.get());emit(Op::Null);emit(Op::Ret);
        for(auto&s:ast)if(s->k==Stmt::FuncS&&!s->external){f=&p.funcs[fidx[s->name]];top=false;locals.clear();const_locals.clear();nextlocal=0;auto dot=s->name.rfind('.');current_ns=dot==std::string::npos?std::string():s->name.substr(0,dot);for(auto&n:s->params)locals[n]=nextlocal++;for(auto&x:s->body)st(x.get());emit(Op::Null);emit(Op::Ret);f->locals=nextlocal;}
        return p;}
};

static void w32(std::ostream&o,uint32_t x){o.write((char*)&x,4);} static uint32_t r32(std::istream&i){uint32_t x;i.read((char*)&x,4);if(!i)throw Error("truncated bytecode");return x;}
static void wstr(std::ostream&o,const std::string&s){w32(o,(uint32_t)s.size());o.write(s.data(),(std::streamsize)s.size());} static std::string rstr(std::istream&i){auto n=r32(i);std::string s(n,'\0');i.read(s.data(),n);if(!i)throw Error("truncated bytecode");return s;}
static void save(const Program&p,const fs::path&fn){std::ofstream o(fn,std::ios::binary);if(!o)throw Error("cannot write "+fn.string());o.write("JBC1",4);w32(o,1);w32(o,p.consts.size());for(auto&v:p.consts){uint8_t tag=(uint8_t)v.data.index();o.write((char*)&tag,1);if(tag==1){auto x=std::get<int64_t>(v.data);o.write((char*)&x,8);}else if(tag==2){auto x=std::get<double>(v.data);o.write((char*)&x,8);}else if(tag==3){uint8_t x=std::get<bool>(v.data);o.write((char*)&x,1);}else if(tag==4)wstr(o,std::get<std::string>(v.data));}w32(o,p.globals.size());for(auto&s:p.globals)wstr(o,s);w32(o,p.funcs.size());for(auto&f:p.funcs){wstr(o,f.name);w32(o,f.arity);w32(o,f.locals);w32(o,f.code.size());for(auto&x:f.code){uint8_t op=(uint8_t)x.op;o.write((char*)&op,1);w32(o,(uint32_t)x.a);}}}
static Program load(const fs::path&fn){std::ifstream i(fn,std::ios::binary);if(!i)throw Error("cannot open "+fn.string());char m[4];i.read(m,4);if(std::memcmp(m,"JBC1",4))throw Error("not Jau bytecode");if(r32(i)!=1)throw Error("unsupported bytecode version");Program p;auto nc=r32(i);for(uint32_t z=0;z<nc;++z){uint8_t tag;i.read((char*)&tag,1);if(tag==0)p.consts.emplace_back();else if(tag==1){int64_t x;i.read((char*)&x,8);p.consts.emplace_back(x);}else if(tag==2){double x;i.read((char*)&x,8);p.consts.emplace_back(x);}else if(tag==3){uint8_t x;i.read((char*)&x,1);p.consts.emplace_back((bool)x);}else if(tag==4)p.consts.emplace_back(rstr(i));else throw Error("bad constant tag");}auto ng=r32(i);for(uint32_t z=0;z<ng;++z)p.globals.push_back(rstr(i));auto nf=r32(i);for(uint32_t z=0;z<nf;++z){Fun f;f.name=rstr(i);f.arity=(int)r32(i);f.locals=(int)r32(i);auto ni=r32(i);for(uint32_t q=0;q<ni;++q){uint8_t op;i.read((char*)&op,1);f.code.push_back({(Op)op,(int32_t)r32(i)});}p.funcs.push_back(std::move(f));}return p;}

class VM {
    Program p; std::vector<Value> globals,stack; struct Frame{int fn;size_t ip;std::vector<Value>locals;};std::vector<Frame> frames; bool trace=false;
    Value pop(){if(stack.empty())throw Error("VM stack underflow");auto v=std::move(stack.back());stack.pop_back();return v;}
    Value builtin(int id){
        if(id==-1){auto v=pop();std::cout<<v.str()<<"\n";return Value();}
        if(id==-2){auto n=std::chrono::steady_clock::now().time_since_epoch();return std::chrono::duration<double>(n).count();}
        if(id==-3){auto v=pop();if(auto a=std::get_if<ArrayPtr>(&v.data))return (int64_t)((*a)?(*a)->size():0);return (int64_t)v.str().size();}
        if(id==-4)return pop().str();
        if(id==-5){auto v=pop();return (int64_t)std::stoll(v.str());}
        if(id==-6){auto path=pop().str();return read_text(path);}
        if(id==-7){auto content=pop().str();auto path=pop().str();write_text(path,content);return true;}
        if(id==-8){auto needle=pop().str(),hay=pop().str();return hay.find(needle)!=std::string::npos;}
        if(id==-9){auto prefix=pop().str(),sv=pop().str();return sv.rfind(prefix,0)==0;}
        if(id==-10)return (int64_t)g_runtime_args.size();
        if(id==-11){auto q=as_int(pop());return q>=0&&q<(int64_t)g_runtime_args.size()?Value(g_runtime_args[(size_t)q]):Value("");}
        if(id==-12){auto k=pop().str();const char*v=std::getenv(k.c_str());return std::string(v?v:"");}
        if(id==-13)return fs::exists(pop().str());
        if(id==-14){auto q=pop().str();std::error_code ec;bool ok=fs::create_directories(q,ec)||fs::exists(q);return ok&&!ec;}
        if(id==-15){auto q=pop().str();std::error_code ec;bool ok=fs::remove(q,ec);return ok&&!ec;}
        if(id==-16){auto b=pop().str(),a=pop().str();return (fs::path(a)/b).lexically_normal().string();}
        if(id==-17)return fs::current_path().string();
        if(id==-18){auto u=pop().str();return http_get_external(u);}
        if(id==-19){auto path=pop().str(),u=pop().str();write_text(path,http_get_external(u));return true;}
        if(id==-20){auto n=as_int(pop()),pos=as_int(pop());auto sv=pop().str();if(pos<0)pos=0;if(n<0)n=0;if((size_t)pos>=sv.size())return std::string("");return sv.substr((size_t)pos,(size_t)n);}
        if(id==-21){auto pos=as_int(pop());auto sv=pop().str();return pos>=0&&(size_t)pos<sv.size()?std::string(1,sv[(size_t)pos]):std::string("");}
        if(id==-22){auto needle=pop().str(),sv=pop().str();auto q=sv.find(needle);return q==std::string::npos?(int64_t)-1:(int64_t)q;}
        if(id==-23)return trim_copy(pop().str());
        if(id==-24)return upper_copy(pop().str());
        if(id==-25)return lower_copy(pop().str());
        if(id==-26){auto b=pop().str(),a=pop().str(),sv=pop().str();return replace_all(sv,a,b);}
        if(id==-27){auto key=pop().str(),text=pop().str();return manifest_value(text,key);}
        if(id==-28)return (int64_t)fnv1a64(pop().str());
        if(id==-29){auto value=pop(),arr=pop();auto a=std::get_if<ArrayPtr>(&arr.data);if(!a||!*a)throw Error("push expects array");(*a)->push_back(value);return (int64_t)(*a)->size();}
        if(id==-30){auto arr=pop();auto a=std::get_if<ArrayPtr>(&arr.data);if(!a||!*a||(*a)->empty())return Value();auto v=(*a)->back();(*a)->pop_back();return v;}
        if(id==-31){auto sep=pop().str();auto arr=pop();auto a=std::get_if<ArrayPtr>(&arr.data);if(!a||!*a)throw Error("join expects array");std::string out;for(size_t i=0;i<(*a)->size();++i){if(i)out+=sep;out+=(*a)->at(i).str();}return out;}
        if(id==-32){auto sep=pop().str(),sv=pop().str();auto a=std::make_shared<Array>();if(sep.empty()){for(char c:sv)a->push_back(std::string(1,c));return a;}size_t pos=0;for(;;){auto q=sv.find(sep,pos);if(q==std::string::npos){a->push_back(sv.substr(pos));break;}a->push_back(sv.substr(pos,q-pos));pos=q+sep.size();}return a;}
        if(id==-33){std::string line;std::getline(std::cin,line);return line;}
        if(id==-34){auto ms=as_int(pop());if(ms>0)std::this_thread::sleep_for(std::chrono::milliseconds(ms));return Value();}
        if(id==-35){
#ifdef _WIN32
            return std::string("windows");
#elif __APPLE__
            return std::string("macos");
#else
            return std::string("linux");
#endif
        }
        if(id==-36){return std::string(sizeof(void*)==8?"x86_64":"x86");}
        if(id==-37)return fs::temp_directory_path().string();
        if(id==-38){auto max=as_int(pop());if(max<=0)return (int64_t)0;static thread_local std::mt19937_64 rng(std::random_device{}());std::uniform_int_distribution<int64_t>d(0,max-1);return d(rng);}
        if(id==-39){auto path=pop().str();auto a=std::make_shared<Array>();std::error_code ec;if(fs::exists(path,ec))for(auto&entry:fs::directory_iterator(path,ec))a->push_back(entry.path().filename().string());return a;}
        if(id==-40){auto path=pop().str();std::error_code ec;auto n=fs::file_size(path,ec);return ec?(int64_t)-1:(int64_t)n;}
        if(id==-41){auto path=pop().str();std::error_code ec;auto n=fs::remove_all(path,ec);return ec?(int64_t)-1:(int64_t)n;}
        if(id==-42){auto output=pop().str(),root=pop().str();return package_pack(root,output);}
        if(id==-43){auto dest=pop().str(),archive=pop().str();return package_extract(archive,dest);}
        if(id==-44){return package_manifest(pop().str());}
        if(id==-45){return package_verify(pop().str());}
        if(id==-46){auto n=std::chrono::steady_clock::now().time_since_epoch();return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(n).count();}
        if(id==-47){auto n=std::chrono::steady_clock::now().time_since_epoch();return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(n).count();}
        throw Error("bad builtin");
    }
    static bool eq(const Value&a,const Value&b){if(is_num(a)&&is_num(b))return as_double(a)==as_double(b);return a.str()==b.str();}
public:
    explicit VM(Program x,bool t=false):p(std::move(x)),globals(p.globals.size()),trace(t){}
    Value run(){frames.push_back({0,0,std::vector<Value>(std::max(0,p.funcs[0].locals))});while(!frames.empty()){auto&fr=frames.back();auto&fn=p.funcs[fr.fn];if(fr.ip>=fn.code.size())throw Error("instruction pointer out of range");size_t trace_ip=fr.ip;auto in=fn.code[fr.ip++];if(trace)std::cerr<<"[jau:trace] fn="<<fn.name<<" ip="<<trace_ip<<" op="<<(int)in.op<<" arg="<<in.a<<" stack="<<stack.size()<<" frames="<<frames.size()<<"\n";switch(in.op){
        case Op::Const:stack.push_back(p.consts.at(in.a));break;case Op::Null:stack.emplace_back();break;case Op::LoadG:stack.push_back(globals.at(in.a));break;case Op::StoreG:globals.at(in.a)=stack.back();break;case Op::LoadL:stack.push_back(fr.locals.at(in.a));break;case Op::StoreL:fr.locals.at(in.a)=stack.back();break;case Op::Pop:pop();break;
        case Op::Add:{auto b=pop(),a=pop();stack.push_back(addv(a,b));break;}case Op::Sub:{auto b=pop(),a=pop();stack.push_back(subv(a,b));break;}case Op::Mul:{auto b=pop(),a=pop();stack.push_back(mulv(a,b));break;}case Op::Div:{auto b=pop(),a=pop();double d=as_double(b);if(d==0)throw Error("division by zero");stack.push_back(as_double(a)/d);break;}case Op::Mod:{auto b=pop(),a=pop();auto d=as_int(b);if(!d)throw Error("division by zero");stack.push_back(as_int(a)%d);break;}case Op::Neg:{auto a=pop();stack.push_back(std::holds_alternative<int64_t>(a.data)?Value(-std::get<int64_t>(a.data)):Value(-as_double(a)));break;}case Op::Not:{auto a=pop();stack.push_back(!a.truthy());break;}case Op::BitNot:{auto a=pop();stack.push_back((int64_t)~as_int(a));break;}
        case Op::Eq:{auto b=pop(),a=pop();stack.push_back(eq(a,b));break;}case Op::Ne:{auto b=pop(),a=pop();stack.push_back(!eq(a,b));break;}case Op::Lt:{auto b=pop(),a=pop();stack.push_back(as_double(a)<as_double(b));break;}case Op::Le:{auto b=pop(),a=pop();stack.push_back(as_double(a)<=as_double(b));break;}case Op::Gt:{auto b=pop(),a=pop();stack.push_back(as_double(a)>as_double(b));break;}case Op::Ge:{auto b=pop(),a=pop();stack.push_back(as_double(a)>=as_double(b));break;}case Op::And:{auto b=pop(),a=pop();stack.push_back(a.truthy()&&b.truthy());break;}case Op::Or:{auto b=pop(),a=pop();stack.push_back(a.truthy()||b.truthy());break;}case Op::BitAnd:{auto b=pop(),a=pop();stack.push_back((int64_t)(as_int(a)&as_int(b)));break;}case Op::BitOr:{auto b=pop(),a=pop();stack.push_back((int64_t)(as_int(a)|as_int(b)));break;}case Op::BitXor:{auto b=pop(),a=pop();stack.push_back((int64_t)(as_int(a)^as_int(b)));break;}case Op::Shl:{auto b=pop(),a=pop();stack.push_back((int64_t)(as_int(a)<<as_int(b)));break;}case Op::Shr:{auto b=pop(),a=pop();stack.push_back((int64_t)(as_int(a)>>as_int(b)));break;}
        case Op::Jump:fr.ip=in.a;break;case Op::JumpFalse:if(!stack.back().truthy())fr.ip=in.a;break;
        case Op::MakeArray:{auto a=std::make_shared<Array>((size_t)in.a);for(int q=in.a-1;q>=0;--q)(*a)[(size_t)q]=pop();stack.push_back(a);break;}
        case Op::Index:{auto idx=as_int(pop());auto arr=pop();auto a=std::get_if<ArrayPtr>(&arr.data);if(!a||!*a)throw Error("indexing expects array");if(idx<0||idx>=(int64_t)(*a)->size())throw Error("array index out of range");stack.push_back((*a)->at((size_t)idx));break;}
        case Op::Call:{if(in.a<0){stack.push_back(builtin(in.a));break;}auto&callee=p.funcs.at(in.a);if(stack.size()<(size_t)callee.arity)throw Error("not enough arguments");std::vector<Value> loc(std::max(callee.locals,callee.arity));for(int q=callee.arity-1;q>=0;--q)loc[q]=pop();frames.push_back({in.a,0,std::move(loc)});break;}
        case Op::Ret:{Value rv=pop();frames.pop_back();if(frames.empty())return rv;stack.push_back(std::move(rv));break;}case Op::Halt:return Value();