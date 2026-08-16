    std::unordered_map<std::string,uint32_t> iat_target;
};

static uint32_t symbol_rva(LinkState& st,size_t oi,const Symbol&s){
    if(s.section<=0||(size_t)s.section>st.objs[oi].sections.size())throw std::runtime_error("symbol has no section: "+s.name);
    auto&sec=st.objs[oi].sections[(size_t)s.section-1];if(sec.out_group<0)throw std::runtime_error("symbol points to discarded section: "+s.name);
    return st.outs[(size_t)sec.out_group].rva+sec.out_off+s.value;
}
static uint32_t resolve_symbol(LinkState&st,size_t oi,uint32_t si){
    if(si>=st.objs[oi].symbols.size())throw std::runtime_error("bad relocation symbol index");auto&s=st.objs[oi].symbols[si];if(!s.valid||s.aux_entry)throw std::runtime_error("relocation references invalid symbol");
    if(s.section>0)return symbol_rva(st,oi,s);
    if(s.section==-1)return s.value;
    auto it=st.globals.find(s.name);if(it!=st.globals.end())return symbol_rva(st,it->second.obj,st.objs[it->second.obj].symbols[it->second.sym]);
    {std::string wanted=undecorate(s.name);DefRef*match=nullptr;std::string matched_name;for(auto&g:st.globals)if(undecorate(g.first)==wanted){if(match&&matched_name!=g.first)throw std::runtime_error("ambiguous decorated external symbol: "+s.name+" matches "+matched_name+" and "+g.first);match=&g.second;matched_name=g.first;}if(match)return symbol_rva(st,match->obj,st.objs[match->obj].symbols[match->sym]);}
    if(!st.x64){
        std::string alt=s.name;
        if(!alt.empty()&&alt[0]=='_')alt.erase(0,1);else alt="_"+alt;
        auto jt=st.globals.find(alt);if(jt!=st.globals.end())return symbol_rva(st,jt->second.obj,st.objs[jt->second.obj].symbols[jt->second.sym]);
        if(s.name.empty()||s.name[0]!='_'){
            std::string prefix="_"+s.name+"@";
            DefRef* found=nullptr;
            for(auto&g:st.globals)if(g.first.rfind(prefix,0)==0){if(found)throw std::runtime_error("ambiguous stdcall external symbol: "+s.name);found=&g.second;}
            if(found)return symbol_rva(st,found->obj,st.objs[found->obj].symbols[found->sym]);
        }
    }
    auto ii=st.import_target.find(s.name);if(ii!=st.import_target.end())return ii->second;
    auto ia=st.iat_target.find(s.name);if(ia!=st.iat_target.end())return ia->second;
    std::string avail;int shown=0;for(auto&g:st.globals){if(shown++>=12)break;if(!avail.empty())avail+=", ";avail+=g.first;}throw std::runtime_error("unresolved external symbol: "+s.name+(avail.empty()?std::string(""):std::string("; defined externals include: ")+avail)+". For C++ extensions export ABI functions with extern \"C\" or use --debug in jauc to inspect package symbols");
}

static void collect_imports(LinkState&st){
    std::set<std::string> unresolved;
    for(size_t oi=0;oi<st.objs.size();++oi)for(auto&sec:st.objs[oi].sections)if(sec.out_group>=0)for(auto&r:sec.relocs){if(r.sym>=st.objs[oi].symbols.size())continue;auto&s=st.objs[oi].symbols[r.sym];if(s.valid&&!s.aux_entry&&s.section==0&&!st.globals.count(s.name))unresolved.insert(s.name);}
    for(auto&exact:unresolved){std::string canon=undecorate(exact),dll=import_dll(canon);if(dll.empty())continue;auto it=st.import_by_canon.find(canon);size_t idx;if(it==st.import_by_canon.end()){idx=st.imports.size();st.import_by_canon[canon]=idx;st.imports.push_back({exact,canon,dll,is_indirect_import(exact),0,0,0});}else idx=it->second;if(is_indirect_import(exact))st.imports[idx].indirect=true;}
}

