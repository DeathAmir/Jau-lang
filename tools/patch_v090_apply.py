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

# std::system goes through cmd.exe on Windows and has fragile first-token
# quoting rules. JauM must work when installed under paths such as Program Files,
# so launch jauc directly with CreateProcess and return its real exit code.
p = Path("src/jaum_main.cpp")
s = p.read_text(encoding="utf-8")
old = '''static int run(const fs::path&exe,const std::vector<std::string>&args,bool verbose){std::string cmd=quote(exe.string());for(auto&a:args)cmd+=" "+quote(a);if(verbose)std::cerr<<"[jaum] "<<cmd<<"\\n";return std::system(cmd.c_str());}'''
new = '''static int run(const fs::path&exe,const std::vector<std::string>&args,bool verbose){std::string cmd=quote(exe.string());for(auto&a:args)cmd+=" "+quote(a);if(verbose)std::cerr<<"[jaum] "<<cmd<<"\\n";
#ifdef _WIN32
    std::vector<char> mutable_cmd(cmd.begin(),cmd.end());mutable_cmd.push_back('\\0');STARTUPINFOA si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};std::string app=exe.string();
    if(!CreateProcessA(app.c_str(),mutable_cmd.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi)){std::cerr<<"[jaum] CreateProcess failed, win32="<<GetLastError()<<"\\n";return 127;}
    WaitForSingleObject(pi.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;
#else
    return std::system(cmd.c_str());
#endif
}'''
if old not in s:
    raise SystemExit("JauM run() patch anchor not found")
s = s.replace(old,new,1)
p.write_text(s,encoding="utf-8")
