#pragma once
#include "SlormModModel.hpp"
#include <functional>

namespace Slorm {
template<class Fn> void InvalidateAffectedSkills(const std::vector<StatPatch>& patches,int hero,Fn invalidate) noexcept {
    for (const auto& p : patches) if (p.target.classId == hero) {try {invalidate(p.target.skillRef);} catch (...) {}}
}
template<class Fn> void InvalidateAffectedSlots(const std::vector<StatPatch>& patches,int hero,Fn invalidate) noexcept {
    for(const auto& p:patches)if(p.target.classId==hero){try{invalidate(p.target.skillRef,p.target.valueIndex);}catch (...) {}}
}
// Backend contract: Read(patch, hero, snapshot), Write(snapshot, value),
// Restore(snapshot, onlyIfValue), and Invalidate(patches). Snapshots retain rows.
// All patches in a mod are validated before any field is written.
template<class Backend> class StatPatchEngine {
    using Snapshot = typename Backend::Snapshot;
    struct Change { std::string modId; Snapshot snapshot; double value; };
    Backend& backend;
    std::vector<Change> changes;
public:
    explicit StatPatchEngine(Backend& backend) : backend(backend) {}
    void Apply(const Mod& mod, bool hero = false) {
        std::vector<Snapshot> prepared;
        for (const auto& patch : mod.patches) {
            Snapshot s{};
            backend.Read(patch,hero,s);
            double desiredBase = patch.target.base, desiredPer = patch.target.perRank;
            for (const auto& sibling : mod.patches) if (RowKey(sibling) == RowKey(patch) && sibling.target.valueIndex==patch.target.valueIndex) {
                if (sibling.field == Field::Base) desiredBase = sibling.value;
                if (sibling.field == Field::PerRank) desiredPer = sibling.value;
            }
            if (s.ref != patch.target.skillRef || s.stat != patch.target.stat ||
                !(s.base == patch.target.base || (hero && s.base == desiredBase)) ||
                !(s.perRank == patch.target.perRank || (hero && s.perRank == desiredPer)) ||
                !(s.value == patch.expected || (hero && s.value == patch.value)))
                throw std::runtime_error(patch.id + ": runtime skill data differs from expected values");
            prepared.push_back(std::move(s));
        }
        changes.reserve(changes.size() + prepared.size());
        size_t committed = 0;
        try {
            for (size_t i = 0; i < prepared.size(); ++i) {
                if (prepared[i].value == mod.patches[i].value) continue;
                // Journal before writing, so an allocation or write failure can restore it.
                changes.push_back({mod.id,prepared[i],mod.patches[i].value}); ++committed;
                backend.Write(prepared[i],mod.patches[i].value);
            }
        } catch (...) {
            while (committed > 0) {--committed; auto& c = changes.back(); backend.Restore(c.snapshot,c.value); changes.pop_back();}
            backend.Invalidate(mod.patches); throw;
        }
        if (committed) backend.Invalidate(mod.patches);
    }
    void RestoreMod(const Mod& mod) {
        bool restored = false;
        for (auto it = changes.rbegin(); it != changes.rend(); ++it) if (it->modId == mod.id) {backend.Restore(it->snapshot,it->value); restored = true;}
        std::erase_if(changes,[&](const Change& c){return c.modId == mod.id;});
        if (restored) backend.Invalidate(mod.patches);
    }
};

// Pure coordinator: the game adapter supplies readiness/hero rows and a clock.
// Keeping lifecycle decisions here lets tests exercise the actual runtime path.
template<class Backend> class StatModSession {
    Backend& backend;
    StatPatchEngine<Backend> engine;
    std::vector<Mod>& mods;
    const std::vector<size_t>& order;
    std::function<void(const std::string&)> log;
    std::map<std::string,uint64_t> heroWait;
    int previousHero = -2;
    bool started = false;
    void Fail(Mod& m,const std::string& reason) {
        engine.RestoreMod(m); m.status = "failed"; m.reason = reason;
        log("MOD_FAILED id=" + m.id + " name=" + m.name + " reason=" + m.reason);
    }
public:
    StatModSession(Backend& b,std::vector<Mod>& mods,const std::vector<size_t>& order,std::function<void(const std::string&)> sink) : backend(b),engine(b),mods(mods),order(order),log(std::move(sink)) {}
    bool Tick(uint64_t nowMs) {
        if (!backend.Ready()) return false;
        bool dirty = false;
        if (!started) {
            started = true;
            for (auto i : order) {
                auto& m = mods[i]; if (m.status == "blocked") continue;
                if (!AppliedDependencies(m,mods,m.reason)) {m.status = "blocked"; log("MOD_BLOCKED id=" + m.id + " reason=" + m.reason); dirty = true; continue;}
                try {
                    engine.Apply(m); m.status = "applied"; dirty = true;
                    for (const auto& p : m.patches) log("STAT_PATCH_APPLIED mod=" + m.id + " patch=" + p.id + " class=" + std::to_string(p.target.classId) + " skill=" + std::to_string(p.target.skillRef) + " valueIndex=" + std::to_string(p.target.valueIndex) + " stat=" + p.target.stat + " field=" + FieldName(p.field) + " expected=" + std::to_string(p.expected) + " value=" + std::to_string(p.value));
                    log("MOD_APPLIED id=" + m.id + " name=" + m.name + " version=" + m.version);
                } catch (const std::exception& e) {Fail(m,e.what()); dirty = true;}
            }
        }
        const int hero = backend.HeroClass();
        if (hero != previousHero) {
            previousHero = hero; heroWait.clear();
            for (auto& m : mods) if (m.status == "applied" && m.reason.rfind("Waiting for character",0) == 0) {m.reason.clear(); dirty = true;}
        }
        for (auto i : order) {
            auto& m = mods[i]; if (m.status != "applied") continue;
            if (!AppliedDependencies(m,mods,m.reason)) {engine.RestoreMod(m); m.status = "blocked"; dirty = true; log("MOD_BLOCKED id=" + m.id + " reason=" + m.reason); continue;}
            if (!backend.HeroReady()) continue;
            Mod selected = m; std::erase_if(selected.patches,[&](const StatPatch& p){return p.target.classId != hero;});
            if (selected.patches.empty() || !backend.HasHeroRows(selected.patches)) continue;
            try {
                engine.Apply(selected,true);
                if (heroWait.erase(m.id)) {m.reason.clear(); dirty = true; log("HERO_PATCH_READY id=" + m.id);}
            } catch (const std::exception& e) {
                auto [wait,inserted] = heroWait.emplace(m.id,nowMs);
                if (inserted) {m.reason = std::string("Waiting for character skill data: ") + e.what(); dirty = true; log("HERO_PATCH_WAITING id=" + m.id + " reason=" + m.reason);}
                if (nowMs - wait->second >= 2000) {Fail(m,e.what()); dirty = true;}
            }
        }
        return dirty;
    }
    void RestoreAll() {for (auto it = order.rbegin(); it != order.rend(); ++it) engine.RestoreMod(mods[*it]);}
};
}
