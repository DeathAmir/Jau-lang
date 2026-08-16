from pathlib import Path

src = Path('tools/patch_v090_shared2.py').read_text(encoding='utf-8')
old = '''def rep(s,a,b,label):\n    if a not in s: raise SystemExit('anchor not found: '+label)\n    return s.replace(a,b,1)\n'''
new = r'''def rep(s,a,b,label):
    if a in s:
        return s.replace(a,b,1)
    # jauld_part02.inc is intentionally dense/minified. Match the semantic
    # call sequence instead of indentation/newlines for this one mutation.
    if label == 'PE export bytes':
        x = 'add_entry_and_thunks(st,entry_off,thunk_off,main_patch);auto id=build_idata(st.imports,st.x64);st.outs[3].data=id.bytes;st.outs[3].virtual_size=(uint32_t)id.bytes.size();'
        y = 'add_entry_and_thunks(st,entry_off,thunk_off,main_patch,!dll_mode);auto id=build_idata(st.imports,st.x64);st.outs[3].data=id.bytes;st.outs[3].virtual_size=(uint32_t)id.bytes.size();auto ed=build_edata(exports,fs::path(output).filename().string());st.outs[4].data=ed.bytes;st.outs[4].virtual_size=(uint32_t)ed.bytes.size();'
        if x in s:
            return s.replace(x,y,1)
    raise SystemExit('anchor not found: '+label)
'''
if old not in src:
    raise SystemExit('shared patch rep() definition not found')
src = src.replace(old,new,1)
exec(compile(src, 'tools/patch_v090_shared2.py', 'exec'), {'__name__':'__main__'})
