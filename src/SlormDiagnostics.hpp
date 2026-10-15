#pragma once
#include "SlormModModel.hpp"
namespace Slorm {
struct DiagnosticOptions {bool calculations=false; std::vector<int> ranks{1,10};};
inline DiagnosticOptions ParseDiagnostics(const Json& j) {
    Keys(j,{"schemaVersion","enabled","calculations","ranks"});
    if(j.at("schemaVersion")!=1||!j.at("enabled").is_boolean()||!j.at("enabled").get<bool>()||!j.at("calculations").is_boolean())throw std::runtime_error("Unsupported diagnostic configuration");
    const auto& ranks=j.at("ranks");if(!ranks.is_array()||ranks.empty()||ranks.size()>4)throw std::runtime_error("Diagnostics requires 1 to 4 probe ranks");
    DiagnosticOptions result;result.calculations=j.at("calculations").get<bool>();result.ranks.clear();
    for(const auto& rank:ranks){if(!rank.is_number_integer()||rank.get<double>()<1||rank.get<double>()>15)throw std::runtime_error("Probe ranks must be integers from 1 to 15");int n=rank.get<int>();if(std::find(result.ranks.begin(),result.ranks.end(),n)!=result.ranks.end())throw std::runtime_error("Duplicate diagnostic rank");result.ranks.push_back(n);}
    return result;
}
inline bool DiagnosticMatches(double actual,double expected) {return std::isfinite(actual)&&std::isfinite(expected)&&std::abs(actual-expected)<=1e-9*(std::max)(1.0,std::abs(expected));}
inline std::pair<double,double> PatchedFormula(const Mod& mod,const StatPatch& patch) {
    double base=patch.target.base,per=patch.target.perRank;
    for(const auto& p:mod.patches)if(p.target.classId==patch.target.classId&&p.target.skillRef==patch.target.skillRef&&p.target.valueIndex==patch.target.valueIndex){if(p.field==Field::Base)base=p.value;if(p.field==Field::PerRank)per=p.value;}
    return {base,per};
}
inline bool DiagnosticRelevant(const std::vector<Mod>& mods,const std::string& routine,int hero,double ref,double index) {
    for(const auto& mod:mods)if(mod.status=="applied"&&mod.reason.empty())for(const auto& patch:mod.patches){
        if(patch.target.classId!=hero||patch.target.skillRef!=ref)continue;
        if(routine=="scr_skill_get_result_value"&&patch.target.valueIndex==index&&(patch.field==Field::Base||patch.field==Field::PerRank))return true;
        if(routine=="scr_modified_skill_cost"&&(patch.field==Field::ManaCost||patch.field==Field::ManaCostPerRank))return true;
        if(routine=="scr_modified_skill_cooldown"&&patch.field==Field::Cooldown)return true;
    }return false;
}
inline bool DiagnosticProbeEligible(const Mod& mod,const StatPatch& patch) {
    auto [base,per]=PatchedFormula(mod,patch);
    return patch.target.classId==0&&patch.target.skillRef==112&&patch.target.valueIndex==0&&patch.target.stat=="tenacity"&&base==20&&(per==2||per==8);
}
template<class Snapshot> inline bool DiagnosticFieldMatches(const Mod& mod,const StatPatch& patch,const Snapshot& s) {
    auto [base,per]=PatchedFormula(mod,patch);
    return s.ref==patch.target.skillRef&&s.stat==patch.target.stat&&DiagnosticMatches(s.base,base)&&DiagnosticMatches(s.perRank,per)&&DiagnosticMatches(s.value,patch.value);
}
}
