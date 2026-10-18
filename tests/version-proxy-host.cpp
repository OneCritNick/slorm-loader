#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winver.h>
#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <fstream>
#include <stdexcept>
unsigned checks=0;
void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=2)std::wcerr<<L"argc="<<argc<<L" command="<<GetCommandLineW()<<L"\n";
    Check(argc==2,"argument: loaded|missing|other|ual|ual_native|ual_priority|ual_running|ual_missing");std::wstring mode=argv[1];const bool ual=mode.starts_with(L"ual");
    const bool priority=mode==L"ual_priority"||mode==L"ual_running";
    const bool native=mode==L"ual_native"||priority;
    wchar_t module[32768];GetModuleFileNameW(nullptr,module,32768);std::wstring root=module;root.resize(root.find_last_of(L'\\'));
    const auto expected=root+(ual?L"\\plugins\\AurieCore.asi":L"\\mods\\Native\\AurieCore.dll");
    HMODULE proxy=GetModuleHandleW((root+L"\\version.dll").c_str());Check(proxy!=nullptr,"static version import loaded proxy");
    GetModuleFileNameW(proxy,module,32768);Check(_wcsicmp(module,(root+L"\\version.dll").c_str())==0,"application imported deployed proxy");
    wchar_t systemPath[32768];GetSystemDirectoryW(systemPath,32768);std::wstring realPath=std::wstring(systemPath)+L"\\version.dll";
    HMODULE real=LoadLibraryExW(realPath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Check(real&&real!=proxy,"distinct real System32 version library");
    const char* names[]={"GetFileVersionInfoA","GetFileVersionInfoByHandle","GetFileVersionInfoExA","GetFileVersionInfoExW","GetFileVersionInfoSizeA","GetFileVersionInfoSizeExA","GetFileVersionInfoSizeExW","GetFileVersionInfoSizeW","GetFileVersionInfoW","VerFindFileA","VerFindFileW","VerInstallFileA","VerInstallFileW","VerLanguageNameA","VerLanguageNameW","VerQueryValueA","VerQueryValueW"};
    for(unsigned i=0;i<17;++i){auto fn=GetProcAddress(proxy,names[i]);Check(fn&&GetProcAddress(real,names[i]),"every named Windows export available");if(!ual)Check(fn==GetProcAddress(proxy,MAKEINTRESOURCEA(i+1)),"Windows ordinals preserved");}
    std::wstring kernel=std::wstring(systemPath)+L"\\kernel32.dll";
    auto realSize=reinterpret_cast<decltype(&GetFileVersionInfoSizeW)>(GetProcAddress(real,"GetFileVersionInfoSizeW"));
    const DWORD size=realSize(kernel.c_str(),nullptr);Check(size>0,"known Windows version resource");
    std::vector<std::thread> threads;volatile LONG bad=0;
    for(unsigned i=0;i<8;++i)threads.emplace_back([&]{for(unsigned j=0;j<20;++j)if(GetFileVersionInfoSizeW(kernel.c_str(),nullptr)!=size)InterlockedIncrement(&bad);});
    for(auto& t:threads)t.join();Check(!bad,"concurrent lazy forwarding");
    std::vector<unsigned char> info(size),control(size);auto realInfo=reinterpret_cast<decltype(&GetFileVersionInfoW)>(GetProcAddress(real,"GetFileVersionInfoW"));
    Check(GetFileVersionInfoW(kernel.c_str(),0,size,info.data())&&realInfo(kernel.c_str(),0,size,control.data()),"W version API forwards all four arguments");
    VS_FIXEDFILEINFO* file=nullptr;UINT length=0;Check(VerQueryValueW(info.data(),L"\\",reinterpret_cast<void**>(&file),&length)&&length==sizeof(*file)&&file->dwSignature==0xFEEF04BD,"W version query forwards output pointers");
    auto savedVersion=file->dwFileVersionMS;Check(VerQueryValueW(control.data(),L"\\",reinterpret_cast<void**>(&file),&length)&&file->dwFileVersionMS==savedVersion,"forwarded data agrees with System32");
    char ansi[32768];WideCharToMultiByte(CP_ACP,0,kernel.c_str(),-1,ansi,32768,nullptr,nullptr);
    DWORD aSize=GetFileVersionInfoSizeA(ansi,nullptr);Check(aSize>0,"ANSI version size");info.resize(aSize);Check(GetFileVersionInfoA(ansi,0,aSize,info.data()),"ANSI version info");
    Check(VerQueryValueA(info.data(),"\\",reinterpret_cast<void**>(&file),&length)&&file->dwSignature==0xFEEF04BD,"ANSI query output");
    DWORD xSize=GetFileVersionInfoSizeExW(FILE_VER_GET_NEUTRAL,kernel.c_str(),nullptr);Check(xSize>0,"extended version size");info.resize(xSize);
    Check(GetFileVersionInfoExW(FILE_VER_GET_NEUTRAL,kernel.c_str(),0,xSize,info.data()),"extended API forwards fifth stack argument");
    wchar_t lang[256],realLang[256];auto realLanguage=reinterpret_cast<decltype(&VerLanguageNameW)>(GetProcAddress(real,"VerLanguageNameW"));
    Check(VerLanguageNameW(0x409,lang,256)==realLanguage(0x409,realLang,256)&&std::wstring(lang)==realLang,"forwarded kernel export");
    using Find=decltype(&VerFindFileW);auto actualFind=reinterpret_cast<Find>(GetProcAddress(real,"VerFindFileW"));
    wchar_t current[32768],out[32768],realCurrent[32768],realOut[32768];UINT cl=32768,ol=32768,rcl=32768,rol=32768;
    DWORD result=VerFindFileW(0,L"kernel32.dll",systemPath,root.c_str(),current,&cl,out,&ol);
    DWORD realResult=actualFind(0,L"kernel32.dll",systemPath,root.c_str(),realCurrent,&rcl,realOut,&rol);
    Check(result==realResult&&cl==rcl&&ol==rol&&std::wstring(current)==realCurrent&&std::wstring(out)==realOut,"eight-argument API preserves stack and outputs");
    if(mode==L"ual_running") Check(LoadLibraryW(expected.c_str())!=nullptr,"direct Core startup in an already running process");
    if(mode==L"ual_priority"){
        std::ifstream phases((root+L"\\priority-phases.txt").c_str());
        std::string text((std::istreambuf_iterator<char>(phases)),{});
        Check(text.starts_with("entrypoint\npreinitialize\nnested-preinitialize\n"),"ASI bootstrap blocks caller until early phases finish");
    }
    for(unsigned i=0;i<500&&!GetModuleHandleW(expected.c_str());++i){if(mode!=L"loaded"&&mode!=L"ual"&&!native)break;Sleep(10);}
    HMODULE core=GetModuleHandleW(expected.c_str());
    if(mode==L"loaded"||mode==L"ual"||native){
        Check(core!=nullptr,"worker loaded Core from application directory despite different current directory");
        GetModuleFileNameW(core,module,32768);Check(_wcsicmp(module,expected.c_str())==0,"absolute bootstrap target");
        if(native){
            bool initialized=false;const auto deadline=GetTickCount64()+5000;while(!initialized&&GetTickCount64()<deadline){std::ifstream file((root+L"\\aurie.log").c_str());std::string text((std::istreambuf_iterator<char>(file)),{});initialized=text.find("[ArProcessAttach] Init done.")!=std::string::npos;if(!initialized&&mode!=L"ual_running")Sleep(10);}
            Check(initialized,"actual Aurie Core completed initialization through ASI bootstrap");
            if(priority){
                std::ifstream phases((root+L"\\priority-phases.txt").c_str());std::string text((std::istreambuf_iterator<char>(phases)),{});
                Check(text=="entrypoint\npreinitialize\nnested-preinitialize\ninitialize\nnested-initialize\n","early, nested and late callbacks run once in the correct order");
            }
            HANDLE report=CreateFileW((root+L"\\native-proxy-result.txt").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,0,nullptr);
            if(report!=INVALID_HANDLE_VALUE){const char* text="PASS actual Aurie Core ASI startup with unchanged surrogate EXE\n";DWORD written;WriteFile(report,text,DWORD(lstrlenA(text)),&written,nullptr);CloseHandle(report);}
        }else {auto probe=reinterpret_cast<int(*)()>(GetProcAddress(core,"SlormCoreProbe"));Check(probe&&probe()==73,"probe Core attached normally");}
    }else Check(!core,"no Core for absent dependency or unrelated process");
    // Give the missing-Core worker time to retain diagnostics before this tiny
    // test process exits. This is a compiled surrogate, never the game binary.
    if(mode==L"missing")Sleep(300);
    std::cout<<"PASS "<<checks<<" proxy forwarding/startup checks mode="<<(mode==L"loaded"?"loaded":mode==L"missing"?"missing":"other")<<"\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
