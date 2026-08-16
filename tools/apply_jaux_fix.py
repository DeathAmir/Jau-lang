from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label} anchor not found")
    return text.replace(old, new, 1)


p = Path("src/jau_part01.inc")
s = p.read_text(encoding="utf-8")
s = replace_once(s, "LBracket,RBracket,Comma,Semi,Dot,Plus", "LBracket,RBracket,Comma,Semi,Colon,Dot,Plus", "token enum")
s = replace_once(
    s,
    "case ',':k=TK::Comma;break;case ';':k=TK::Semi;break;case '.':k=TK::Dot;break;",
    "case ',':k=TK::Comma;break;case ';':k=TK::Semi;break;case ':':k=TK::Colon;break;case '.':k=TK::Dot;break;",
    "lexer colon",
)
block = '    std::vector<std::unique_ptr<Stmt>> block_body(){need(TK::LBrace,"expected {");std::vector<std::unique_ptr<Stmt>> v;while(!at(TK::RBrace)&&!at(TK::End))v.push_back(stmt());need(TK::RBrace,"expected }");return v;}\n'
helper = '    void skip_type_annotation(){if(eat(TK::Colon))qualified_id();}\n'
if helper not in s:
    if block not in s:
        raise SystemExit("block_body anchor not found")
    s = s.replace(block, block + helper, 1)
old_func = '''    std::unique_ptr<Stmt> parse_func(bool external){
        auto s=std::make_unique<Stmt>();s->k=Stmt::FuncS;s->external=external;s->name=qualified_id();if(!current_namespace.empty()&&s->name.find('.')==std::string::npos)s->name=current_namespace+"."+s->name;need(TK::LParen,"expected (");if(!at(TK::RParen)){do{s->params.push_back(need(TK::Id,"expected parameter").text);}while(eat(TK::Comma));}need(TK::RParen,"expected )");if(external){need(TK::Semi,"expected ; after extern func");}else s->body=block_body();return s;
    }'''
new_func = '''    std::unique_ptr<Stmt> parse_func(bool external){
        auto s=std::make_unique<Stmt>();s->k=Stmt::FuncS;s->external=external;s->name=qualified_id();if(!current_namespace.empty()&&s->name.find('.')==std::string::npos)s->name=current_namespace+"."+s->name;need(TK::LParen,"expected (");
        if(!at(TK::RParen)){do{auto param=need(TK::Id,"expected parameter").text;skip_type_annotation();s->params.push_back(param);}while(eat(TK::Comma));}
        need(TK::RParen,"expected )");skip_type_annotation();
        if(external){need(TK::Semi,"expected ; after extern func");}else s->body=block_body();return s;
    }'''
s = replace_once(s, old_func, new_func, "typed function parser")
s = replace_once(
    s,
    's->name=need(TK::Id,"expected variable name").text;if(eat(TK::Eq))s->expr=expr();',
    's->name=need(TK::Id,"expected variable name").text;skip_type_annotation();if(eat(TK::Eq))s->expr=expr();',
    "typed variable parser",
)
old_pkg = '''    if(fs::exists(archive)){
        if(sub.empty()){auto mf=package_manifest(archive.string());sub=manifest_value(mf,"main");if(sub.empty())sub="main.jau";}
        return expand_package_impl(archive,sub,opts,seen);
    }'''
new_pkg = '''    if(fs::exists(archive)){
        auto mf=package_manifest(archive.string());
        auto type=lower_copy(manifest_value(mf,"type"));
        if(sub.empty()){sub=manifest_value(mf,"main");if(sub.empty())sub="main.jau";}
        auto ext=lower_copy(fs::path(sub).extension().string());
        if(type=="native"&&!ext.empty()&&ext!=".jau")throw Error("native package source entry must be Jau source, not binary object: "+sub);
        return expand_package_impl(archive,sub,opts,seen);
    }'''
s = replace_once(s, old_pkg, new_pkg, "package source entry guard")
s = replace_once(
    s,
    '    fs::path normalized=fs::path(rel).lexically_normal();std::string rs=normalized.generic_string();std::string key=',
    '    fs::path normalized=fs::path(rel).lexically_normal();auto source_ext=lower_copy(normalized.extension().string());if(!source_ext.empty()&&source_ext!=".jau")throw Error("package source resolver refused binary/non-Jau member: "+normalized.generic_string());std::string rs=normalized.generic_string();std::string key=',
    "package binary parser guard",
)
p.write_text(s, encoding="utf-8")

