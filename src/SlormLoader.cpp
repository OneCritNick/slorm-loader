#include <YYToolkit/YYTK_Shared.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <mutex>
#include <set>
#include <sstream>
#include <share.h>
#include "SlormRuntime.hpp"
#include "SlormLoaderStatus.hpp"
#include <memory>

using namespace Aurie;
using namespace YYTK;
namespace fs=std::filesystem;
static_assert(YYTK_MAJOR == 5, "SlormLoader requires the pinned YYToolkit 5 SDK");
static_assert(sizeof(RValue) == 16, "Unsupported GameMaker value layout");
// The v5 SDK's FWCodeEvent still reverses the final two parameters. Match Hooks.cpp.
using ActualCodeEvent = FunctionWrapper<bool(CInstance*, CInstance*, CCode*, RValue*, int)>;
static YYTKInterface* api = nullptr;
static AurieModule* owner = nullptr;
static FILE* logfile = nullptr;
static std::mutex logmutex;
static bool drew = false, gateMissingLogged = false;
static fs::path gameDirectory;
static bool nativeScanFinalized = false;
static uint64_t lastNativePoll = 0;
static int firstLoadObject = -2;
static std::unique_ptr<Slorm::LoaderStatus> loaderStatus;
static int lastDrawCount = -1, lastErrors = -1, lastPending = -1;
static bool lastNativeComplete = false;