struct IdataPatch {size_t off=0;uint32_t rel=0;bool qword=false;};
struct IdataBuild {std::vector<uint8_t> bytes;std::vector<IdataPatch> patches;std::unordered_map<std::string,uint32_t> iat_rel;uint32_t dir_rel=0,dir_size=0,iat_rel_min=0,iat_rel_max=0;};
static IdataBuild build_idata(const std::vector<ImportDesc>& imports,bool x64){
    IdataBuild z;if(imports.empty())return z;std::map<std::string,std::vector<size_t>> groups;for(size_t i=0;i<imports.size();++i)groups[imports[i].dll].push_back(i);size_t desc_count=groups.size()+1;z.bytes.resize(desc_count*20u,0);struct G{std::string dll;std::vector<size_t> idx;uint32_t int_off=0,iat_off=0,dll_off=0;};std::vector<G> gs;size_t ptr=x64?8:4;
    for(auto&kv:groups){G g;g.dll=kv.first;g.idx=kv.second;z.bytes.resize(alignsz(z.bytes.size(),ptr),0);g.int_off=(uint32_t)z.bytes.size();z.bytes.resize(z.bytes.size()+(g.idx.size()+1)*ptr,0);g.iat_off=(uint32_t)z.bytes.size();z.bytes.resize(z.bytes.size()+(g.idx.size()+1)*ptr,0);gs.push_back(g);}
    std::vector<uint32_t> hint(imports.size(),0);for(size_t i=0;i<imports.size();++i){z.bytes.resize(alignsz(z.bytes.size(),2),0);hint[i]=(uint32_t)z.bytes.size();w16(z.bytes,0);z.bytes.insert(z.bytes.end(),imports[i].canonical.begin(),imports[i].canonical.end());z.bytes.push_back(0);}
    for(auto&g:gs){g.dll_off=(uint32_t)z.bytes.size();z.bytes.insert(z.bytes.end(),g.dll.begin(),g.dll.end());z.bytes.push_back(0);}
    size_t di=0;bool first_iat=true;for(auto&g:gs){size_t d=di++*20u;p32(z.bytes,d+0,g.int_off);p32(z.bytes,d+12,g.dll_off);p32(z.bytes,d+16,g.iat_off);for(size_t k=0;k<g.idx.size();++k){size_t idx=g.idx[k];uint32_t io=g.int_off+(uint32_t)(k*ptr),ao=g.iat_off+(uint32_t)(k*ptr);if(x64){p64(z.bytes,io,hint[idx]);p64(z.bytes,ao,hint[idx]);}else{p32(z.bytes,io,hint[idx]);p32(z.bytes,ao,hint[idx]);}z.iat_rel[imports[idx].canonical]=ao;if(first_iat){z.iat_rel_min=ao;z.iat_rel_max=ao+(uint32_t)ptr;first_iat=false;}else{z.iat_rel_min=std::min(z.iat_rel_min,ao);z.iat_rel_max=std::max(z.iat_rel_max,ao+(uint32_t)ptr);}}}
    z.dir_rel=0;z.dir_size=(uint32_t)(groups.size()*20u);return z;
}

static void add_entry_and_thunks(LinkState&st,uint32_t&entry_off,std::unordered_map<std::string,uint32_t>&thunk_off,size_t&entry_main_patch){
    auto&text=st.outs[0].data;text.resize(alignsz(text.size(),16),0x90);entry_off=(uint32_t)text.size();
    if(st.x64){text.insert(text.end(),{0x48,0x83,0xec,0x28,0xe8,0,0,0,0,0x48,0x83,0xc4,0x28,0xc3});entry_main_patch=entry_off+5;}
    else{text.insert(text.end(),{0xe8,0,0,0,0,0xc3});entry_main_patch=entry_off+1;}
    for(auto&i:st.imports){if(thunk_off.count(i.canonical))continue;text.resize(alignsz(text.size(),st.x64?8:4),0x90);uint32_t off=(uint32_t)text.size();thunk_off[i.canonical]=off;text.insert(text.end(),{0xff,0x25,0,0,0,0});}
    st.outs[0].virtual_size=(uint32_t)text.size();
}

