from pathlib import Path

def read(p): return Path(p).read_text(encoding='utf-8')
def write(p,s): Path(p).write_text(s,encoding='utf-8')
def rep(s,a,b,label):
    if a not in s: raise SystemExit('anchor not found: '+label)
    return s.replace(a,b,1)

# --- jauld export directory + DLL mode ---------------------------------------
p='src/jauld_part02.inc'; s=read(p)
s=rep(s,
'''static void add_entry_and_thunks(LinkState&st,uint32_t&entry_off,std::unordered_map<std::string,uint32_t>&thunk_off,size_t&entry_main_patch){
    auto&text=st.outs[0].data;text.resize(alignsz(text.size(),16),0x90);entry_off=(uint32_t)text.size();
    if(st.x64){text.insert(text.end(),{0x48,0x83,0xec,0x28,0xe8,0,0,0,0,0x48,0x83,0xc4,0x28,0xc3});entry_main_patch=entry_off+5;}
    else{text.insert(text.end(),{0xe8,0,0,0,0,0xc3});entry_main_patch=entry_off+1;}
''',
'''static void add_entry_and_thunks(LinkState&st,uint32_t&entry_off,std::unordered_map<std::string,uint32_t>&thunk_off,size_t&entry_main_patch,bool executable_entry){
    auto&text=st.outs[0].data;text.resize(alignsz(text.size(),16),0x90);entry_off=(uint32_t)text.size();
    if(executable_entry){
        if(st.x64){text.insert(text.end(),{0x48,0x83,0xec,0x28,0xe8,0,0,0,0,0x48,0x83,0xc4,0x28,0xc3});entry_main_patch=entry_off+5;}
        else{text.insert(text.end(),{0xe8,0,0,0,0,0xc3});entry_main_patch=entry_off+1;}
    }else entry_main_patch=(size_t)-1;
''','entry optional')
# insert export structs/helpers before link_pe
anchor='''static int link_pe(const std::vector<std::string>&inputs,const std::string&output,const std::string&target,const std::string&entry,const std::string&subsystem){
'''
helper=r'''struct ExportItem {std::string public_name,internal_name;};
struct EdataBuild {std::vector<uint8_t> bytes;uint32_t funcs=0,names=0,ords=0,dll_name=0;std::vector<uint32_t> name_offs;};
static std::vector<ExportItem> parse_exports(const std::vector<std::string>&specs){std::vector<ExportItem> out;std::set<std::string> seen;for(auto&s:specs){auto q=s.find('=');ExportItem e{q==std::string::npos?s:s.substr(0,q),q==std::string::npos?s:s.substr(q+1)};if(e.public_name.empty()||e.internal_name.empty())throw std::runtime_error("--export expects name or public=internal");if(!seen.insert(e.public_name).second)throw std::runtime_error("duplicate DLL export: "+e.public_name);out.push_back(e);}std::sort(out.begin(),out.end(),[](auto&a,auto&b){return a.public_name<b.public_name;});return out;}
static EdataBuild build_edata(const std::vector<ExportItem>&ex,const std::string&dll){EdataBuild z;if(ex.empty())return z;z.bytes.resize(40,0);z.funcs=(uint32_t)z.bytes.size();z.bytes.resize(z.bytes.size()+ex.size()*4,0);z.names=(uint32_t)z.bytes.size();z.bytes.resize(z.bytes.size()+ex.size()*4,0);z.ords=(uint32_t)z.bytes.size();z.bytes.resize(z.bytes.size()+ex.size()*2,0);z.dll_name=(uint32_t)z.bytes.size();z.bytes.insert(z.bytes.end(),dll.begin(),dll.end());z.bytes.push_back(0);for(auto&e:ex){z.name_offs.push_back((uint32_t)z.bytes.size());z.bytes.insert(z.bytes.end(),e.public_name.begin(),e.public_name.end());z.bytes.push_back(0);}return z;}
static uint32_t find_export_rva(LinkState&st,const ExportItem&e){std::vector<std::string> names;if(e.internal_name.rfind("jau_fn_",0)==0){names={e.internal_name,"_"+e.internal_name};}else{names={"jau_fn_"+e.internal_name,"_jau_fn_"+e.internal_name,e.internal_name,"_"+e.internal_name};}return find_global_rva(st,names);}

static int link_pe(const std::vector<std::string>&inputs,const std::string&output,const std::string&target,const std::string&entry,const std::string&subsystem,bool dll_mode,const std::vector<std::string>&export_specs){
'''
s=rep(s,anchor,helper,'link signature and export helpers')
s=rep(s,
'''    if(inputs.empty())throw std::runtime_error("no object inputs");LinkState st;st.x64=target=="windows-x86_64";if(!st.x64&&target!="windows-x86")throw std::runtime_error("jauld supports windows-x86_64/windows-x86");st.machine=st.x64?0x8664:0x014c;st.image_base=st.x64?0x140000000ull:0x00400000ull;
''',
'''    if(inputs.empty())throw std::runtime_error("no object inputs");LinkState st;st.x64=target=="windows-x86_64";if(!st.x64&&target!="windows-x86")throw std::runtime_error("jauld supports windows-x86_64/windows-x86");st.machine=st.x64?0x8664:0x014c;st.image_base=dll_mode?(st.x64?0x180000000ull:0x10000000ull):(st.x64?0x140000000ull:0x00400000ull);auto exports=parse_exports(export_specs);if(dll_mode&&exports.empty())throw std::runtime_error("DLL build requires at least one --export name");
''','dll image base')
s=rep(s,
'''    st.outs={{".text",{},0,0,0,0,0x60000020u},{".rdata",{},0,0,0,0,0x40000040u},{".data",{},0,0,0,0,0xC0000040u},{".idata",{},0,0,0,0,0xC0000040u}};
''',
'''    st.outs={{".text",{},0,0,0,0,0x60000020u},{".rdata",{},0,0,0,0,0x40000040u},{".data",{},0,0,0,0,0xC0000040u},{".idata",{},0,0,0,0,0xC0000040u},{".edata",{},0,0,0,0,0x40000040u}};
''','edata section')
s=rep(s,
'''    collect_imports(st);std::unordered_map<std::string,uint32_t>thunk_off;uint32_t entry_off=0;size_t main_patch=0;add_entry_and_thunks(st,entry_off,thunk_off,main_patch);auto id=build_idata(st.imports,st.x64);st.outs[3].data=id.bytes;st.outs[3].virtual_size=(uint32_t)id.bytes.size();
''',
'''    collect_imports(st);std::unordered_map<std::string,uint32_t>thunk_off;uint32_t entry_off=0;size_t main_patch=0;add_entry_and_thunks(st,entry_off,thunk_off,main_patch,!dll_mode);auto id=build_idata(st.imports,st.x64);st.outs[3].data=id.bytes;st.outs[3].virtual_size=(uint32_t)id.bytes.size();auto ed=build_edata(exports,fs::path(output).filename().string());st.outs[4].data=ed.bytes;st.outs[4].virtual_size=(uint32_t)ed.bytes.size();
''','build edata')
write(p,s)

