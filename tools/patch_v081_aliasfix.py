from pathlib import Path
p=Path('src/jau_part01.inc')
s=p.read_text(encoding='utf-8')
old='''            auto it=kw.find(x); t.push_back({it==kw.end()?TK::Id:it->second,x,ln}); continue;}'''
new='''            auto it=kw.find(x);std::string tx=x;if(x=="and")tx="&&";else if(x=="or")tx="||";else if(x=="not")tx="!";t.push_back({it==kw.end()?TK::Id:it->second,tx,ln}); continue;}'''
if old not in s: raise SystemExit('keyword token text anchor missing')
s=s.replace(old,new,1)
p.write_text(s,encoding='utf-8')
