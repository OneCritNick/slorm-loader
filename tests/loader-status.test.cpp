#define NOMINMAX
#include "../src/SlormLoaderStatus.hpp"
#include "../src/SlormRuntime.hpp"
#include <iostream>
#include <chrono>
#include <climits>
using namespace Slorm;
static unsigned checks=0;
static void Check(bool b,const char* message){if(!b)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
int main(){try{const auto root=std::filesystem::current_path();
        {
            const std::string complete="[12:00:00] [trace] Aurie Core v2.0.2 loaded at 123\n[12:00:01] [trace] [ArProcessAttach] Init done.\n";
            Check(AurieStartupFinished(complete,true),"current-session Aurie completion marker confirms native startup");
            Check(!AurieStartupFinished(complete,false),"old Aurie log cannot finalize native status");
            Check(!AurieStartupFinished("[ArProcessAttach] Init done.\n",true),"completion text without current Aurie startup header is not proof");
            Check(!AurieStartupFinished(complete.substr(0,complete.size()-1),true),"partial final completion line remains pending");
            const std::string failed="[trace] [MdMapImageEx] MdpMapImage(Example.dll) returns AURIE_SUCCESS (GLE 0)\n[trace] MdpMapImage(Example.dll) returns AURIE_OBJECT_ALREADY_EXISTS\n[warning] [ArProcessAttach] Module \"Example.dll\" failed ModuleInitialize with status AURIE_MODULE_DEPENDENCY_NOT_RESOLVED and will be purged.\n";
            Check(AurieFailureStatus(failed,"Example.dll",true)=="AURIE_MODULE_DEPENDENCY_NOT_RESOLVED","native initialization failure retains exact current-log status");
            Check(AurieFailureStatus(failed,"Example.dll",false).empty(),"duplicate DLL basenames never inherit ambiguous log errors");
            Check(AurieFailureStatus(failed,"Other.dll",true).empty(),"one DLL cannot inherit another DLL's failure");
            Check(AurieFailureStatus("MdpMapImage(Example.dll) returns AURIE_SUCCESS\nMdpMapImage(Example.dll) returns AURIE_OBJECT_ALREADY_EXISTS\n","Example.dll",true).empty(),"successful and already-mapped log events are not failures");
            NativeEvidence valid;valid.architectureRead=true;valid.architecture=0x8664;valid.frameworkInit=true;valid.moduleInit=true;valid.lookupStatus="AURIE_OBJECT_NOT_FOUND";
            auto initialized=valid;initialized.registered=true;initialized.initialized=true;
            Check(NativeStatus("mods/Aurie/Example.dll",initialized)["status"]=="loaded","native loaded status requires confirmed Aurie initialization");
            initialized.initialized=false;Check(NativeStatus("mods/Aurie/Example.dll",initialized)["status"]=="not_initialized","registered but uninitialized DLL is not counted as loaded");
            auto proxy=valid;proxy.frameworkInit=false;proxy.moduleInit=false;
            auto rejected=NativeStatus("mods/Aurie/SlormReforger.dll",proxy);
            Check(rejected["status"]=="rejected"&&rejected["aurieStatus"]=="AURIE_INVALID_SIGNATURE"&&rejected["reason"].get<std::string>().find("__AurieFrameworkInit")!=std::string::npos,"proxy DLL missing Aurie exports has a readable signature rejection");
            auto x86=valid;x86.architecture=0x14c;Check(NativeStatus("mods/Aurie/Example.dll",x86)["aurieStatus"]=="AURIE_INVALID_ARCH","wrong architecture is reported distinctly");
            auto unreadable=valid;unreadable.architectureRead=false;unreadable.inspectionStatus="AURIE_ACCESS_DENIED";Check(NativeStatus("mods/Aurie/Example.dll",unreadable)["status"]=="inspection_failed","unreadable DLL is reported as an inspection failure");
            auto preinit=valid;preinit.moduleInit=false;preinit.modulePreinit=true;Check(NativeStatus("mods/Aurie/Example.dll",preinit)["status"]=="not_loaded","preinitialize-only Aurie modules pass export compatibility inspection");
            auto unknown=NativeStatus("mods/Aurie/Example.dll",valid);Check(unknown["status"]=="not_loaded"&&unknown["evidence"]=="aurie_lookup","absent compatible DLL does not invent an initialization failure reason");
            auto failure=valid;failure.originalFailure="AURIE_MODULE_DEPENDENCY_NOT_RESOLVED";Check(NativeStatus("mods/Aurie/Example.dll",failure)["evidence"]=="aurie_log","known original failure is attributed to its log evidence");
            Check(NativeStatus("mods/Aurie/Example.DLL",valid)["reason"].get<std::string>().find("lowercase .dll")!=std::string::npos,"uppercase manual DLL extension explains Aurie's discovery limitation");
            Check(!NativeCandidate("YYToolkit.dll")&&!NativeCandidate("SLORMLOADER.DLL")&&!NativeCandidate("AurieCore.dll")&&NativeCandidate("Example.DLL"),"framework DLLs are excluded case-insensitively from gameplay status");
            const auto fixture=root/"tests"/("native-status-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(fixture/"mods/Aurie/nested");
            struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}} cleanup{fixture};
            for(const auto& name:{"Example.dll","nested/Example.dll","Upper.DLL","YYToolkit.dll","SlormLoader.dll","AurieCore.dll","README.txt"}){std::ofstream out(fixture/"mods/Aurie"/name);out<<"fixture";}
            unsigned observations=0;auto pending=ScanNativeMods(fixture,false,[&](const auto&,bool){++observations;return valid;},"Waiting for startup");
            Check(observations==0&&pending.items.size()==3&&pending.items[0]["status"]=="pending","pending native scan enumerates candidates without touching Aurie's module list");
            unsigned ambiguous=0;auto scan=ScanNativeMods(fixture,true,[&](const auto& path,bool unique){++observations;if(!unique)++ambiguous;return path.filename()=="Upper.DLL"?valid:initialized;},"");
            Check(observations==3&&ambiguous==2&&scan.complete,"post-startup scan observes each candidate and detects duplicate basenames");
            Check(scan.items[0]["id"]!=scan.items[2]["id"]&&scan.items[0]["path"]!=scan.items[2]["path"],"nested DLLs retain distinct path-based identities");
            auto isolated=ScanNativeMods(fixture,true,[&](const auto& path,bool){if(path.parent_path().filename()=="nested")throw std::runtime_error("Injected inspection failure");return valid;},"");
            Check(isolated.items.size()==3&&isolated.items[2]["status"]=="inspection_failed","one DLL inspection failure preserves other native candidates");
            std::filesystem::create_hard_link(fixture/"mods/Aurie/Example.dll",fixture/"mods/Aurie/Alias.dll");
            auto aliases=ScanNativeMods(fixture,true,[&](const auto& path,bool){auto observed=valid;observed.registered=true;observed.initialized=true;observed.moduleIdentity=std::filesystem::equivalent(path,fixture/"mods/Aurie/Example.dll")?1:2;return observed;},"");
            Check(std::count_if(aliases.items.begin(),aliases.items.end(),[](const Json& item){return item["status"]=="loaded";})==2&&std::count_if(aliases.items.begin(),aliases.items.end(),[](const Json& item){return item["status"]=="alias";})==2,"multiple DLL paths sharing an Aurie module are counted once per module identity");
            std::filesystem::create_hard_link(fixture/"mods/Aurie/YYToolkit.dll",fixture/"mods/Aurie/ToolkitAlias.dll");
            auto noFrameworkAlias=ScanNativeMods(fixture,false,[&](const auto&,bool){throw std::runtime_error("No lookup allowed");return valid;},"Waiting");
            Check(noFrameworkAlias.items.size()==4,"hardlink aliases of pinned framework DLLs are excluded before lookup");
            auto registeredFramework=ScanNativeMods(fixture,true,[&](const auto&,bool){auto observed=valid;observed.registered=true;observed.initialized=true;observed.registeredFilename="YYToolkit.dll";return observed;},"");
            Check(registeredFramework.items.empty(),"registered framework identity cannot be counted under a gameplay alias filename");
            Json jsonItems=Json::array({Json{{"id","json-ok"},{"status","applied"}},Json{{"id","json-blocked"},{"status","blocked"}},Json{{"id","json-invalid"},{"status","invalid"}}});
            Json nativeItems=Json::array({NativeStatus("mods/Aurie/Loaded.dll",NativeEvidence{true,true}),rejected,pending.items[0]});
            auto report=ModStatusReport(jsonItems,nativeItems,123,false);
            Check(report["active"]==1&&report["errors"]==3&&report["pending"]==1,"native totals and rejected JSON remain separate");
            Check(report["nativeActive"]==1&&report["unsupported"]==3&&report["mods"].size()==6,"native and unsupported entries are retained without applying JSON");
            Check(report["nativeScanState"]=="pending"&&ModStatusReport(jsonItems,Json::array(),123,false,true)["nativeScanState"]=="incomplete","pending startup and incomplete filesystem scan remain distinct");
            Check(ModStatusReport(Json::array(),Json::array(),123,true)["active"]==0&&ModStatusReport(Json::array(),Json::array(),123,true)["nativeScanState"]=="complete","empty successful native scan reports zero mods without errors");
            const auto broken=fixture/"broken";std::filesystem::create_directories(broken/"mods");{std::ofstream out(broken/"mods/Aurie");out<<"not a directory";}
            auto badScan=ScanNativeMods(broken,true,[&](const auto&,bool){return valid;},"");
            Check(!badScan.complete&&badScan.items.size()==1&&StatusError(badScan.items[0]),"native root scan failures are visible instead of silently becoming zero errors");
            FILETIME creation{},exited{},kernel{},user{};
            if(!GetProcessTimes(GetCurrentProcess(),&creation,&exited,&kernel,&user))throw std::runtime_error("Cannot read fixture process creation time (Windows error "+std::to_string(GetLastError())+").");
            ULARGE_INTEGER time{};time.LowPart=creation.dwLowDateTime;time.HighPart=creation.dwHighDateTime;
            if(time.QuadPart<10000000||time.QuadPart>ULLONG_MAX-10000000)throw std::runtime_error("Fixture process creation time cannot be offset safely.");
            time.QuadPart+=10000000;const FILETIME fresh{time.LowPart,time.HighPart};
            time.QuadPart-=20000000;const FILETIME old{time.LowPart,time.HighPart};
            // Set explicit times around process creation instead of depending on filesystem wall-clock precision.
            const auto log=fixture/"aurie.log";{std::ofstream out(log);out<<complete;}
            const auto stampLog=[&](const FILETIME& stamp){
                const auto handle=CreateFileW(log.c_str(),FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open fixture timestamp (Windows error "+std::to_string(GetLastError())+").");
                const bool changed=SetFileTime(handle,nullptr,nullptr,&stamp);const auto error=GetLastError();CloseHandle(handle);
                if(!changed)throw std::runtime_error("Cannot set fixture timestamp (Windows error "+std::to_string(error)+").");
            };
            stampLog(fresh);
            std::string text,reason;const bool currentRead=SlormReadCurrentAurieLog(fixture,text,reason);
            Check(currentRead&&AurieStartupFinished(text,true),("actual current-process log can be read with shared access"+(currentRead?std::string():": "+reason)).c_str());
            stampLog(old);
            Check(!SlormReadCurrentAurieLog(fixture,text,reason)&&reason.find("older log")!=std::string::npos,"actual stale log timestamp prevents current native status confirmation");
            const auto writer=CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(writer==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open held-writer fixture");
            struct CloseWriter {HANDLE file;~CloseWriter(){CloseHandle(file);}} closeWriter{writer};DWORD written=0;if(!WriteFile(writer,complete.data(),static_cast<DWORD>(complete.size()),&written,nullptr)||written!=complete.size()||!FlushFileBuffers(writer))throw std::runtime_error("Cannot write held-open Aurie fixture");
            stampLog(fresh);const bool heldRead=SlormReadCurrentAurieLog(fixture,text,reason);
            Check(heldRead&&AurieStartupFinished(text,true),("truncated current Aurie log is confirmed while its writer stays open"+(heldRead?std::string():": "+reason)).c_str());
        }
    auto report=ModStatusReport(Json::array({{{"status","unsupported"},{"reason","JSON skill patches removed"}}}),Json::array({NativeItem("mods/Aurie/Example.dll","loaded","","test")}),42,true,true);
    Check(report["active"]==1&&report["errors"]==1&&report["unsupported"]==1,"unsupported JSON is an error, never a loaded mod");
    Check(!report.contains("statApiVersion")&&report["gameplayPatching"]==false,"report exposes no skill/passive API");
    const auto scratch=root/"build"/("retired-json-"+std::to_string(GetTickCount64()));fs::create_directories(scratch/"mods/Slormancer/old-belt");
    std::ofstream(scratch/"mods/Slormancer/old-belt/mod.json")<<"{invalid,never parsed or applied}";
    fs::create_directories(scratch/"mods/Slormancer/empty");fs::create_directories(scratch/"mods/Slormancer/marker");std::ofstream(scratch/"mods/Slormancer/marker/__folder_managed_by_vortex")<<"marker";
    std::string logged;LoaderStatus status(scratch,[&](const auto& m){logged+=m;});status.Discover();
    Check(status.ErrorCount()==1&&status.TotalActiveCount()==0&&status.PendingCount()==0,"old JSON mods rejected without parsing patches; empty and marker directories ignored");
    Check(logged.find("JSON skill/passive")!=std::string::npos,"retired JSON error explains removal");
    NativeSnapshot snapshot;snapshot.items=Json::array({NativeItem("mods/Aurie/Example.dll","loaded","","test")});status.UpdateNativeStatus(snapshot,true);
    Check(status.TotalActiveCount()==1&&status.ErrorCount()==1&&status.NativeScanComplete(),"retired JSON does not stop native mod status");
    Json saved;std::ifstream(scratch/".slorm-loader/mod-status.json")>>saved;Check(saved["active"]==1&&saved["errors"]==1,"status committed atomically");
    std::cout<<"RESULT: "<<checks<<" loader status checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
