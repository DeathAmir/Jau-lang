from pathlib import Path

src = Path('tools/patch_v090_shared2.py').read_text(encoding='utf-8')
old = '''def rep(s,a,b,label):\n    if a not in s: raise SystemExit('anchor not found: '+label)\n    return s.replace(a,b,1)\n'''
new = r'''def rep(s,a,b,label):
    if a in s:
        return s.replace(a,b,1)
    # jauld/jauc sources contain dense generated-style lines in a few places.
    # Match their semantic core when a full formatted line is too brittle.
    if label == 'PE export bytes':
        x = 'add_entry_and_thunks(st,entry_off,thunk_off,main_patch);auto id=build_idata(st.imports,st.x64);st.outs[3].data=id.bytes;st.outs[3].virtual_size=(uint32_t)id.bytes.size();'
        y = 'add_entry_and_thunks(st,entry_off,thunk_off,main_patch,!dll_mode);auto id=build_idata(st.imports,st.x64);st.outs[3].data=id.bytes;st.outs[3].virtual_size=(uint32_t)id.bytes.size();auto ed=build_edata(exports,fs::path(output).filename().string());st.outs[4].data=ed.bytes;st.outs[4].virtual_size=(uint32_t)ed.bytes.size();'
        if x in s:
            return s.replace(x,y,1)
    if label == 'PE link report':
        x = '<<out.size()<<" bytes, "<<nsec<<" sections, entry="<<entry_names.front()<<")\\n";return 0;'
        y = '<<out.size()<<" bytes, "<<nsec<<" sections"<<(dll_mode?std::string(", exports=")+std::to_string(exports.size()):std::string(", entry=")+entry_names.front())<<")\\n";return 0;'
        if x in s:
            return s.replace(x,y,1)
    if label == 'jauld DLL help':
        x = '[--subsystem console|windows]\\n\\n--system-lib'
        y = '[--subsystem console|windows] [--dll --export public[=internal]]\\n\\n--system-lib'
        if x in s:
            return s.replace(x,y,1)
    if label == 'jauc shared help':
        x = '              << "  jauc targets\\n"'
        y = '              << "  jauc shared <file.jau> -o library --target <target> --export name [--export public=internal] [--system-lib name]\\n"\n              << "  jauc targets\\n"'
        if x in s:
            return s.replace(x,y,1)
    raise SystemExit('anchor not found: '+label)
'''
if old not in src:
    raise SystemExit('shared patch rep() definition not found')
src = src.replace(old,new,1)
exec(compile(src, 'tools/patch_v090_shared2.py', 'exec'), {'__name__':'__main__'})
