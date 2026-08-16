from pathlib import Path
import runpy

runpy.run_path('tools/patch_v090_static.py', run_name='__main__')

# The archive builder uses write_binary before its full definition. Keep a
# forward declaration so the helper stays close to the archive-format code.
p=Path('src/jauc_main.cpp')
s=p.read_text(encoding='utf-8')
needle='static void write_static_archive(const fs::path&object,const fs::path&output,const std::string&target){'
if needle not in s:
    raise SystemExit('static archive function not found after patch')
s=s.replace(needle,'static void write_binary(const fs::path&p,const std::string&data);\n'+needle,1)
p.write_text(s,encoding='utf-8')