p='src/jauld_part03.inc'; s=read(p)
# After section layout, patch export table RVAs and function addresses.
needle='''    auto&idata=st.outs[3].data;std::map<std::string,std::vector<size_t>>groups;'''
insert='''    if(dll_mode&&!exports.empty()){auto&edata=st.outs[4].data;uint32_t er=st.outs[4].rva;p32(edata,12,er+ed.dll_name);p32(edata,16,1);p32(edata,20,(uint32_t)exports.size());p32(edata,24,(uint32_t)exports.size());p32(edata,28,er+ed.funcs);p32(edata,32,er+ed.names);p32(edata,36,er+ed.ords);for(size_t i=0;i<exports.size();++i){p32(edata,ed.funcs+i*4,find_export_rva(st,exports[i]));p32(edata,ed.names+i*4,er+ed.name_offs[i]);p16(edata,ed.ords+i*2,(uint16_t)i);}}
    auto&idata=st.outs[3].data;std::map<std::string,std::vector<size_t>>groups;'''
s=rep(s,needle,insert,'patch edata')
# Make entry optional for DLL.
old='''    std::vector<std::string> entry_names;
    if(!entry.empty()){
        entry_names.push_back(entry);
        if(!st.x64){if(entry[0]=='_')entry_names.push_back(entry.substr(1));else entry_names.push_back("_"+entry);}
    }else if(st.x64)entry_names={"main","jau_fn_main"};
    else entry_names={"_main","main","_jau_fn_main","jau_fn_main"};
    uint32_t mainr=find_global_rva(st,entry_names);uint32_t entryr=st.outs[0].rva+entry_off;patch_rel32(st.outs[0].data,main_patch,st.outs[0].rva+(uint32_t)main_patch,mainr);
'''
new='''    std::vector<std::string> entry_names;uint32_t entryr=0;
    if(!dll_mode){if(!entry.empty()){entry_names.push_back(entry);if(!st.x64){if(entry[0]=='_')entry_names.push_back(entry.substr(1));else entry_names.push_back("_"+entry);}}else if(st.x64)entry_names={"main","jau_fn_main"};else entry_names={"_main","main","_jau_fn_main","jau_fn_main"};uint32_t mainr=find_global_rva(st,entry_names);entryr=st.outs[0].rva+entry_off;patch_rel32(st.outs[0].data,main_patch,st.outs[0].rva+(uint32_t)main_patch,mainr);}
'''
s=rep(s,old,new,'optional DLL entry')
# PE sizes/flags/directories/message.
s=rep(s,
'''p32(pe,p,st.outs[1].raw_size+st.outs[2].raw_size+st.outs[3].raw_size);p+=4;''',
'''uint32_t data_raw=0;for(size_t zi=1;zi<st.outs.size();++zi)data_raw+=st.outs[zi].raw_size;p32(pe,p,data_raw);p+=4;''','PE data raw size')
s=rep(s,
'''uint16_t chars=0x0003|(st.x64?0x0020:0x0100);p16(pe,p,chars);''',
'''uint16_t chars=(uint16_t)(0x0003|(st.x64?0x0020:0x0100)|(dll_mode?0x2000:0));p16(pe,p,chars);''','DLL file characteristic')
s=rep(s,
'''if(!groups.empty()){p32(pe,dirs+8,st.outs[3].rva);p32(pe,dirs+12,(uint32_t)(groups.size()*20u));}''',
'''if(dll_mode&&!exports.empty()){p32(pe,dirs,st.outs[4].rva);p32(pe,dirs+4,(uint32_t)st.outs[4].virtual_size);}if(!groups.empty()){p32(pe,dirs+8,st.outs[3].rva);p32(pe,dirs+12,(uint32_t)(groups.size()*20u));}''','export data directory')
s=rep(s,
'''write_file(output,out);std::cout<<"jauld: wrote "<<output<<" ("<<(st.x64?"PE32+ x86-64":"PE32 x86")<<", internal linker, "<<out.size()<<" bytes, "<<nsec<<" sections, entry="<<entry_names.front()<<")\\n";return 0;''',
'''write_file(output,out);std::cout<<"jauld: wrote "<<output<<" ("<<(st.x64?"PE32+ x86-64":"PE32 x86")<<(dll_mode?" DLL":" EXE")<<", internal linker, "<<out.size()<<" bytes, "<<nsec<<" sections"<<(dll_mode?std::string(", exports=")+std::to_string(exports.size()):std::string(", entry=")+entry_names.front())<<")\\n";return 0;''','link output message')
# CLI --dll/--export
s=rep(s,
'''static void usage(){std::cout<<"jauld 0.9\\nusage: jauld <input.obj> [more.obj ...] -o <program.exe> [--target <windows-x86_64|windows-x86>] [--entry symbol] [--meta file.jmeta] [--system-lib user32] [--import symbol=dll] [--subsystem console|windows]''',
'''static void usage(){std::cout<<"jauld 0.9\\nusage: jauld <input.obj> [more.obj ...] -o <program.exe|library.dll> [--target <windows-x86_64|windows-x86>] [--entry symbol] [--dll --export public[=internal]] [--meta file.jmeta] [--system-lib user32] [--import symbol=dll] [--subsystem console|windows]''','jauld help dll')
s=rep(s,
'''if(argc<2){usage();return 0;}std::vector<std::string>in,system_libs,imports;std::string out="a.exe",target,entry,meta_path,subsystem="console";
''',
'''if(argc<2){usage();return 0;}std::vector<std::string>in,system_libs,imports,exports;std::string out="a.exe",target,entry,meta_path,subsystem="console";bool dll_mode=false;
''','jauld dll vars')
s=rep(s,
'''else if(a=="--subsystem"&&i+1<argc)subsystem=lower(argv[++i]);else if(a=="--help"||a=="-h")''',
'''else if(a=="--subsystem"&&i+1<argc)subsystem=lower(argv[++i]);else if(a=="--dll")dll_mode=true;else if(a=="--export"&&i+1<argc)exports.push_back(argv[++i]);else if(a=="--help"||a=="-h")''','jauld parse dll')
s=rep(s,
'''return link_pe(in,out,target,entry,subsystem);''',
'''return link_pe(in,out,target,entry,subsystem,dll_mode,exports);''','jauld link dll args')
write(p,s)

