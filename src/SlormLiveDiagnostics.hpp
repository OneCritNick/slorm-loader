#pragma once
// Included after the loader's logging, numeric and bounded-cache helpers.
#include "SlormDiagnostics.hpp"
static bool diagnosticsEnabled=false,diagnosticProbeRunning=false;
static Slorm::DiagnosticOptions diagnosticOptions;
static std::set<std::string> diagnosticReads,diagnosticProbes;
static int diagnosticHero=-2;
static PFUNC_YYGMLScript diagnosticOriginalResult=nullptr,diagnosticOriginalCost=nullptr,diagnosticOriginalCooldown=nullptr;
static std::vector<std::string> diagnosticHooks;
static unsigned diagnosticResultCalls=0,diagnosticCostCalls=0,diagnosticCooldownCalls=0;
static void DiagnosticObserved(const char* routine,CInstance* self,RValue& result,int argc,RValue* argv[],unsigned& count) noexcept {
    try {
        if(!diagnosticsEnabled||!statRuntime||statRuntime->ActiveCount()==0||count>=40||!argv)return;
        Slorm::YYTKBackend observedBackend(api);if(!observedBackend.Ready())return;int hero=observedBackend.HeroClass();
        double ref{},index=-1;bool relevant=false,isResult=strcmp(routine,"scr_skill_get_result_value")==0;
        if(isResult&&argc>1&&argv[1])Number(argv[1],index);
        if(argc>0&&argv[0]&&Number(argv[0],ref))relevant=Slorm::DiagnosticRelevant(statRuntime->Mods(),routine,hero,ref,index);
        if(!relevant)return;++count;Slorm::Json args=Slorm::Json::array();
        for(int i=0;i<argc&&i<8;++i){double n{};if(argv[i]&&Number(argv[i],n)&&std::isfinite(n))args.push_back(n);else args.push_back(argv[i]?Describe(*argv[i]):"null");}
        Log("DIAG_GAME_CALL routine=%s origin=%s currentHeroClass=%d self=%p args=%s returned=%s evidence=original_game_return",routine,diagnosticProbeRunning?"probe":"natural",hero,static_cast<void*>(self),args.dump().c_str(),Describe(result).c_str());
        if(count==40)Log("DIAG_GAME_CALL_LIMIT routine=%s limit=40",routine);
    }catch(...){Log("DIAG_OBSERVATION_ERROR routine=%s",routine);}
}
static RValue& DiagnosticResultHook(CInstance* self,CInstance* other,RValue& result,int argc,RValue* argv[]){auto& returned=diagnosticOriginalResult(self,other,result,argc,argv);DiagnosticObserved("scr_skill_get_result_value",self,returned,argc,argv,diagnosticResultCalls);return returned;}
static RValue& DiagnosticCostHook(CInstance* self,CInstance* other,RValue& result,int argc,RValue* argv[]){auto& returned=diagnosticOriginalCost(self,other,result,argc,argv);DiagnosticObserved("scr_modified_skill_cost",self,returned,argc,argv,diagnosticCostCalls);return returned;}
static RValue& DiagnosticCooldownHook(CInstance* self,CInstance* other,RValue& result,int argc,RValue* argv[]){auto& returned=diagnosticOriginalCooldown(self,other,result,argc,argv);DiagnosticObserved("scr_modified_skill_cooldown",self,returned,argc,argv,diagnosticCooldownCalls);return returned;}
static void LoadDiagnostics(const fs::path& game) noexcept {
    try {
        auto file=game/".slorm-loader"/"diagnostics.json";
        if(!fs::exists(file))return;
        for(const auto& p:{game/".slorm-loader",file}){auto attributes=GetFileAttributesW(p.c_str());if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Linked diagnostic paths unsupported");}
        if(fs::file_size(file)>65536)throw std::runtime_error("Diagnostic config exceeds 64 KiB");
        std::ifstream input(file,std::ios::binary);std::string text((std::istreambuf_iterator<char>(input)),{});
        diagnosticOptions=Slorm::ParseDiagnostics(Slorm::ParseJson(text));diagnosticsEnabled=true;
        Log("DIAG_ENABLED calculations=%d scope=one_check_per_mod_and_hero_class rank_argument=explicit_calculator_rank",diagnosticOptions.calculations);
        struct Hook {const char* routine;const char* name;PVOID replacement;PFUNC_YYGMLScript* original;};
        for(const auto& h:{Hook{"gml_Script_scr_skill_get_result_value","SlormLoader.Diagnostics.Result",reinterpret_cast<PVOID>(DiagnosticResultHook),&diagnosticOriginalResult},Hook{"gml_Script_scr_modified_skill_cost","SlormLoader.Diagnostics.Cost",reinterpret_cast<PVOID>(DiagnosticCostHook),&diagnosticOriginalCost},Hook{"gml_Script_scr_modified_skill_cooldown","SlormLoader.Diagnostics.Cooldown",reinterpret_cast<PVOID>(DiagnosticCooldownHook),&diagnosticOriginalCooldown}}){
            PVOID routine=nullptr;auto status=api->GetNamedRoutinePointer(h.routine,&routine);bool installed=false;
            if(AurieSuccess(status)&&routine&&static_cast<CScript*>(routine)->m_Functions){auto fn=static_cast<CScript*>(routine)->m_Functions->m_ScriptFunction;if(fn){status=MmCreateHook(owner,h.name,reinterpret_cast<PVOID>(fn),h.replacement,reinterpret_cast<PVOID*>(h.original));if(AurieSuccess(status)){diagnosticHooks.push_back(h.name);installed=true;}}}
            Log("DIAG_HOOK routine=%s installed=%d status=%s",h.routine,installed,AurieStatusToString(status));
        }
    }catch(const std::exception& e){diagnosticsEnabled=false;Log("DIAG_CONFIG_REJECTED reason=%s",e.what());}
}
static void ProbeDiagnosticFormula(CInstance* self,const Slorm::Mod& mod,const Slorm::StatPatch& patch) {
    CInstance* global=nullptr;if(!AurieSuccess(api->GetGlobalInstance(&global))||!global)return;
    RValue *cache=nullptr,*cacheRow=nullptr,*target=nullptr;
    if(!Member(RValue(global),"fast_check_skills_value",cache)||!ArrayCell(*cache,patch.target.skillRef,cacheRow)||!ArrayCell(*cacheRow,patch.target.valueIndex,target)||(target->m_Kind!=VALUE_REAL&&target->m_Kind!=VALUE_INT32&&target->m_Kind!=VALUE_INT64)){
        Log("DIAG_PROBE_SKIPPED mod=%s skill=%d reason=numeric_target_cache_not_ready",mod.id.c_str(),patch.target.skillRef);return;
    }
    Slorm::YYTKBackend readyBackend(api);if(!readyBackend.Ready()||readyBackend.HeroClass()!=patch.target.classId)return;
    Slorm::YYTKBackend::Snapshot live;readyBackend.Read(patch,true,live);const auto [guardBase,guardPer]=Slorm::PatchedFormula(mod,patch);
    if(live.ref!=patch.target.skillRef||live.stat!=patch.target.stat||live.base!=guardBase||live.perRank!=guardPer){Log("DIAG_PROBE_SKIPPED mod=%s reason=hero_schema_mismatch",mod.id.c_str());return;}
    std::vector<CacheCell> cells;unsigned budget=40000;
    const auto enumeration=Slorm::EnumerateMembersCompletely(api,RValue(global),[&](const char* name,RValue* value){
        if(std::string(name).rfind("fast_check_",0)==0&&value->m_Kind==VALUE_ARRAY)return SnapshotCache(*value,cells,0,budget);
        return true;
    });
    if(!enumeration.complete||cells.empty()){Log("DIAG_PROBE_SKIPPED mod=%s skill=%d valueIndex=%d reason=bounded_cache_snapshot_unavailable detail=%s enumStatus=%s expectedMembers=%d visitedMembers=%zu",mod.id.c_str(),patch.target.skillRef,patch.target.valueIndex,enumeration.reason.empty()?"no_cache_leaves":enumeration.reason.c_str(),AurieStatusToString(enumeration.status),enumeration.expected,enumeration.visited);return;}
    Log("DIAG_PROBE_SNAPSHOT mod=%s members=%zu cacheCells=%zu enumStatus=%s",mod.id.c_str(),enumeration.visited,cells.size(),AurieStatusToString(enumeration.status));
    const auto [base,per]=Slorm::PatchedFormula(mod,patch);
    for(int rank:diagnosticOptions.ranks){
        if(!diagnosticsEnabled)break;
        if(rank!=1&&rank!=10){Log("DIAG_PROBE_SKIPPED mod=%s rankArgument=%d reason=only_ranks_1_and_10_are_verified",mod.id.c_str(),rank);continue;}
        // The original calculator can update transient caches. Restore every captured leaf even on exceptions.
        struct Rollback {std::vector<CacheCell>& cells;~Rollback(){diagnosticProbeRunning=false;bool complete=true;for(auto& c:cells)try{*c.address=c.original;}catch(...){complete=false;Log("DIAG_CACHE_RESTORE_FAILED");}if(!complete){diagnosticsEnabled=false;Log("DIAG_STOPPED reason=cache_restore_failed");}}} rollback{cells};
        diagnosticProbeRunning=true;
        try {RValue result;auto status=api->CallGameScriptEx(result,"gml_Script_scr_skill_get_result_value",self,self,{patch.target.skillRef,patch.target.valueIndex,rank,false});double actual{};bool numeric=Number(&result,actual)&&std::isfinite(actual);unsigned restored=RestoreCaches(cells);double prediction=base+per*rank;
            Log("DIAG_CALCULATED mod=%s skill=%d valueIndex=%d rankArgument=%d actual=%s linearPrediction=%.12g match=%d status=%s cacheCellsRestored=%u evidence=explicit_game_calculator linear_model_is_not_combat_damage",mod.id.c_str(),patch.target.skillRef,patch.target.valueIndex,rank,Describe(result).c_str(),prediction,AurieSuccess(status)&&numeric&&Slorm::DiagnosticMatches(actual,prediction),AurieStatusToString(status),restored);
        }catch(const std::exception& e){Log("DIAG_PROBE_ERROR mod=%s reason=%s",mod.id.c_str(),e.what());}
    }
}
static void TickDiagnostics(CInstance* self) noexcept {
    if(!diagnosticsEnabled||!statRuntime)return;
    try {
        Slorm::YYTKBackend backend(api);if(!backend.Ready())return;int hero=backend.HeroClass();
        if(hero!=diagnosticHero){diagnosticHero=hero;diagnosticProbes.clear();for(auto it=diagnosticReads.begin();it!=diagnosticReads.end();)if(it->rfind("hero:",0)==0)it=diagnosticReads.erase(it);else ++it;Log("DIAG_HERO_CLASS class=%d",hero);}
        for(const auto& mod:statRuntime->Mods())if(mod.status=="applied")for(const auto& patch:mod.patches){
            for(bool selectedHero:{false,true}){
                if(selectedHero&&(patch.target.classId!=hero||!backend.HeroReady()||!mod.reason.empty()||!backend.HasHeroRows({patch})))continue;
                auto key=std::string(selectedHero?"hero:":"class:")+mod.id+":"+patch.id;if(diagnosticReads.contains(key))continue;
                try{Slorm::YYTKBackend::Snapshot snapshot;backend.Read(patch,selectedHero,snapshot);
                    Log("DIAG_FIELD_CHECK mod=%s patch=%s scope=%s class=%d skill=%d valueIndex=%d field=%s original=%.12g expected=%.12g actual=%.12g match=%d evidence=live_table_readback actualRef=%.12g actualStat=%s actualBase=%.12g actualPerRank=%.12g",mod.id.c_str(),patch.id.c_str(),selectedHero?"hero":"class",patch.target.classId,patch.target.skillRef,patch.target.valueIndex,Slorm::FieldName(patch.field),patch.expected,patch.value,snapshot.value,Slorm::DiagnosticFieldMatches(mod,patch,snapshot),snapshot.ref,snapshot.stat.c_str(),snapshot.base,snapshot.perRank);diagnosticReads.insert(key);
                }catch(const std::exception& e){if(!selectedHero){Log("DIAG_FIELD_ERROR mod=%s reason=%s",mod.id.c_str(),e.what());diagnosticReads.insert(key);}}
            }
            if(diagnosticOptions.calculations&&patch.target.classId==hero&&mod.reason.empty()&&backend.HeroReady()&&backend.HasHeroRows({patch})&&(patch.field==Slorm::Field::Base||patch.field==Slorm::Field::PerRank)){
                auto key=mod.id+":"+std::to_string(patch.target.skillRef)+":"+std::to_string(patch.target.valueIndex);
                if(diagnosticProbes.insert(key).second){
                    if(Slorm::DiagnosticProbeEligible(mod,patch))ProbeDiagnosticFormula(self,mod,patch);
                    else Log("DIAG_PROBE_SKIPPED mod=%s skill=%d valueIndex=%d reason=explicit_probe_not_yet_verified_for_this_skill use=natural_game_call_observation",mod.id.c_str(),patch.target.skillRef,patch.target.valueIndex);
                }
            }
        }
    }catch(const std::exception& e){diagnosticsEnabled=false;Log("DIAG_STOPPED reason=%s",e.what());}
}
