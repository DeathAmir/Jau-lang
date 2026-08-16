from pathlib import Path
import runpy

runpy.run_path('tools/patch_v090_docs.py', run_name='__main__')

# The GUI documentation should use a command that deterministically produces the
# object filename consumed by jauc. `cl /Fo:` does that directly in a VS dev
# prompt; CI still builds the same source through CMake and discovers the object.
for name in ['README.md','docs/WINDOWS_GUI.md']:
    p=Path(name); s=p.read_text(encoding='utf-8')
    old='''cmake -S examples\\windows_gui -B gui-native -A x64\ncmake --build gui-native --config Release\ncopy gui-native\\Release\\jau_window.obj examples\\windows_gui\\window.obj\n'''
    new='''cl /nologo /c /EHsc examples\\windows_gui\\window.cpp /Fo:examples\\windows_gui\\window.obj\n'''
    if old in s: s=s.replace(old,new)
    p.write_text(s,encoding='utf-8')

# The target table should distinguish validated shared/static combinations from
# merely available x86 codegen. Linux x86 native EXE is covered by main CI, while
# shared/static release validation in 0.9 is x64 Linux + x86/x64 Windows.
p=Path('README.md'); s=p.read_text(encoding='utf-8')
s=s.replace('| `linux-x86` | ✅ | ELF `.o` | ELF | toolchain path* | archive path* |','| `linux-x86` | ✅ | ELF `.o` | ELF | not release-gated | not release-gated |')
p.write_text(s,encoding='utf-8')