# --- Jau core generic shared support -----------------------------------------
p='src/jau_part03.inc'; s=read(p)
s=rep(s,
'''    std::string cc=pick_cc(target);std::string cc0=cc,flag;if(target=="linux-x86")flag=" -m32";else if(target=="linux-x86_64")flag=" -no-pie";
''',
'''    std::string cc=pick_cc(target);std::string cc0=cc,flag;if(o.shared){flag=" -shared -fPIC";if(target=="linux-x86")flag+=" -m32";}else if(target=="linux-x86")flag=" -m32";else if(target=="linux-x86_64")flag=" -no-pie";
''','native shared compiler flags')
write(p,s)

# --- jauc shared command ------------------------------------------------------
p='src/jauc_main.cpp'; s=read(p)
s=rep(s,
'''static jau::Result windows_native_build(const char* argv0,const std::string&input,std::string output,const std::string&target,jau::CompileOptions opt){''',
'''static jau::Result windows_native_build(const char* argv0,const std::string&input,std::string output,const std::string&target,jau::CompileOptions opt,bool shared=false){''','windows build shared signature')
s=rep(s,
'''fs::path asmp=tmp/"app.s",obj=tmp/"app.obj";opt.library_mode=false;auto r=jau::emit_assembly''',
'''fs::path asmp=tmp/"app.s",obj=tmp/"app.obj";opt.library_mode=shared;opt.shared=shared;auto r=jau::emit_assembly''','library mode for shared')
old='''        if(fs::path(output).extension()!=".exe")output+=".exe";std::vector<std::string> args=objs;args.push_back("-o");args.push_back(output);args.push_back("--target");args.push_back(target);args.push_back("--entry");args.push_back("main");args.push_back("--subsystem");args.push_back(opt.subsystem);for(auto&x:opt.system_libs){args.push_back("--system-lib");args.push_back(x);}for(auto&x:opt.native_imports){args.push_back("--import");args.push_back(x);}int rc=run_process(linker,args);fs::remove_all(tmp,ec);if(rc!=0)return {false,"link stage: internal jauld PE link failed (rerun with --debug to list package objects/symbols)"};return {true,"native executable built without external linker: "+output};
'''
new='''        if(shared){if(lower_ext(output)!=".dll")output+=".dll";}else if(fs::path(output).extension()!=".exe")output+=".exe";std::vector<std::string> args=objs;args.push_back("-o");args.push_back(output);args.push_back("--target");args.push_back(target);if(shared){args.push_back("--dll");for(auto&x:opt.exports){args.push_back("--export");args.push_back(x);}}else{args.push_back("--entry");args.push_back("main");args.push_back("--subsystem");args.push_back(opt.subsystem);}for(auto&x:opt.system_libs){args.push_back("--system-lib");args.push_back(x);}for(auto&x:opt.native_imports){args.push_back("--import");args.push_back(x);}int rc=run_process(linker,args);fs::remove_all(tmp,ec);if(rc!=0)return {false,std::string("link stage: internal jauld PE ")+(shared?"DLL":"EXE")+" link failed"};return {true,std::string(shared?"native shared library built: ":"native executable built without external linker: ")+output};
'''
s=rep(s,old,new,'windows shared link command')
s=rep(s,
'''              << "  jauc native <file.jau> -o program --target <target> [--link native.obj] [--system-lib name] [--import symbol=dll] [--subsystem console|windows] [-O0..-O3]\\n"
''',
'''              << "  jauc native <file.jau> -o program --target <target> [--link native.obj] [--system-lib name] [--import symbol=dll] [--subsystem console|windows] [-O0..-O3]\\n"
              << "  jauc shared <file.jau> -o library --target <target> --export name [--export public=internal] [--system-lib name]\\n"
''','jauc shared help')
s=rep(s,
'''else if (a == "--subsystem" && i + 1 < argc) opt.subsystem=lower_copy_local(argv[++i]); else if (a == "-I"''',
'''else if (a == "--subsystem" && i + 1 < argc) opt.subsystem=lower_copy_local(argv[++i]); else if (a == "--export" && i + 1 < argc) opt.exports.push_back(argv[++i]); else if (a == "-I"''','parse export')
s=rep(s,
'''    if(cmd=="native"&&!optimize_set)opt.optimize=0; // correctness-first native default
''',
'''    if((cmd=="native"||cmd=="shared")&&!optimize_set)opt.optimize=0; // correctness-first native default
''','shared optimization default')
needle='''    else if (cmd == "native") { if (target.empty()) target = "linux-x86_64"; if (output.empty()) output = is_windows_target(target)?"jau-app.exe":"a.out"; r = is_windows_target(target)?windows_native_build(argv[0],input,output,target,opt):generic_native_build_with_packages(input, output, target, opt); }
'''
replace=needle+'''    else if (cmd == "shared") { if(target.empty())target="linux-x86_64";if(opt.exports.empty()){std::cerr<<"jauc: shared requires at least one --export name\\n";return 2;}opt.shared=true;opt.library_mode=true;if(output.empty())output=is_windows_target(target)?"jau-lib.dll":"libjau.so";r=is_windows_target(target)?windows_native_build(argv[0],input,output,target,opt,true):generic_native_build_with_packages(input,output,target,opt); }
'''
s=rep(s,needle,replace,'shared command branch')
write(p,s)

