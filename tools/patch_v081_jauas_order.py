from pathlib import Path
p=Path('src/jauas_main.cpp')
s=p.read_text(encoding='utf-8')
anchor='''        if(starts(line,"add ")||starts(line,"sub ")||starts(line,"and ")||starts(line,"or ")||starts(line,"xor ")||starts(line,"cmp ")||starts(line,"test ")){'''
insert='''        if(line=="and al,cl"||line=="and al, cl"){o.insert(o.end(),{0x20,0xc8});continue;}\n        if(line=="xor eax,eax"||line=="xor eax, eax"){o.insert(o.end(),{0x31,0xc0});continue;}\n\n'''+anchor
if anchor not in s: raise SystemExit('generic bitwise dispatch anchor missing')
s=s.replace(anchor,insert,1)
p.write_text(s,encoding='utf-8')
