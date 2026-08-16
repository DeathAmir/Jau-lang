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

# ar members are padded with one actual LF byte. The raw Python patch text had
# preserved two backslashes, which would create a C++ multi-character literal.
s=s.replace(r"out.push_back('\\n');", r"out.push_back('\n');")
p.write_text(s,encoding='utf-8')

# The shared validation patch created this source in its runner, but the old
# diff-based landing step could not carry untracked files. Keep the common
# library fixture in the repository from this phase onward.
Path('tests/v090_shared.jau').write_text(
    'func add(a:int,b:int):int { return a+b; }\n'
    'func answer():int { return 42; }\n',
    encoding='utf-8'
)
