from pathlib import Path
import runpy

# Older edits left a few C++ string literals using backslash-newline source
# splicing. Besides making patch matching fragile, that also concatenated debug
# lines at runtime. Normalize those source-spliced newlines to an explicit \n.
p = Path("src/jauc_main.cpp")
s = p.read_text(encoding="utf-8")
s = s.replace("\\\n", "\\n")
p.write_text(s, encoding="utf-8")

runpy.run_path("tools/patch_v090_core.py", run_name="__main__")
