#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
namespace fs=std::filesystem;
static fs::path current_executable(const char* argv0){
#ifdef _WIN32
    std::vector<char> buf(32768);
    DWORD n=GetModuleFileNameA(nullptr,buf.data(),(DWORD)buf.size());
    if(n>0&&n<buf.size())return fs::path(std::string(buf.data(),n));
#endif
    std::error_code ec;auto p=fs::absolute(argv0?argv0:"",ec);return ec?fs::path(argv0?argv0:""):p;
}
static void copy_if(const fs::path&a,const fs::path&b){if(!fs::exists(a))return;fs::create_directories(b.parent_path());fs::copy_file(a,b,fs::copy_options::overwrite_existing);}
static std::string q(const std::string&s){return "\""+s+"\"";}
int main(int argc,char**argv){
    try{
        fs::path self=current_executable(argv[0]), root=self.parent_path(), source=root;
        fs::path prefix;
#ifdef _WIN32
        const char* la=std::getenv("LOCALAPPDATA");prefix=la?fs::path(la)/"Jau":fs::current_path()/"Jau";
#else
        const char* home=std::getenv("HOME");prefix=home?fs::path(home)/".local"/"jau":fs::current_path()/".jau";
#endif
        bool patch=true;
        for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--prefix"&&i+1<argc)prefix=argv[++i];else if(a=="--source-root"&&i+1<argc)source=argv[++i];else if(a=="--no-path")patch=false;}
        fs::create_directories(prefix/"bin");fs::create_directories(prefix/"stdlib");fs::create_directories(prefix/"tools");fs::create_directories(prefix/"packages");
#ifdef _WIN32
        copy_if(root/"jauc.exe",prefix/"bin"/"jauc.exe");copy_if(root/"jur.exe",prefix/"bin"/"jur.exe");copy_if(root/"jauas.exe",prefix/"bin"/"jauas.exe");copy_if(root/"jauld.exe",prefix/"bin"/"jauld.exe");copy_if(root/"jau-setup.exe",prefix/"bin"/"jau-setup.exe");copy_if(root/"jaupm.exe",prefix/"bin"/"jaupm.exe");
#else
        copy_if(root/"jauc",prefix/"bin"/"jauc");copy_if(root/"jur",prefix/"bin"/"jur");copy_if(root/"jauas",prefix/"bin"/"jauas");copy_if(root/"jauld",prefix/"bin"/"jauld");copy_if(root/"jau-setup",prefix/"bin"/"jau-setup");copy_if(root/"jaupm",prefix/"bin"/"jaupm");
#endif
        if(fs::exists(source/"stdlib"))fs::copy(source/"stdlib",prefix/"stdlib",fs::copy_options::recursive|fs::copy_options::overwrite_existing);
        copy_if(source/"tools"/"jaupm.jau",prefix/"tools"/"jaupm.jau");
#ifdef _WIN32
        {std::error_code ec;fs::remove(prefix/"bin"/"jaupm.cmd",ec);}
        if(patch){HKEY key; if(RegOpenKeyExA(HKEY_CURRENT_USER,"Environment",0,KEY_READ|KEY_WRITE,&key)==ERROR_SUCCESS){std::string ph=prefix.string();RegSetValueExA(key,"JAU_HOME",0,REG_SZ,(const BYTE*)ph.c_str(),(DWORD)ph.size()+1);DWORD type=0,n=0;std::string path;if(RegQueryValueExA(key,"Path",nullptr,&type,nullptr,&n)==ERROR_SUCCESS&&n){path.resize(n);RegQueryValueExA(key,"Path",nullptr,&type,(BYTE*)path.data(),&n);if(!path.empty()&&path.back()=='\0')path.pop_back();}std::string bin=(prefix/"bin").string();std::string next=path;if(path.rfind(bin,0)!=0){next=bin;if(!path.empty())next+=";"+path;}RegSetValueExA(key,"Path",0,REG_EXPAND_SZ,(const BYTE*)next.c_str(),(DWORD)next.size()+1);RegCloseKey(key);SendMessageTimeoutA(HWND_BROADCAST,WM_SETTINGCHANGE,0,(LPARAM)"Environment",SMTO_ABORTIFHUNG,2000,nullptr);}}
#else
        if(!fs::exists(prefix/"bin"/"jaupm")){std::ofstream f(prefix/"bin"/"jaupm");f<<"#!/bin/sh\nexec \"${JAU_HOME:-"<<prefix.string()<<"}/bin/jauc\" run \"${JAU_HOME:-"<<prefix.string()<<"}/tools/jaupm.jau\" -I \"${JAU_HOME:-"<<prefix.string()<<"}/stdlib\" -- \"$@\"\n";}fs::permissions(prefix/"bin"/"jaupm",fs::perms::owner_exec|fs::perms::group_exec|fs::perms::others_exec,fs::perm_options::add);
        if(patch){const char* home=std::getenv("HOME");if(home){fs::path profile=fs::path(home)/".profile";std::string old;if(fs::exists(profile)){std::ifstream in(profile);old.assign(std::istreambuf_iterator<char>(in),{});}std::string mark="# >>> Jau >>>";if(old.find(mark)==std::string::npos){std::ofstream out(profile,std::ios::app);out<<"\n"<<mark<<"\nexport JAU_HOME="<<q(prefix.string())<<"\nexport PATH=\"$JAU_HOME/bin:$PATH\"\n# <<< Jau <<<\n";}}}
#endif
        std::cout<<"Jau installed at "<<prefix<<"\n"<<(patch?"PATH/Jau home configured. Open a new terminal.\n":"PATH was not modified.\n");return 0;
    }catch(const std::exception&e){std::cerr<<"jau-setup: "<<e.what()<<"\n";return 1;}
}