static uint32_t find_global_rva(LinkState&st,const std::vector<std::string>&names){
    for(auto&n:names){auto it=st.globals.find(n);if(it!=st.globals.end())return symbol_rva(st,it->second.obj,st.objs[it->second.obj].symbols[it->second.sym]);}
    std::string tried;for(size_t i=0;i<names.size();++i){if(i)tried+=", ";tried+=names[i];}
    throw std::runtime_error("entry symbol not found; tried: "+tried);
}
static void patch_rel32(std::vector<uint8_t>&data,size_t off,uint32_t place_rva,uint32_t target_rva,int extra=0){int32_t add=(int32_t)r32(data,off);int64_t v=(int64_t)add+(int64_t)target_rva-(int64_t)(place_rva+4u+(uint32_t)extra);if(v<INT32_MIN||v>INT32_MAX)throw std::runtime_error("REL32 out of range");p32(data,off,(uint32_t)(int32_t)v);}

static int link_pe(const std::vector<std::string>&inputs,const std::string&output,const std::string&target,const std::string&entry){
    if(inputs.empty())throw std::runtime_error("no object inputs");LinkState st;st.x64=target=="windows-x86_64";if(!st.x64&&target!="windows-x86")throw std::runtime_error("jauld supports windows-x86_64/windows-x86");st.machine=st.x64?0x8664:0x014c;st.image_base=st.x64?0x140000000ull:0x00400000ull;
    for(auto&p:inputs){auto o=read_coff(p);if(o.machine!=st.machine)throw std::runtime_error("object architecture mismatch: "+p);st.objs.push_back(std::move(o));}
    st.outs={{".text",{},0,0,0,0,0x60000020u},{".rdata",{},0,0,0,0,0x40000040u},{".data",{},0,0,0,0,0xC0000040u},{".idata",{},0,0,0,0,0xC0000040u}};
    for(size_t oi=0;oi<st.objs.size();++oi){auto&o=st.objs[oi];for(size_t si=0;si<o.sections.size();++si){auto&s=o.sections[si];if(skip_section(s))continue;int g=group_for(s);s.out_group=g;auto&out=st.outs[(size_t)g];out.data.resize(alignsz(out.data.size(),group_align(g)),0);s.out_off=(uint32_t)out.data.size();out.data.insert(out.data.end(),s.data.begin(),s.data.end());uint32_t need=s.out_off+std::max<uint32_t>((uint32_t)s.data.size(),s.virtual_size);if(need>out.data.size())out.data.resize(need,0);out.virtual_size=(uint32_t)out.data.size();}}
    for(size_t oi=0;oi<st.objs.size();++oi){auto&o=st.objs[oi];for(uint32_t si=0;si<o.symbols.size();++si){auto&s=o.symbols[si];if(!s.valid||s.aux_entry||s.section<=0||s.storage!=2||s.name.empty())continue;if((size_t)s.section>o.sections.size()||o.sections[(size_t)s.section-1].out_group<0)continue;auto it=st.globals.find(s.name);if(it!=st.globals.end())throw std::runtime_error("duplicate external symbol: "+s.name);st.globals[s.name]={oi,si};}}
    collect_imports(st);std::unordered_map<std::string,uint32_t>thunk_off;uint32_t entry_off=0;size_t main_patch=0;add_entry_and_thunks(st,entry_off,thunk_off,main_patch);auto id=build_idata(st.imports,st.x64);st.outs[3].data=id.bytes;st.outs[3].virtual_size=(uint32_t)id.bytes.size();