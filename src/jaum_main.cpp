#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace fs=std::filesystem;
static std::string trim(std::string s){size_t a=0,b=s.size();while(a<b&&std::isspace((unsigned char)s[a]))++a;while(b>a&&std::isspace((unsigned char)s[b-1]))--b;return s.substr(a,b-a);}
static std::vector<std::string> split(std::string s){std::vector<std::string>v;std::string x;std::istringstream in(s);while(std::getline(in,x,',')){x=trim(x);if(!x.empty())v.push_back(x);}return v;}
static std::string quote(const std::string&s){
#ifdef _WIN32
 std::string r="\"";for(char c:s){if(c=='\"')r+="\\\"";else r+=c;}return r+"\"";
#else
 std::string r="'";for(char c:s){if(c=='\'')r+="'\\''";else r+=c;}return r+"'";
#endif
}
static fs::path self_path(const char*argv0){
#ifdef _WIN32
 std::vector<char>b(32768);DWORD n=GetModuleFileNameA(nullptr,b.data(),(DWORD)b.size());if(n&&n<b.size())return fs::path(std::string(b.data(),n));
#else
 std::vector<char>b(4096);ssize_t n=::readlink("/proc/self/exe",b.data(),b.size()-1);if(n>0)return fs::path(std::string(b.data(),(size_t)n));
#endif
 std::error_code ec;auto p=fs::absolute(argv0?argv0:"",ec);return ec?fs::path(argv0?argv0:""):p;
}
static std::map<std::string,std::string> config(const fs::path&p){std::ifstream f(p);if(!f)throw std::runtime_error("cannot open "+p.string());std::map<std::string,std::string>m;std::string line;while(std::getline(f,line)){line=trim(line);if(line.empty()||line[0]=='#'||line[0]==';'||line[0]=='[')continue;auto q=line.find('=');if(q==std::string::npos)throw std::runtime_error("invalid jaum.txt line: "+line);auto k=trim(line.substr(0,q)),v=trim(line.substr(q+1));if(v.size()>=2&&((v.front()=='\"'&&v.back()=='\"')||(v.front()=='\''&&v.back()=='\'')))v=v.substr(1,v.size()-2);m[k]=v;}return m;}
static std::string replace_all(std::string s,const std::string&a,const std::string&b){size_t p=0;while(!a.empty()&&(p=s.find(a,p))!=std::string::npos){s.replace(p,a.size(),b);p+=b.size();}return s;}
static int run(const fs::path&exe,const std::vector<std::string>&args,bool verbose){std::string cmd=quote(exe.string());for(auto&a:args)cmd+=" "+quote(a);if(verbose)std::cerr<<"[jaum] "<<cmd<<"\n";
#ifdef _WIN32
    std::vector<char> mutable_cmd(cmd.begin(),cmd.end());mutable_cmd.push_back('\0');STARTUPINFOA si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};std::string app=exe.string();
    if(!CreateProcessA(app.c_str(),mutable_cmd.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi)){std::cerr<<"[jaum] CreateProcess failed, win32="<<GetLastError()<<"\n";return 127;}
    WaitForSingleObject(pi.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;
#else
    return std::system(cmd.c_str());
#endif
}
static std::string ext_for(const std::string&type,const std::string&target){bool win=target.rfind("windows-",0)==0;if(type=="exe")return win?".exe":"";if(type=="shared")return win?".dll":".so";if(type=="obj")return win?".obj":".o";if(type=="asm")return ".s";return "";}
static int build_one(const fs::path&cfg,const std::string&override_target){auto m=config(cfg);auto get=[&](const std::string&k,const std::string&d=""){auto i=m.find(k);return i==m.end()?d:i->second;};std::string name=get("name","app"),source=get("source","src/main.jau"),type=get("type","exe"),target=override_target.empty()?get("target","linux-x86_64"):override_target,opt=get("optimize","0");if(type=="native")type="exe";if(type!="exe"&&type!="shared"&&type!="obj"&&type!="asm")throw std::runtime_error("type must be exe, shared, obj or asm in JauM 0.9");std::string out=get("output");if(out.empty())out=(fs::path(get("build_dir","build"))/(name+"-"+target+ext_for(type,target))).string();out=replace_all(out,"{name}",name);out=replace_all(out,"{target}",target);out=replace_all(out,"{ext}",ext_for(type,target));if(fs::path(out).has_parent_path())fs::create_directories(fs::path(out).parent_path());auto self=self_path(nullptr);auto jauc=self.parent_path()/
#ifdef _WIN32
"jauc.exe";
#else
"jauc";
#endif
if(!fs::exists(jauc))throw std::runtime_error("jauc not found next to jaum: "+jauc.string());std::vector<std::string>a; a.push_back(type=="exe"?"native":type);a.push_back(source);a.push_back("-o");a.push_back(out);a.push_back("--target");a.push_back(target);a.push_back("-O"+opt);for(auto&x:split(get("links"))){a.push_back("--link");a.push_back(x);}for(auto&x:split(get("system_libs"))){a.push_back("--system-lib");a.push_back(x);}for(auto&x:split(get("imports"))){a.push_back("--import");a.push_back(x);}for(auto&x:split(get("exports"))){a.push_back("--export");a.push_back(x);}for(auto&x:split(get("include_paths"))){a.push_back("-I");a.push_back(x);}if(type=="exe"){a.push_back("--subsystem");a.push_back(get("subsystem","console"));}if(get("debug","false")=="true")a.push_back("--debug");int rc=run(jauc,a,true);if(rc!=0)std::cerr<<"[jaum] build failed for "<<target<<"\n";else std::cout<<"JauM built "<<out<<"\n";return rc;}
static void usage(){std::cout<<"JauM 0.9\nusage: jaum init [name] | jaum build [-f jaum.txt] [--target target] | jaum build-all [-f jaum.txt] | jaum clean [-f jaum.txt] | jaum show [-f jaum.txt]\n";}
int main(int argc,char**argv){std::cerr<<"DeathAmir Jau @ DeathAmir 2026 (C)\n";try{if(argc<2){usage();return 0;}std::string cmd=argv[1];fs::path file="jaum.txt";std::string target;for(int i=2;i<argc;++i){std::string a=argv[i];if(a=="-f"&&i+1<argc)file=argv[++i];else if(a=="--target"&&i+1<argc)target=argv[++i];}
if(cmd=="init"){std::string name=argc>2?argv[2]:fs::current_path().filename().string();if(fs::exists(file))throw std::runtime_error(file.string()+" already exists");std::ofstream o(file);o<<"# JauM project file\nname="<<name<<"\nsource=src/main.jau\ntype=exe\ntarget=windows-x86_64\n# targets=windows-x86_64,windows-x86,linux-x86_64,linux-x86\noutput=build/{name}-{target}{ext}\noptimize=0\nsubsystem=console\nlinks=\nsystem_libs=\nimports=\ninclude_paths=stdlib\n";fs::create_directories("src");if(!fs::exists("src/main.jau")){std::ofstream j("src/main.jau");j<<"func main() {\n    print(\"Hello from JauM\");\n    return 0;\n}\n";}std::cout<<"created "<<file.string()<<"\n";return 0;}
if(cmd=="build")return build_one(file,target);
if(cmd=="build-all"){auto m=config(file);auto it=m.find("targets");if(it==m.end()||split(it->second).empty())throw std::runtime_error("build-all requires targets=... in jaum.txt");int bad=0;for(auto&t:split(it->second))if(build_one(file,t)!=0)bad=1;return bad;}
if(cmd=="clean"){auto m=config(file);auto it=m.find("build_dir");fs::path d=it==m.end()?"build":it->second;std::error_code ec;fs::remove_all(d,ec);if(ec)throw std::runtime_error("cannot clean "+d.string()+": "+ec.message());std::cout<<"cleaned "<<d.string()<<"\n";return 0;}
if(cmd=="show"){for(auto&kv:config(file))std::cout<<kv.first<<"="<<kv.second<<"\n";return 0;}usage();return 2;}catch(const std::exception&e){std::cerr<<"jaum: "<<e.what()<<"\n";return 1;}}
