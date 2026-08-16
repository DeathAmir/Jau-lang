from pathlib import Path
p=Path('src/jauld_part02.inc')
s=p.read_text(encoding='utf-8')
old='''    std::unordered_map<std::string,uint32_t> iat_target;\n};'''
new='''    std::unordered_map<std::string,uint32_t> iat_target;\n    std::unordered_map<std::string,uint32_t> builtin_target;\n};'''
if old not in s: raise SystemExit('LinkState tail anchor missing')
s=s.replace(old,new,1)

old='''    if(s.section==-1)return s.value;\n    auto it=st.globals.find(s.name);'''
new='''    if(s.section==-1)return s.value;\n    auto bt=st.builtin_target.find(s.name);if(bt!=st.builtin_target.end())return bt->second;\n    if(!st.x64&&undecorate(s.name)=="allmul"){auto rt=st.builtin_target.find("__allmul");if(rt!=st.builtin_target.end())return rt->second;}\n    auto it=st.globals.find(s.name);'''
if old not in s: raise SystemExit('resolve builtin anchor missing')
s=s.replace(old,new,1)

anchor='''static void add_entry_and_thunks(LinkState&st,uint32_t&entry_off,std::unordered_map<std::string,uint32_t>&thunk_off,size_t&entry_main_patch){'''
helper=r'''static void add_x86_runtime_helpers(LinkState&st){
    if(st.x64)return;
    auto&text=st.outs[0].data;
    text.resize(alignsz(text.size(),16),0x90);
    uint32_t off=(uint32_t)text.size();
    // MSVC x86 __allmul: signed/unsigned low-64 multiplication has the same
    // two's-complement result. Arguments are two 64-bit stack values and the
    // compiler helper returns EDX:EAX while cleaning 16 argument bytes.
    const uint8_t code[]={
        0x53,                         // push ebx
        0x8b,0x44,0x24,0x0c,         // mov eax,[esp+0c]  ; a_hi
        0xf7,0x64,0x24,0x10,         // mul dword [esp+10]; a_hi*b_lo
        0x8b,0xd8,                   // mov ebx,eax
        0x8b,0x44,0x24,0x08,         // mov eax,[esp+08]  ; a_lo
        0xf7,0x64,0x24,0x14,         // mul dword [esp+14]; a_lo*b_hi
        0x03,0xd8,                   // add ebx,eax
        0x8b,0x44,0x24,0x08,         // mov eax,[esp+08]  ; a_lo
        0xf7,0x64,0x24,0x10,         // mul dword [esp+10]; a_lo*b_lo
        0x03,0xd3,                   // add edx,ebx
        0x5b,                         // pop ebx
        0xc2,0x10,0x00                // ret 16
    };
    text.insert(text.end(),std::begin(code),std::end(code));
    st.builtin_target["__allmul"]=off;
    st.builtin_target["_allmul"]=off;
    st.builtin_target["allmul"]=off;
    st.outs[0].virtual_size=(uint32_t)text.size();
}

'''
if anchor not in s: raise SystemExit('add_entry anchor missing')
s=s.replace(anchor,helper+anchor,1)

old='''    collect_imports(st);std::unordered_map<std::string,uint32_t>thunk_off;uint32_t entry_off=0;size_t main_patch=0;add_entry_and_thunks(st,entry_off,thunk_off,main_patch);auto id=build_idata(st.imports,st.x64);'''
new='''    collect_imports(st);std::unordered_map<std::string,uint32_t>thunk_off;uint32_t entry_off=0;size_t main_patch=0;add_entry_and_thunks(st,entry_off,thunk_off,main_patch);add_x86_runtime_helpers(st);auto id=build_idata(st.imports,st.x64);'''
if old not in s: raise SystemExit('helper call anchor missing')
s=s.replace(old,new,1)

old='''uint32_t size_image=align32(rva,sec_align);\n    auto&idata=st.outs[3].data;'''
new='''uint32_t size_image=align32(rva,sec_align);\n    for(auto&kv:st.builtin_target)kv.second+=st.outs[0].rva;\n    auto&idata=st.outs[3].data;'''
if old not in s: raise SystemExit('builtin RVA anchor missing')
s=s.replace(old,new,1)

p.write_text(s,encoding='utf-8')
