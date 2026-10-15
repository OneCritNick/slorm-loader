// Full native loader smoke in an isolated hidden GUI process.
// Loads only its copied Core-under-test.dll; never launches the game or mod DLLs.
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include "../src/vendor/nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <string>
#include <atomic>
#include <thread>
#include <vector>

using Json=nlohmann::json;
static std::string Utf8(const std::wstring& text){if(text.empty())return {};int count=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);std::string value(count,'\0');WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),value.data(),count,nullptr,nullptr);return value;}
static std::string Message(DWORD code){wchar_t text[4096]{};DWORD size=FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,text,4096,nullptr);return size?Utf8(std::wstring(text,size)):"Windows did not provide a message";}
static std::string ReadLog(const std::filesystem::path& path){
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return {};
    char bytes[131072];DWORD size{};const BOOL read=ReadFile(file,bytes,sizeof(bytes),&size,nullptr);CloseHandle(file);return read?std::string(bytes,size):std::string();
}
static std::wstring ReadConsole(){
    HANDLE output=CreateFileW(L"CONOUT$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    if(output==INVALID_HANDLE_VALUE)return {};
    CONSOLE_SCREEN_BUFFER_INFO info{};std::wstring result;
    if(GetConsoleScreenBufferInfo(output,&info)){const DWORD limit=static_cast<DWORD>(info.dwSize.X)*static_cast<DWORD>(info.dwCursorPosition.Y+1);result.resize(limit<8192?limit:8192);DWORD read{};if(ReadConsoleOutputCharacterW(output,result.data(),static_cast<DWORD>(result.size()),{0,0},&read))result.resize(read);else result.clear();}
    CloseHandle(output);return result;
}
static HMODULE BoundControls(HMODULE core){
    if(!core)return nullptr;
    const auto image=reinterpret_cast<const unsigned char*>(core);const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
    const auto imports=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;if(!imports)return nullptr;
    for(auto descriptor=reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(image+imports);descriptor->Name;++descriptor){
        if(_stricmp(reinterpret_cast<const char*>(image+descriptor->Name),"comctl32.dll")||!descriptor->OriginalFirstThunk)continue;
        auto names=reinterpret_cast<const IMAGE_THUNK_DATA64*>(image+descriptor->OriginalFirstThunk);auto addresses=reinterpret_cast<const IMAGE_THUNK_DATA64*>(image+descriptor->FirstThunk);
        for(size_t i=0;names[i].u1.Ordinal;++i)if(IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal)&&IMAGE_ORDINAL64(names[i].u1.Ordinal)==345){HMODULE owner=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(addresses[i].u1.Function),&owner);return owner;}
    }
    return nullptr;
}
static BOOL CALLBACK CollectManifestLanguage(HMODULE,LPCWSTR,LPCWSTR,WORD language,LONG_PTR context){reinterpret_cast<std::vector<WORD>*>(context)->push_back(language);return TRUE;}
static int RemoveManifest(const std::filesystem::path& folder,const std::filesystem::path& core){
    std::vector<WORD> languages;
    HMODULE resource=LoadLibraryExW(core.c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if(resource){EnumResourceLanguagesW(resource,MAKEINTRESOURCEW(24),MAKEINTRESOURCEW(2),CollectManifestLanguage,reinterpret_cast<LONG_PTR>(&languages));FreeLibrary(resource);}
    DWORD error=ERROR_RESOURCE_NAME_NOT_FOUND;bool removed=false;
    if(!languages.empty()){
        HANDLE update=BeginUpdateResourceW(core.c_str(),FALSE);error=update?ERROR_SUCCESS:GetLastError();
        if(update){bool okay=true;for(WORD language:languages)if(!UpdateResourceW(update,MAKEINTRESOURCEW(24),MAKEINTRESOURCEW(2),language,nullptr,0)){error=GetLastError();okay=false;break;}
            if(!EndUpdateResourceW(update,!okay)){error=GetLastError();okay=false;}removed=okay;}
    }
    Json result{{"removedManifestResource2",removed},{"languages",languages},{"windowsErrorCode",error},{"windowsErrorMessage",Message(error)},{"scope","Copied fixture DLL only; no DLL code executed"}};
    {std::ofstream output(folder/L"manifest-strip-result.json",std::ios::binary);output<<result.dump(2)<<'\n';output.flush();if(!output)return 2;}
    return removed?0:1;
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR arguments,int){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX|SEM_NOGPFAULTERRORBOX);
    wchar_t image[32768]{};GetModuleFileNameW(nullptr,image,32768);const auto folder=std::filesystem::path(image).parent_path();
    const auto core=folder/L"Core-under-test.dll";const auto log=folder/L"aurie.log";
    if(arguments&&std::wstring_view(arguments).find(L"--remove-manifest")!=std::wstring_view::npos)return RemoveManifest(folder,core);
    Json result{{"schemaVersion",1},{"hostPid",GetCurrentProcessId()},{"core",Utf8(core.wstring())},{"fixture",Utf8(folder.wstring())},{"gameOrModDllsLoaded",false},{"waitLimitMs",3000}};
    // Start before loading Core so DLL loader lock cannot prevent the watchdog
    // thread from entering. It samples only this isolated host's main thread.
    HANDLE mainThread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,GetCurrentThreadId());
    std::atomic<bool> ready=false,loadReturned=false;
    std::thread watchdog([&]{
        ready=true;Sleep(3500);if(loadReturned.load())return;
        Json timeout{{"schemaVersion",1},{"hostPid",GetCurrentProcessId()},{"core",Utf8(core.wstring())},{"fixture",Utf8(folder.wstring())},{"loadSuccess",false},{"loadReturned",false},{"loadErrorCode",ERROR_TIMEOUT},{"loadErrorMessage","LoadLibrary did not return within 3.5 seconds"},{"coreLogBytes",nullptr},{"coreStartupHeader",nullptr},{"manifestResource2Present",nullptr},{"commonControlsOrdinal345Available",nullptr},{"consoleWindowVisible",nullptr},{"consoleVisibleObserved",nullptr}};
        if(mainThread&&SuspendThread(mainThread)!=DWORD(-1)){
            CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL;const BOOL captured=GetThreadContext(mainThread,&context);ResumeThread(mainThread);
            if(captured){MEMORY_BASIC_INFORMATION info{};if(VirtualQuery(reinterpret_cast<void*>(context.Rip),&info,sizeof(info))){wchar_t mapped[32768]{};K32GetMappedFileNameW(GetCurrentProcess(),info.AllocationBase,mapped,32768);timeout["stuckModule"]=Utf8(mapped);timeout["stuckRva"]=context.Rip-reinterpret_cast<uintptr_t>(info.AllocationBase);}}
            FILETIME created{},exited{},kernel{},user{};if(GetThreadTimes(mainThread,&created,&exited,&kernel,&user)){timeout["mainThreadKernel100ns"]=(uint64_t(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime;timeout["mainThreadUser100ns"]=(uint64_t(user.dwHighDateTime)<<32)|user.dwLowDateTime;}
        }
        {std::ofstream output(folder/L"core-load-result.json",std::ios::binary);output<<timeout.dump(2)<<'\n';output.flush();}
        TerminateProcess(GetCurrentProcess(),0);
    });
    while(!ready.load())Sleep(1);
    const auto begin=GetTickCount64();SetLastError(ERROR_SUCCESS);HMODULE module=LoadLibraryW(core.c_str());const DWORD loadError=module?ERROR_SUCCESS:GetLastError();
    loadReturned=true;result["loadReturned"]=true;
    result["loadSuccess"]=module!=nullptr;result["loadErrorCode"]=loadError;result["loadErrorMessage"]=Message(loadError);result["loadMillis"]=GetTickCount64()-begin;
    bool visibleObserved=false;
    if(module){
        // Core's worker waits for a game window after logging early initialization.
        // A dummy host deliberately does not supply that window or gameplay modules.
        while(GetTickCount64()-begin<3000){HWND console=GetConsoleWindow();if(console&&IsWindowVisible(console)){visibleObserved=true;ShowWindow(console,SW_HIDE);}Sleep(10);}
    }
    const auto text=ReadLog(log);result["coreLogExists"]=GetFileAttributesW(log.c_str())!=INVALID_FILE_ATTRIBUTES;result["coreLogBytes"]=text.size();result["coreStartupHeader"]=text.find("Aurie Core v2.0.2")!=std::string::npos;
    result["coreReachedGameWindowWait"]=text.find("[ElWaitForCurrentProcessWindow] Waiting for process window...")!=std::string::npos;
    const HWND console=GetConsoleWindow();result["consoleWindowPresent"]=console!=nullptr;result["consoleWindowVisible"]=console&&IsWindowVisible(console);result["consoleVisibleObserved"]=visibleObserved;result["consoleText"]=Utf8(ReadConsole());result["totalMillis"]=GetTickCount64()-begin;
    // Resource-only mapping does not run DLL code or bind its imports.
    HMODULE resource=LoadLibraryExW(core.c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    HRSRC manifest=resource?FindResourceW(resource,MAKEINTRESOURCEW(2),MAKEINTRESOURCEW(24)):nullptr;
    result["manifestResource2Present"]=manifest!=nullptr;bool controls6=false;
    if(manifest){const DWORD size=SizeofResource(resource,manifest);const void* bytes=LockResource(LoadResource(resource,manifest));if(bytes){const std::string xml(static_cast<const char*>(bytes),size);controls6=xml.find("Microsoft.Windows.Common-Controls")!=std::string::npos&&xml.find("6.0.0.0")!=std::string::npos;}}
    result["manifestCommonControls6"]=controls6;
    HMODULE controls=BoundControls(module);
    result["commonControlsLookupBasis"]=controls?"resolved_core_import_ordinal345":"unactivated_default_dll_lookup";
    if(!controls)controls=LoadLibraryW(L"comctl32.dll");
    wchar_t controlsPath[32768]{};if(controls)GetModuleFileNameW(controls,controlsPath,32768);
    SetLastError(ERROR_SUCCESS);const auto ordinal=controls?GetProcAddress(controls,MAKEINTRESOURCEA(345)):nullptr;const auto ordinalError=ordinal?ERROR_SUCCESS:GetLastError();
    result["commonControlsModule"]=Utf8(controlsPath);result["commonControlsOrdinal345Available"]=ordinal!=nullptr;result["commonControlsOrdinal345Error"]=ordinalError;
    result["scope"]= "LoadLibrary/import binding and early Core startup only; does not prove game compatibility or complete Aurie initialization.";
    {std::ofstream output(folder/L"core-load-result.json",std::ios::binary);output<<result.dump(2)<<'\n';output.flush();if(!output)TerminateProcess(GetCurrentProcess(),2);}
    // Do not invoke FreeLibrary/ModuleUnload in this intentionally incomplete host.
    TerminateProcess(GetCurrentProcess(),0);return 0;
}
