#pragma once
#include "vendor/nlohmann/json.hpp"
#include "SlormFieldSpecs.hpp"
#include <algorithm>
#include <cmath>
#include <compare>
#include <fstream>
#include <map>
#include <set>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Slorm {
inline constexpr const char* RuntimeVersion = "0.3.10";
using Json = nlohmann::json;
struct Version {
    int major{}, minor{}, patch{};
    auto operator<=>(const Version&) const = default;
};
inline Version ParseVersion(const std::string& text) {
    static const std::regex pattern(R"((0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4}))");
    std::smatch m;
    if (!std::regex_match(text, m, pattern)) throw std::runtime_error("Version must be stable major.minor.patch: " + text);
    Version v{std::stoi(m[1]), std::stoi(m[2]), std::stoi(m[3])};
    if (v.major > 65535 || v.minor > 65535 || v.patch > 65535) throw std::runtime_error("Version component exceeds 65535");
    return v;
}
inline bool Satisfies(const std::string& version, const std::string& range) {
    auto actual = ParseVersion(version);
    std::istringstream input(range); std::string token; unsigned count = 0; bool matches = true;
    while (input >> token) {
        if (++count > 4) throw std::runtime_error("At most four version comparators are supported");
        size_t start = token.find_first_not_of("<>=");
        if (start == std::string::npos) throw std::runtime_error("Invalid version comparator");
        const auto op = token.substr(0, start); const auto expected = ParseVersion(token.substr(start));
        if (op.empty() || op == "=") matches &= actual == expected;
        else if (op == ">=") matches &= actual >= expected;
        else if (op == "<=") matches &= actual <= expected;
        else if (op == ">") matches &= actual > expected;
        else if (op == "<") matches &= actual < expected;
        else throw std::runtime_error("Unsupported version comparator: " + op);
    }
    if (!count) throw std::runtime_error("Version range is empty");
    return matches;
}
inline void Keys(const Json& object, std::initializer_list<const char*> allowed) {
    if (!object.is_object()) throw std::runtime_error("Expected an object");
    for (auto it = object.begin(); it != object.end(); ++it)
        if (std::find_if(allowed.begin(), allowed.end(), [&](const char* key) {return it.key() == key;}) == allowed.end())
            throw std::runtime_error("Unknown field: " + it.key());
}
inline std::string Text(const Json& object, const char* key, size_t limit) {
    const auto& value = object.at(key);
    if (!value.is_string()) throw std::runtime_error(std::string(key) + " must be text");
    auto result = value.get<std::string>();
    if (result.empty() || result.size() > limit || std::any_of(result.begin(), result.end(), [](unsigned char c) {return c < 32 || c == 127;}))
        throw std::runtime_error(std::string(key) + " is empty, too long, or contains control characters");
    return result;
}
inline void CheckId(const std::string& id) {
    static const std::regex pattern("[a-z][a-z0-9]*(-[a-z0-9]+)*");
    static const std::set<std::string> reserved{"con","prn","aux","nul","com1","com2","com3","com4","com5","com6","com7","com8","com9","lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9"};
    if (id.size() > 64 || !std::regex_match(id, pattern) || reserved.contains(id)) throw std::runtime_error("Invalid mod/patch ID: " + id);
}
inline double Number(const Json& object, const char* key) {
    const auto& value = object.at(key);
    if (!value.is_number()) throw std::runtime_error(std::string(key) + " must be a JSON number");
    double result = value.get<double>();
    if (!std::isfinite(result) || std::abs(result) > 1e9) throw std::runtime_error(std::string(key) + " exceeds supported numeric bounds");
    return result;
}
struct Target { int classId{}, skillRef{}; std::string stat; double base{}, perRank{}; int valueIndex=0; };
struct StatPatch { std::string id; Target target; Field field{}; double expected{}, value{}; };
struct Requirement { std::string id, version; };
struct Mod {
    std::string id, name, version, author, description, type;
    std::vector<Requirement> dependencies;
    std::vector<std::string> conflicts;
    std::vector<StatPatch> patches;
    std::string status = "pending", reason;
};
inline Mod ParseManifest(const Json& root, bool allowRuntime = false) {
    Keys(root, {"schemaVersion","id","name","version","author","description","type","requires","conflicts","patches"});
    if (!root.at("schemaVersion").is_number() || root.at("schemaVersion") != 1) throw std::runtime_error("Unsupported manifest schemaVersion; expected 1");
    Mod mod;
    mod.id = Text(root,"id",64); CheckId(mod.id);
    mod.name = Text(root,"name",100); mod.version = Text(root,"version",40); ParseVersion(mod.version);
    mod.author = Text(root,"author",100); mod.description = Text(root,"description",2000); mod.type = Text(root,"type",32);
    if (mod.type != "stat-patches" && !(allowRuntime && mod.type == "runtime" && mod.id == "slormancer-runtime")) throw std::runtime_error("Unsupported mod type: " + mod.type);
    if (mod.type != "runtime" && (mod.id == "slormancer-runtime" || mod.id == "aurie" || mod.id == "yytoolkit")) throw std::runtime_error("A gameplay mod cannot impersonate the shared runtime");
    const auto& requirements = root.at("requires");
    if (!requirements.is_array() || requirements.size() > 32) throw std::runtime_error("requires must be an array of at most 32 dependencies");
    std::set<std::string> ids; bool hasRuntime = false;
    for (const auto& r : requirements) {
        Keys(r,{"id","version"}); Requirement dep{Text(r,"id",64),Text(r,"version",120)}; CheckId(dep.id); Satisfies("0.0.0",dep.version);
        if (dep.id == mod.id || !ids.insert(dep.id).second) throw std::runtime_error("Duplicate or self dependency: " + dep.id);
        hasRuntime |= dep.id == "slormancer-runtime"; mod.dependencies.push_back(dep);
    }
    if (mod.type == "stat-patches" && !hasRuntime) throw std::runtime_error("A stat mod must declare its slormancer-runtime dependency");
    const auto& conflicts = root.at("conflicts"); ids.clear();
    if (!conflicts.is_array() || conflicts.size() > 32) throw std::runtime_error("conflicts must be an array of at most 32 IDs");
    for (const auto& item : conflicts) {
        if (!item.is_string()) throw std::runtime_error("Conflict IDs must be text");
        auto id = item.get<std::string>(); CheckId(id);
        if (id == mod.id || id == "slormancer-runtime" || id == "aurie" || id == "yytoolkit" || !ids.insert(id).second) throw std::runtime_error("Invalid, duplicate or self conflict: " + id);
        mod.conflicts.push_back(id);
    }
    const auto& patches = root.at("patches"); ids.clear(); std::set<std::string> cells;
    if (!patches.is_array() || patches.size() > 32 || (mod.type == "stat-patches" && patches.empty()) || (mod.type == "runtime" && !patches.empty())) throw std::runtime_error("Invalid patch list for mod type");
    for (const auto& p : patches) {
        Keys(p,{"id","target","field","expected","value"}); StatPatch patch;
        patch.id = Text(p,"id",64); CheckId(patch.id);
        if (!ids.insert(patch.id).second) throw std::runtime_error("Duplicate patch ID");
        const auto& t = p.at("target"); Keys(t,{"classId","skillRef","stat","expectedBase","expectedPerRank","valueIndex"});
        auto classId = Number(t,"classId"), ref = Number(t,"skillRef");
        if (classId < 0 || classId > 2 || ref < 0 || ref > 10000 || std::floor(classId) != classId || std::floor(ref) != ref) throw std::runtime_error("classId and skillRef must be bounded integers");
        patch.target = {static_cast<int>(classId),static_cast<int>(ref),Text(t,"stat",64),Number(t,"expectedBase"),Number(t,"expectedPerRank")};
        if(t.contains("valueIndex")) {double index=Number(t,"valueIndex");if(index<0||index>15||std::floor(index)!=index)throw std::runtime_error("valueIndex must be an integer from 0 to 15");patch.target.valueIndex=static_cast<int>(index);}
        if (!std::regex_match(patch.target.stat,std::regex("[a-z][a-z0-9_]*"))) throw std::runtime_error("Invalid expected stat name");
        auto field = Text(p,"field",20);
        patch.field = ParseField(field);
        patch.expected = Number(p,"expected"); patch.value = Number(p,"value");
        if(patch.value<GetFieldSpec(patch.field).minimum)throw std::runtime_error("Replacement is below the supported minimum for "+field);
        if(!GetFieldSpec(patch.field).resultSlots&&patch.target.valueIndex!=0)throw std::runtime_error("Direct skill fields require valueIndex 0");
        if ((patch.field == Field::Base && patch.expected != patch.target.base) || (patch.field == Field::PerRank && patch.expected != patch.target.perRank)) throw std::runtime_error("Patch expected value contradicts target guard");
        auto key = std::to_string(classId) + ":" + std::to_string(ref) + ":" + field + ":" + std::to_string(patch.target.valueIndex);
        if (!cells.insert(key).second) throw std::runtime_error("A mod cannot patch one field twice");
        for (const auto& prior : mod.patches) if (prior.target.classId == classId && prior.target.skillRef == ref && prior.target.valueIndex==patch.target.valueIndex && (prior.target.stat != patch.target.stat || prior.target.base != patch.target.base || prior.target.perRank != patch.target.perRank)) throw std::runtime_error("Inconsistent guards for one skill result slot");
        mod.patches.push_back(patch);
    }
    return mod;
}
inline Json ParseJson(const std::string& text) {
    if (text.size() > 65536) throw std::runtime_error("Manifest exceeds 64 KiB");
    std::vector<std::set<std::string>> keys;
    return Json::parse(text, [&](int depth, Json::parse_event_t event, Json& parsed) {
        if (depth > 32) throw std::runtime_error("Manifest nesting exceeds 32");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        if (event == Json::parse_event_t::key && !keys.back().insert(parsed.get<std::string>()).second) throw std::runtime_error("Duplicate JSON key: " + parsed.get<std::string>());
        if (event == Json::parse_event_t::object_end) keys.pop_back();
        return true;
    });
}
inline std::string RowKey(const StatPatch& p) { return std::to_string(p.target.classId) + ":" + std::to_string(p.target.skillRef); }
inline void Block(Mod& mod, const std::string& reason) { if (mod.status != "blocked") {mod.status = "blocked"; mod.reason = reason;} }
inline bool AppliedDependencies(const Mod& m,const std::vector<Mod>& mods,std::string& reason) {
    for (const auto& d : m.dependencies) {
        if (d.id == "slormancer-runtime" || d.id == "aurie" || d.id == "yytoolkit") continue;
        auto found = std::find_if(mods.begin(),mods.end(),[&](const Mod& other){return other.id == d.id;});
        if (found == mods.end() || found->status != "applied") {reason = "Dependency did not apply: " + d.id; return false;}
    }
    return true;
}
inline std::vector<size_t> Resolve(std::vector<Mod>& mods) {
    std::map<std::string,std::vector<size_t>> byId;
    const std::map<std::string,std::string> builtin{{"slormancer-runtime",RuntimeVersion},{"aurie","2.0.2"},{"yytoolkit","5.0.0"}};
    for (size_t i = 0; i < mods.size(); ++i) byId[mods[i].id].push_back(i);
    for (const auto& [id, indices] : byId) if (indices.size() > 1) for (auto i : indices) Block(mods[i],"Duplicate deployed mod ID: " + id);
    std::map<std::string,size_t> rows;
    for (size_t i = 0; i < mods.size(); ++i) for (const auto& patch : mods[i].patches) {
        auto [it, inserted] = rows.emplace(RowKey(patch),i);
        if (!inserted && it->second != i) {Block(mods[i],"Skill target also patched by " + mods[it->second].name); Block(mods[it->second],"Skill target also patched by " + mods[i].name);}
    }
    for (size_t i = 0; i < mods.size(); ++i) for (const auto& id : mods[i].conflicts) if (byId.contains(id)) {
        Block(mods[i],"Conflicting deployed mod: " + id); for (auto j : byId[id]) Block(mods[j],"Conflicting deployed mod: " + mods[i].id);
    }
    for (auto& mod : mods) for (const auto& dep : mod.dependencies) {
        std::string version;
        if (builtin.contains(dep.id)) version = builtin.at(dep.id);
        else if (byId.contains(dep.id) && byId.at(dep.id).size() == 1) version = mods[byId.at(dep.id)[0]].version;
        else {Block(mod,"Missing or ambiguous dependency: " + dep.id); continue;}
        if (!Satisfies(version,dep.version)) Block(mod,"Dependency " + dep.id + " requires " + dep.version + "; found " + version);
    }
    // Stable topological order. Unresolved cycles and their dependents never apply.
    std::vector<size_t> order; std::set<size_t> done;
    bool progress = true;
    while (progress) {
        progress = false;
        for (const auto& [id, indices] : byId) for (auto i : indices) {
            auto& mod = mods[i]; if (mod.status == "blocked" || done.contains(i)) continue;
            bool ready = true;
            for (const auto& dep : mod.dependencies) if (!builtin.contains(dep.id)) {
                auto j = byId.at(dep.id)[0]; if (mods[j].status == "blocked") {Block(mod,"Dependency is blocked: " + dep.id); progress = true; ready = false; break;}
                if (!done.contains(j)) ready = false;
            }
            if (ready) {order.push_back(i); done.insert(i); progress = true;}
        }
    }
    for (size_t i = 0; i < mods.size(); ++i) if (mods[i].status != "blocked" && !done.contains(i)) Block(mods[i],"Dependency cycle or dependency on a cycle");
    return order;
}
}