p = Path("src/jauc_main.cpp")
s = p.read_text(encoding="utf-8")
anchor = 'static std::string native_key(std::string target){for(char&c:target)if(c==\'-\')c=\'_\';return "native_"+target;}\n'
helper = 'static std::string lower_copy_local(std::string x){for(char&c:x)c=(char)std::tolower((unsigned char)c);return x;}\n'
if helper not in s:
    if anchor not in s:
        raise SystemExit("jauc helper anchor not found")
    s = s.replace(anchor, anchor + helper, 1)
old_native = '''        std::string mf=jau::package_manifest(archive.string());std::string members=manifest_value(mf,native_key(target));if(members.empty())members=manifest_value(mf,"native");
        for(auto&member:split_csv(members)){std::string bytes=jau::package_read_file(archive.string(),member);fs::path p=temp/("pkg_"+std::to_string(serial++)+"_"+fs::path(member).filename().string());if(p.extension().empty())p+=".obj";write_binary(p,bytes);objects.push_back(p.string());}
        for(auto&dep:split_csv(manifest_value(mf,"dependencies")))package(dep);
        if(sub.empty())sub=manifest_value(mf,"main");if(!sub.empty()){try{scan_text(jau::package_read_file(archive.string(),sub),fs::current_path()/"__package__.jau");}catch(...) {}}'''
new_native = '''        std::string mf=jau::package_manifest(archive.string());std::string type=lower_copy_local(manifest_value(mf,"type"));
        if(type=="native"){
            std::string members=manifest_value(mf,native_key(target));if(members.empty())members=manifest_value(mf,"native");
            if(members.empty())throw std::runtime_error("native package "+name+" has no object for target "+target);
            for(auto&member:split_csv(members)){
                auto ext=lower_copy_local(fs::path(member).extension().string());
                if(ext!=".obj"&&ext!=".o")throw std::runtime_error("native package "+name+" member is not an object file: "+member);
                std::string bytes=jau::package_read_file(archive.string(),member);
                fs::path p=temp/("pkg_"+std::to_string(serial++)+"_"+fs::path(member).filename().string());
                write_binary(p,bytes);objects.push_back(p.string());
            }
        }
        for(auto&dep:split_csv(manifest_value(mf,"dependencies")))package(dep);
        if(sub.empty())sub=manifest_value(mf,"main");
        if(!sub.empty()){
            auto ext=lower_copy_local(fs::path(sub).extension().string());
            if(!ext.empty()&&ext!=".jau")throw std::runtime_error("package source entry is not Jau source: "+sub);
            scan_text(jau::package_read_file(archive.string(),sub),fs::current_path()/"__package__.jau");
        }'''
s = replace_once(s, old_native, new_native, "native collector separation")
p.write_text(s, encoding="utf-8")

p = Path("src/jau_part03.inc")
s = p.read_text(encoding="utf-8").replace('std::string version(){return "0.7.0";}', 'std::string version(){return "0.7.1";}')
p.write_text(s, encoding="utf-8")

Path("tests/native_extension/src/main.jau").write_text(
    "extern func cppmath_mul(a:int, b:int):int;\n\nnamespace CppMath {\n    func mul(a:int, b:int):int {\n        return cppmath_mul(a, b);\n    }\n}\n",
    encoding="utf-8",
)

p = Path("docs/INTEROP.md")
if p.exists():
    s = p.read_text(encoding="utf-8")
    marker = "## Native package source/binary separation (0.7.1)"
    if marker not in s:
        s += """\n\n## Native package source/binary separation (0.7.1)\n\nFor `type=\"native\"` packages, the manifest `main` member is parsed as Jau source while `native_windows_*` members are opaque object bytes. `.obj`/`.o` members never enter the lexer/parser; `jauc native` extracts them and passes them directly to `jauld`.\n\nOptional annotations such as `extern func f(a:int):int;` are accepted as syntax metadata. Full static type checking is not implied yet.\n"""
        p.write_text(s, encoding="utf-8")

print("Jau 0.7.1 native package fix applied")
