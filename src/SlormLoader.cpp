#include <YYToolkit/YYTK_Shared.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <mutex>
#include <set>
#include <sstream>
#include <share.h>
#include "SlormRuntime.hpp"
#include "SlormStatRuntime.hpp"
#include "SlormYYTKEnumeration.hpp"
#include <memory>

using namespace Aurie;
using namespace YYTK;
static_assert(YYTK_MAJOR == 5, "SlormLoader requires the pinned YYToolkit 5 SDK");
static_assert(sizeof(RValue) == 16, "Unsupported GameMaker value layout");
// The v5 SDK's FWCodeEvent still reverses the final two parameters. Match Hooks.cpp.
using ActualCodeEvent = FunctionWrapper<bool(CInstance*, CInstance*, CCode*, RValue*, int)>;
static YYTKInterface* api = nullptr;
static AurieModule* owner = nullptr;
static FILE* logfile = nullptr;
static std::mutex logmutex;
static int dataCount = 0;
static std::vector<std::string> dllNames;
static bool drew = false, inspected = false, gateMissingLogged = false;
static PFUNC_YYGMLScript originalUpgrade = nullptr;
static unsigned upgradeLogs = 0;
static bool upgradeHooked = false;
static fs::path gameDirectory;
static bool nativeScanFinalized = false;
static uint64_t lastNativePoll = 0;
static int firstLoadObject = -2;
static bool verifyPassive = false, verificationDone = false;
static std::unique_ptr<Slorm::StatRuntime> statRuntime;
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
static std::string Describe(const RValue& value) {
    switch (value.m_Kind) {
    case VALUE_STRING: return value.ToString().substr(0, 160);
    case VALUE_REAL: case VALUE_INT32: case VALUE_INT64: case VALUE_BOOL: {
        char text[64]; snprintf(text, sizeof(text), "%.12g", value.ToDouble()); return text;
    }
    case VALUE_ARRAY: return "array";
    case VALUE_OBJECT: return "object";
    case VALUE_PTR: return "pointer";
    case VALUE_NULL: return "null";
    case VALUE_UNDEFINED: case VALUE_UNSET: return "undefined";
    default: return "type=" + std::to_string(static_cast<unsigned>(value.m_Kind));
    }
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
static bool ArrayCell(RValue& array, size_t index, RValue*& result) {
    result = nullptr;
    return array.m_Kind == VALUE_ARRAY && AurieSuccess(api->GetArrayEntry(array, index, result)) && result &&
        result->m_Kind != VALUE_UNDEFINED && result->m_Kind != VALUE_UNSET;
}
static bool Number(const RValue* value, double& number) {
    if (Numeric(value)) { number = value->ToDouble(); return true; }
    if (value && value->m_Kind == VALUE_STRING) {
        auto text = value->ToString(); char* end = nullptr;
        number = strtod(text.c_str(), &end);
        return end != text.c_str() && *end == '\0';
    }
    return false;
}
struct CacheCell { RValue* address; RValue original; };
static bool SnapshotCache(RValue& value, std::vector<CacheCell>& cells, unsigned depth, unsigned& budget) {
    if (!budget || depth > 4) return false;
    --budget;
    if (value.m_Kind != VALUE_ARRAY) {
        if (!Numeric(&value) && value.m_Kind != VALUE_UNDEFINED && value.m_Kind != VALUE_UNSET &&
            value.m_Kind != VALUE_NULL && value.m_Kind != VALUE_STRING) return false;
        cells.push_back({&value, value}); return true;
    }
    size_t length = 0;
    if (!AurieSuccess(api->GetArraySize(value, length))) return false;
    for (size_t i = 0; i < length; ++i) {
        RValue* cell = nullptr;
        if (!AurieSuccess(api->GetArrayEntry(value, i, cell)) || !cell || !SnapshotCache(*cell, cells, depth + 1, budget)) return false;
    }
    return true;
}
static unsigned RestoreCaches(std::vector<CacheCell>& cells) {
    unsigned changed = 0;
    for (auto& cell : cells) {
        if (cell.address->m_Kind != cell.original.m_Kind || Describe(*cell.address) != Describe(cell.original)) ++changed;
        *cell.address = cell.original;
    }
    return changed;
}
static void VerifyPassiveCalculation(CInstance* self) {
    if (!verifyPassive || verificationDone) return;
    verificationDone = true;
    CInstance* global = nullptr;
    RValue *hero = nullptr, *heroSkills = nullptr, *row = nullptr, *stat = nullptr, *base = nullptr, *per = nullptr;
    double heroNumber = -1, baseNumber = 0, perNumber = 0;
    if (!AurieSuccess(api->GetGlobalInstance(&global)) || !global ||
        !Member(RValue(global), "current_hero", hero) || !Number(hero, heroNumber) || heroNumber != 0 ||
        !Member(RValue(global), "hero_skills", heroSkills) || !ArrayCell(*heroSkills, 112, row) ||
        !ArrayCell(*row, 3, stat) || stat->m_Kind != VALUE_STRING || stat->ToString() != "tenacity" ||
        !ArrayCell(*row, 4, base) || !Number(base, baseNumber) || baseNumber != 20 ||
        !ArrayCell(*row, 5, per) || !Number(per, perNumber) || (perNumber != 2 && perNumber != 8)) {
        Log("PASSIVE_VERIFY_SKIPPED Knight hero_skills[112] schema is not initialized or differs"); return;
    }
    RValue *cache = nullptr, *cacheRow = nullptr, *target = nullptr;
    if (!Member(RValue(global), "fast_check_skills_value", cache) || !ArrayCell(*cache, 112, cacheRow) ||
        !ArrayCell(*cacheRow, 0, target) || !Numeric(target)) {
        Log("PASSIVE_VERIFY_SKIPPED expected cache[112][0] is unavailable"); return;
    }
    unsigned budget = 40000;
    std::vector<CacheCell> cells;
    unsigned cacheArrays = 0;
    const auto enumeration = Slorm::EnumerateMembersCompletely(api, RValue(global), [&](const char* name, RValue* value) {
        if (std::string(name).rfind("fast_check_", 0) != 0) return true;
        if (value->m_Kind == VALUE_ARRAY) {
            ++cacheArrays;
            if (!SnapshotCache(*value, cells, 0, budget)) return false;
        }
        return true;
    });
    if (!enumeration.complete || cells.empty()) {
        Log("PASSIVE_VERIFY_SKIPPED cache snapshot unavailable reason=%s enumStatus=%s expectedMembers=%d visitedMembers=%zu",
            enumeration.reason.c_str(), AurieStatusToString(enumeration.status), enumeration.expected, enumeration.visited);
        return;
    }
    Log("PASSIVE_VERIFY_BEGIN current_hero=0 skill=112 result_index=0 base=%.0f per_rank=%.0f cache_arrays=%u cache_cells=%zu",
        baseNumber, perNumber, cacheArrays, cells.size());
    for (int rank : {1, 10}) {
        if (!verifyPassive) break;
        struct CacheRollback {
            std::vector<CacheCell>& cells;
            ~CacheRollback() noexcept {
                // Restore every leaf even if a conversion or a preceding assignment failed.
                for (auto& cell : cells) try {*cell.address = cell.original;}
                catch (...) {verifyPassive = false; Log("ERROR passive probe cache restoration failed; probe disabled");}
            }
        } rollback{cells};
        RValue result;
        const auto status = api->CallGameScriptEx(result, "gml_Script_scr_skill_get_result_value", self, self, {112, 0, rank, false});
        double actual = 0;
        const bool numeric = Number(&result, actual);
        const unsigned changed = RestoreCaches(cells);
        Log("PASSIVE_GAME_RESULT rank=%d result=%s expected=%.12g matches=%d status=%s cache_cells_restored=%u",
            rank, Describe(result).c_str(), baseNumber + perNumber * rank,
            AurieSuccess(status) && numeric && actual == baseNumber + perNumber * rank ? 1 : 0,
            AurieStatusToString(status), changed);
    }
    Log("PASSIVE_VERIFY_END no skill-rank or save writes; transient calculation caches restored");
}
#include "SlormLiveDiagnostics.hpp"
static void InspectGlobals(CInstance*) {
    CInstance* global = nullptr;
    if (!AurieSuccess(api->GetGlobalInstance(&global)) || !global) { Log("global namespace unavailable"); return; }
    RValue *skills = nullptr, *class0 = nullptr, *row = nullptr;
    if (!Member(RValue(global), "skills", skills) || !ArrayCell(*skills, 0, class0) || !ArrayCell(*class0, 112, row)) {
        Log("IRREMOVABLE_DATA_UNAVAILABLE global.skills[0][112]"); return;
    }
    size_t columns = 0;
    if (row->m_Kind != VALUE_ARRAY || !AurieSuccess(api->GetArraySize(*row, columns))) {
        Log("IRREMOVABLE_DATA_UNAVAILABLE row is not array"); return;
    }
    Log("IRREMOVABLE_RUNTIME_ROW path=global.skills[0][112] columns=%zu", columns);
    for (size_t i = 0; i < columns && i < 20; ++i) {
        RValue* cell = nullptr;
        if (ArrayCell(*row, i, cell)) Log("IRREMOVABLE_CELL [%zu]=%s", i, Describe(*cell).c_str());
    }
    RValue *ref = nullptr, *base = nullptr, *rank = nullptr;
    double refNumber = 0, baseNumber = 0, rankNumber = 0;
    if (ArrayCell(*row, 0, ref) && ArrayCell(*row, 4, base) && ArrayCell(*row, 5, rank) &&
        Number(ref, refNumber) && Number(base, baseNumber) && Number(rank, rankNumber) && refNumber == 112) {
        Log("IRREMOVABLE_DATA_VERIFIED ref=112 base=%.12g per_rank=%.12g source=runtime_table", baseNumber, rankNumber);
        Log("ARITHMETIC_EXPECTATION formula=base+per_rank*rank rank1=%.12g rank10=%.12g gameplay_formula_requires_script_verification",
            baseNumber + rankNumber, baseNumber + rankNumber * 10.0);
    }
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
    if(nativeScanFinalized||!statRuntime)return;
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
        statRuntime->UpdateNativeStatus(snapshot,finished);
        if(finished){nativeScanFinalized=true;Log("NATIVE_SCAN_FINISHED complete=%d native_mods=%d errors=%d",snapshot.complete,statRuntime->NativeActiveCount(),statRuntime->ErrorCount());}
    }catch(const std::exception& e){
        Slorm::NativeSnapshot failed;Slorm::NativeScanIssue(failed,std::string("Native status collection failed: ")+e.what());
        try{statRuntime->UpdateNativeStatus(failed,true);}catch(...){}nativeScanFinalized=true;
        Log("NATIVE_SCAN_FAILED reason=%s",e.what());
    }catch(...){
        try{Slorm::NativeSnapshot failed;Slorm::NativeScanIssue(failed,"Native status collection failed with an unknown exception.");statRuntime->UpdateNativeStatus(failed,true);}catch(...){}
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
    if (!inspected && verifyPassive) { inspected = true; InspectGlobals(self); }
    try {VerifyPassiveCalculation(self);}
    catch (const std::exception& e) {verifyPassive = false; Log("PASSIVE_VERIFY_FAILED reason=%s", e.what());}
    const int count = statRuntime ? statRuntime->TotalActiveCount() : 0;
    auto height = Builtin("display_get_gui_height").ToDouble();
    auto color = Builtin("draw_get_color"), alpha = Builtin("draw_get_alpha");
    auto halign = Builtin("draw_get_halign"), valign = Builtin("draw_get_valign");
    Builtin("draw_set_halign", {0}); Builtin("draw_set_valign", {0}); Builtin("draw_set_alpha", {1.0});
    const int errors = statRuntime ? statRuntime->ErrorCount() : 0;
    const int pending = statRuntime ? statRuntime->PendingCount() : 0;
    const bool nativeComplete = statRuntime && statRuntime->NativeScanComplete();
    const bool nativeStarted = statRuntime && statRuntime->NativeStartupFinished();
    const std::string caption = std::to_string(count) + " mods loaded" + (errors ? " (" + std::to_string(errors) + " errors)" : "") + (pending ? " ("+std::to_string(pending)+" pending)" : "") + (!nativeComplete ? nativeStarted?" (native scan incomplete)":pending?"":" (native scan pending)" : "");
    Builtin("draw_set_color", {0}); Builtin("draw_text", {13, height - 23, caption.c_str()});
    Builtin("draw_set_color", {16777215}); Builtin("draw_text", {12, height - 24, caption.c_str()});
    Builtin("draw_set_color", {color}); Builtin("draw_set_alpha", {alpha});
    Builtin("draw_set_halign", {halign}); Builtin("draw_set_valign", {valign});
    if (count != lastDrawCount || errors != lastErrors || pending != lastPending || nativeComplete != lastNativeComplete) { Log("DRAW_CONFIRMED main-menu count=%d stat_mods=%d native_mods=%d errors=%d pending=%d native_scan=%s event=obj_controller_Draw_64 gui_height=%.0f", count, statRuntime ? statRuntime->ActiveCount() : 0, statRuntime ? statRuntime->NativeActiveCount() : 0, errors, pending, nativeComplete?"complete":nativeStarted?"incomplete":"pending", height); drew = true; lastDrawCount = count; lastErrors = errors; lastPending = pending; lastNativeComplete = nativeComplete; }
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
    if (statRuntime) {
        if (firstLoadObject == -2) {auto object = Builtin("asset_get_index", {"obj_first_load"}); firstLoadObject = Numeric(&object) ? object.ToInt32() : -1;}
        bool loading = false;
        if (firstLoadObject >= 0) {auto value = Builtin("instance_exists", {firstLoadObject}); loading = Numeric(&value) && value.ToDouble() != 0;}
        if (!loading) {statRuntime->Tick();RefreshNativeStatus();TickDiagnostics(self);}
    }
    DrawCount(self);
}
static RValue& UpgradeDiagnostic(CInstance* self, CInstance* other, RValue& result, int argc, RValue* argv[]) {
    RValue& returned = originalUpgrade(self, other, result, argc, argv);
    if (upgradeLogs < 12) {
        std::string args;
        for (int i = 0; i < argc && i < 8; ++i) { if (i) args += ", "; args += argv[i] ? Describe(*argv[i]) : "null"; }
        Log("UPGRADE_RESULT natural-call args=[%s] result=%s", args.c_str(), Describe(returned).c_str()); ++upgradeLogs;
    }
    return returned;
}
EXPORTED AurieStatus ModuleInitialize(AurieModule* module, const fs::path&) {
    owner = module;
    auto path = SlormRuntimeLogPath();
    if (!path.empty()) logfile = _wfsopen(path.c_str(), L"ab", _SH_DENYNO);
    char env[4096];
    if (GetEnvironmentVariableA("SLORM_VERIFY_PASSIVE", env, sizeof(env))) verifyPassive = strcmp(env, "1") == 0;
    if (GetEnvironmentVariableA("SLORM_MOD_PROFILE", env, sizeof(env))) Log("INIT profile=%s data_mods=%d", env, dataCount);
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
        statRuntime = std::make_unique<Slorm::StatRuntime>(api,gameDirectory,[](const std::string& message){Log("%s",message.c_str());});
        statRuntime->Discover();
        Log("STAT_API_READY runtime=%s schemaVersion=1",Slorm::RuntimeVersion);
    } catch (const std::exception& e) {Log("ERROR stat-mod discovery failed: %s",e.what()); statRuntime.reset(); return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;}
    auto status = api->CreateCallback(module, EVENT_OBJECT_CALL, reinterpret_cast<PVOID>(CodeCallback), -100);
    if (!AurieSuccess(status)) { Log("ERROR callback registration %s", AurieStatusToString(status)); return status; }
    {wchar_t image[32768]{};GetModuleFileNameW(nullptr,image,32768);LoadDiagnostics(fs::path(image).parent_path());}
    if (verifyPassive) {
        PVOID script = nullptr;
        status = api->GetNamedRoutinePointer("gml_Script_scr_skill_get_upgrade_result", &script);
        if (AurieSuccess(status) && script && static_cast<CScript*>(script)->m_Functions) {
            auto fn = static_cast<CScript*>(script)->m_Functions->m_ScriptFunction;
            status = MmCreateHook(module, "SlormLoader.UpgradeDiagnostic", reinterpret_cast<PVOID>(fn), reinterpret_cast<PVOID>(UpgradeDiagnostic), reinterpret_cast<PVOID*>(&originalUpgrade));
            upgradeHooked = AurieSuccess(status);
            Log("read-only skill result hook=%s", AurieStatusToString(status));
        } else Log("skill result script unavailable=%s", AurieStatusToString(status));
    }
    Log("INIT_SUCCESS standalone loader; deployed DLL discovery uses Aurie initialization state");
    return AURIE_SUCCESS;
}
EXPORTED AurieStatus ModuleUnload(AurieModule* module, const fs::path&) {
    for(const auto& hook:diagnosticHooks)MmRemoveHook(module,hook.c_str());
    if (statRuntime) {statRuntime->Unload(); statRuntime.reset();}
    if (upgradeHooked) MmRemoveHook(module, "SlormLoader.UpgradeDiagnostic");
    if (api) api->RemoveCallback(module, reinterpret_cast<PVOID>(CodeCallback));
    Log("UNLOAD callbacks and diagnostic hook removed");
    if (logfile) { fclose(logfile); logfile = nullptr; }
    return AURIE_SUCCESS;
}