static void Log(const char* format, ...) {
    std::lock_guard<std::mutex> lock(logmutex);
    if (!logfile) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(logfile, "%02u:%02u:%02u ", t.wHour, t.wMinute, t.wSecond);
    va_list args; va_start(args, format); vfprintf(logfile, format, args); va_end(args);
    fputc('\n', logfile); fflush(logfile);
}
static RValue Builtin(const char* name, std::vector<RValue> args = {}) {
    RValue result;
    auto status = api->CallBuiltinEx(result, name, nullptr, nullptr, std::move(args));
    if (!AurieSuccess(status)) Log("builtin %s failed: %s", name, AurieStatusToString(status));
    return result;
}
static bool Member(RValue object, const char* name, RValue*& value) {
    value = nullptr;
    return AurieSuccess(api->GetInstanceMember(object, name, value)) && value &&
        value->m_Kind != VALUE_UNDEFINED && value->m_Kind != VALUE_UNSET && value->m_Kind != VALUE_NULL;
}
static bool Numeric(const RValue* value) {
    return value && (value->m_Kind == VALUE_REAL || value->m_Kind == VALUE_INT32 ||
        value->m_Kind == VALUE_INT64 || value->m_Kind == VALUE_BOOL);
}
static bool IsMainMenu(CInstance* self) {
    RValue* gate = nullptr;
    CInstance* global = nullptr;
    if (AurieSuccess(api->GetGlobalInstance(&global)) && global && Member(RValue(global), "show_main_menu", gate) && Numeric(gate))
        return gate->ToDouble() != 0.0;
    if (self && Member(RValue(self), "show_main_menu", gate) && Numeric(gate)) return gate->ToDouble() != 0.0;
    if (!gateMissingLogged) { Log("main menu gate show_main_menu missing; watermark suppressed"); gateMissingLogged = true; }
    return false;
}
static void RefreshNativeStatus() noexcept {
    if(nativeScanFinalized||!loaderStatus)return;
    const auto now=GetTickCount64();if(lastNativePoll&&now-lastNativePoll<1000)return;lastNativePoll=now;
    try {
        std::string text,reason;
        const bool current=SlormReadCurrentAurieLog(gameDirectory,text,reason);
        const bool finished=Slorm::AurieStartupFinished(text,current);
        if(reason.empty())reason="Waiting for Aurie startup confirmation in aurie.log.";
        const auto snapshot=Slorm::ScanNativeMods(gameDirectory,finished,[&](const fs::path& path,bool uniqueName){
            Slorm::NativeEvidence evidence;AurieModule* module=nullptr;
            const auto lookup=Internal::MdpLookupModuleByPath(path,module);
            evidence.lookupStatus=AurieStatusToString(lookup);evidence.registered=AurieSuccess(lookup)&&module;
            // Never retain AurieModule pointers after this observation.
            if(evidence.registered){
                evidence.initialized=MdIsImageInitialized(module);
                evidence.moduleIdentity=reinterpret_cast<uintptr_t>(Internal::MdpGetModuleBaseAddress(module));
                std::wstring filename;if(AurieSuccess(MdGetImageFilename(module,filename)))evidence.registeredFilename=fs::path(filename);
                return evidence;
            }
            if(path.extension()!=fs::path(L".dll"))return evidence;
            auto status=PpQueryImageArchitecture(path,evidence.architecture);
            evidence.architectureRead=AurieSuccess(status);evidence.inspectionStatus=AurieStatusToString(status);
            if(evidence.architectureRead&&evidence.architecture==0x8664){
                evidence.frameworkInit=PpFindFileExportByName(path,"__AurieFrameworkInit")!=0;
                evidence.moduleInit=PpFindFileExportByName(path,"ModuleInitialize")!=0;
                evidence.modulePreinit=PpFindFileExportByName(path,"ModulePreinitialize")!=0;
            }
            evidence.originalFailure=Slorm::AurieFailureStatus(text,Slorm::PathText(path.filename()),uniqueName);
            return evidence;
        },reason);
        loaderStatus->UpdateNativeStatus(snapshot,finished);
        if(finished){nativeScanFinalized=true;Log("NATIVE_SCAN_FINISHED complete=%d native_mods=%d errors=%d",snapshot.complete,loaderStatus->NativeActiveCount(),loaderStatus->ErrorCount());}
    }catch(const std::exception& e){
        Slorm::NativeSnapshot failed;Slorm::NativeScanIssue(failed,std::string("Native status collection failed: ")+e.what());
        try{loaderStatus->UpdateNativeStatus(failed,true);}catch(...){}nativeScanFinalized=true;
        Log("NATIVE_SCAN_FAILED reason=%s",e.what());
    }catch(...){
        try{Slorm::NativeSnapshot failed;Slorm::NativeScanIssue(failed,"Native status collection failed with an unknown exception.");loaderStatus->UpdateNativeStatus(failed,true);}catch(...){}
        nativeScanFinalized=true;Log("NATIVE_SCAN_FAILED unknown exception; native status unavailable");
    }
}
static void DrawCount(CInstance* self) {
    if (!IsMainMenu(self)) return;
    if (firstLoadObject == -2) {
        auto object = Builtin("asset_get_index", {"obj_first_load"});
        firstLoadObject = Numeric(&object) ? object.ToInt32() : -1;
    }
    if (firstLoadObject >= 0) {
        auto loading = Builtin("instance_exists", {firstLoadObject});
        if (Numeric(&loading) && loading.ToDouble() != 0.0) return;
    }
    const int count = loaderStatus ? loaderStatus->TotalActiveCount() : 0;
    auto height = Builtin("display_get_gui_height").ToDouble();
    auto color = Builtin("draw_get_color"), alpha = Builtin("draw_get_alpha");
    auto halign = Builtin("draw_get_halign"), valign = Builtin("draw_get_valign");
    Builtin("draw_set_halign", {0}); Builtin("draw_set_valign", {0}); Builtin("draw_set_alpha", {1.0});
    const int errors = loaderStatus ? loaderStatus->ErrorCount() : 0;
    const int pending = loaderStatus ? loaderStatus->PendingCount() : 0;
    const bool nativeComplete = loaderStatus && loaderStatus->NativeScanComplete();
    const bool nativeStarted = loaderStatus && loaderStatus->NativeStartupFinished();
    const std::string caption = std::to_string(count) + " mods loaded" + (errors ? " (" + std::to_string(errors) + " errors)" : "") + (pending ? " ("+std::to_string(pending)+" pending)" : "") + (!nativeComplete ? nativeStarted?" (native scan incomplete)":pending?"":" (native scan pending)" : "");
    Builtin("draw_set_color", {0}); Builtin("draw_text", {13, height - 23, caption.c_str()});
    Builtin("draw_set_color", {16777215}); Builtin("draw_text", {12, height - 24, caption.c_str()});
    Builtin("draw_set_color", {color}); Builtin("draw_set_alpha", {alpha});
    Builtin("draw_set_halign", {halign}); Builtin("draw_set_valign", {valign});
    if (count != lastDrawCount || errors != lastErrors || pending != lastPending || nativeComplete != lastNativeComplete) { Log("DRAW_CONFIRMED main-menu count=%d native_mods=%d errors=%d pending=%d native_scan=%s event=obj_controller_Draw_64 gui_height=%.0f", count, loaderStatus ? loaderStatus->NativeActiveCount() : 0, errors, pending, nativeComplete?"complete":nativeStarted?"incomplete":"pending", height); drew = true; lastDrawCount = count; lastErrors = errors; lastPending = pending; lastNativeComplete = nativeComplete; }
}
static void CodeCallback(ActualCodeEvent& context) {
    auto* code = std::get<2>(context.Arguments());
    if (!code || !code->GetName()) return;
    const char* name = code->GetName();
    if (strcmp(name, "gml_Object_obj_controller_Draw_64") != 0) return;
    if (!context.CalledOriginal()) context.Call();
    auto* self = std::get<0>(context.Arguments());
    static bool callbackConfirmed = false;
    if (!callbackConfirmed) { callbackConfirmed = true; Log("CODE_CALLBACK_CONFIRMED %s", name); }
    if (loaderStatus) {
        if (firstLoadObject == -2) {auto object = Builtin("asset_get_index", {"obj_first_load"}); firstLoadObject = Numeric(&object) ? object.ToInt32() : -1;}
        bool loading = false;
        if (firstLoadObject >= 0) {auto value = Builtin("instance_exists", {firstLoadObject}); loading = Numeric(&value) && value.ToDouble() != 0;}
        if (!loading) {loaderStatus->Tick();RefreshNativeStatus();}
    }
    DrawCount(self);
}
EXPORTED AurieStatus ModuleInitialize(AurieModule* module, const fs::path&) {
    owner = module;
    auto path = SlormRuntimeLogPath();
    if (!path.empty()) logfile = _wfsopen(path.c_str(), L"ab", _SH_DENYNO);
    api = YYTK::GetInterface();
    if (!api) { Log("ERROR YYTK_ZeusMain interface missing; YYToolkit5 is required"); return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED; }
    short major = 0, minor = 0, patch = 0; api->QueryVersion(major, minor, patch);
    Log("YYTK_INTERFACE version=%d.%d.%d engine=GameMaker YYC x64", major, minor, patch);
    if (major != 5) { Log("ERROR unsupported YYTK major; expected5"); return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED; }
    // Every v5 RValue constructor, copy and conversion requires this interface.
    if (!YYTK::GetPrivateInterface()) {Log("ERROR YYTK_ZeusPrivate interface missing"); return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;}
    try {
        wchar_t image[32768]{}; GetModuleFileNameW(nullptr,image,32768);
        gameDirectory=fs::path(image).parent_path();
        loaderStatus = std::make_unique<Slorm::LoaderStatus>(gameDirectory,[](const std::string& message){Log("%s",message.c_str());});
        loaderStatus->Discover();
        Log("LOADER_READY runtime=%s gameplay_patching=disabled",Slorm::RuntimePackageVersion);
    } catch (const std::exception& e) {Log("ERROR loader status initialization failed: %s",e.what()); loaderStatus.reset(); return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;}
    auto status = api->CreateCallback(module, EVENT_OBJECT_CALL, reinterpret_cast<PVOID>(CodeCallback), -100);
    if (!AurieSuccess(status)) { Log("ERROR callback registration %s", AurieStatusToString(status)); return status; }
    Log("INIT_SUCCESS standalone loader; deployed DLL discovery uses Aurie initialization state");
    return AURIE_SUCCESS;
}
EXPORTED AurieStatus ModuleUnload(AurieModule* module, const fs::path&) {
    loaderStatus.reset();
    if (api) api->RemoveCallback(module, reinterpret_cast<PVOID>(CodeCallback));
    Log("UNLOAD loader status callback removed");
    if (logfile) { fclose(logfile); logfile = nullptr; }
    return AURIE_SUCCESS;
}