# JauM shared type.
p='src/jaum_main.cpp'; s=read(p)
s=rep(s,
'''static std::string ext_for(const std::string&type,const std::string&target){bool win=target.rfind("windows-",0)==0;if(type=="exe")return win?".exe":"";if(type=="obj")return win?".obj":".o";if(type=="asm")return ".s";return "";}''',
'''static std::string ext_for(const std::string&type,const std::string&target){bool win=target.rfind("windows-",0)==0;if(type=="exe")return win?".exe":"";if(type=="shared")return win?".dll":".so";if(type=="obj")return win?".obj":".o";if(type=="asm")return ".s";return "";}''','JauM shared extension')
s=rep(s,
'''if(type!="exe"&&type!="obj"&&type!="asm")throw std::runtime_error("type must be exe, obj or asm in JauM 0.9");''',
'''if(type!="exe"&&type!="shared"&&type!="obj"&&type!="asm")throw std::runtime_error("type must be exe, shared, obj or asm in JauM 0.9");''','JauM shared type')
s=rep(s,
'''a.push_back(type=="exe"?"native":type);''',
'''a.push_back(type=="exe"?"native":type);''','JauM no-op anchor')
# add exports after imports loop
s=rep(s,
'''for(auto&x:split(get("imports"))){a.push_back("--import");a.push_back(x);}for(auto&x:split(get("include_paths")))''',
'''for(auto&x:split(get("imports"))){a.push_back("--import");a.push_back(x);}for(auto&x:split(get("exports"))){a.push_back("--export");a.push_back(x);}for(auto&x:split(get("include_paths")))''','JauM exports')
write(p,s)

Path('tests/v090_shared.jau').write_text('''func add(a:int,b:int):int { return a+b; }\nfunc answer():int { return 42; }\n''',encoding='utf-8')
print('Jau 0.9 shared-library patch applied')
