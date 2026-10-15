#pragma once
#include <YYToolkit/YYTK_Shared.hpp>
#include "SlormStatApi.hpp"
#include "SlormValueSlots.hpp"
#include <charconv>

namespace Slorm {
using namespace YYTK;
class YYTKBackend {
    YYTK::YYTKInterface* api;
    YYTK::CInstance* global = nullptr;
    static bool Numeric(const YYTK::RValue* v) { return v && (v->m_Kind == VALUE_REAL || v->m_Kind == VALUE_INT32 || v->m_Kind == VALUE_INT64); }
    static double ReadNumber(const YYTK::RValue* v) {
        if (Numeric(v)) { double n = v->ToDouble(); if (std::isfinite(n)) return n; }
        if (v && v->m_Kind == VALUE_STRING) {
            return NumericText(v->ToString());
        }
        throw std::runtime_error("Expected a finite numeric runtime field");
    }
    YYTK::RValue* Member(const char* name) {
        YYTK::RValue* result = nullptr;
        if (!global || !Aurie::AurieSuccess(api->GetInstanceMember(YYTK::RValue(global),name,result)) || !result) throw std::runtime_error(std::string("Runtime variable unavailable: ") + name);
        return result;
    }
    YYTK::RValue* Cell(YYTK::RValue& array, size_t index) {
        YYTK::RValue* result = nullptr; size_t length = 0;
        if (array.m_Kind != VALUE_ARRAY || !Aurie::AurieSuccess(api->GetArraySize(array,length)) || index >= length || !Aurie::AurieSuccess(api->GetArrayEntry(array,index,result)) || !result) throw std::runtime_error("Runtime skill array layout differs from expected");
        return result;
    }
    static YYTK::RValue ValueLike(const YYTK::RValue& previous, double value) {
        if (previous.m_Kind == VALUE_STRING) {
            char text[80]; auto result = std::to_chars(text,text+sizeof(text),value);
            if (result.ec != std::errc{}) throw std::runtime_error("Unable to format stat value");
            return YYTK::RValue(std::string(text,result.ptr));
        }
        if (previous.m_Kind == VALUE_INT32 && std::floor(value) == value && value >= INT32_MIN && value <= INT32_MAX) {YYTK::RValue result; result.m_i32 = static_cast<int32_t>(value); result.m_Kind = VALUE_INT32; return result;}
        if (previous.m_Kind == VALUE_INT64 && std::floor(value) == value) return YYTK::RValue(static_cast<int64_t>(value));
        if (previous.m_Kind == VALUE_INT32 || previous.m_Kind == VALUE_INT64) throw std::runtime_error("An integer runtime field requires an integer replacement");
        return YYTK::RValue(value);
    }
    static std::vector<std::string> Parts(const YYTK::RValue* value) {
        if(value&&value->m_Kind==VALUE_STRING)return SplitSlots(value->ToString());
        return {NumericString(ReadNumber(value))};
    }
    static double ComponentNumber(const YYTK::RValue* value,int index) {return NumericText(Slot(Parts(value),index));}
public:
    struct Snapshot {
        YYTK::RValue row, previous;
        size_t column{};
        int component=0;
        bool resultSlots=false;
        double ref{}, base{}, perRank{}, value{};
        std::string stat;
    };
    explicit YYTKBackend(YYTK::YYTKInterface* api) : api(api) {}
    bool Ready() {
        if (!Aurie::AurieSuccess(api->GetGlobalInstance(&global)) || !global) return false;
        try {return Member("skills")->m_Kind == VALUE_ARRAY;} catch (...) {return false;}
    }
    int HeroClass() {try {double n = ReadNumber(Member("current_hero")); if (n >= 0 && n <= 2 && std::floor(n) == n) return static_cast<int>(n);} catch (...) {} return -1;}
    bool HeroReady() { try {return Member("hero_skills")->m_Kind == VALUE_ARRAY;} catch (...) {return false;} }
    bool HasHeroRows(const std::vector<StatPatch>& patches) {
        try {for (const auto& p : patches) if (Cell(*Member("hero_skills"),p.target.skillRef)->m_Kind != VALUE_ARRAY) return false; return true;}
        catch (...) {return false;}
    }
    void Read(const StatPatch& p, bool hero, Snapshot& s) {
        YYTK::RValue* row;
        if (hero) row = Cell(*Member("hero_skills"),p.target.skillRef);
        else row = Cell(*Cell(*Member("skills"),p.target.classId),p.target.skillRef);
        s.row = *row;
        size_t length{};
        if(!Aurie::AurieSuccess(api->GetArraySize(s.row,length))||length!=54)throw std::runtime_error("Expected the verified 54-column skill layout");
        s.ref = ReadNumber(Cell(s.row,0));
        auto stat = Cell(s.row,3); if (stat->m_Kind != VALUE_STRING) throw std::runtime_error("Expected a text stat identifier");
        auto stats=SplitSlots(stat->ToString()),bases=Parts(Cell(s.row,4)),ranks=Parts(Cell(s.row,5));
        if(stats.size()!=bases.size()||stats.size()!=ranks.size())throw std::runtime_error("Skill result-slot lists have inconsistent lengths");
        s.stat=Slot(stats,p.target.valueIndex);s.base=NumericText(Slot(bases,p.target.valueIndex));s.perRank=NumericText(Slot(ranks,p.target.valueIndex));
        const auto& field=GetFieldSpec(p.field);s.column=field.column;s.resultSlots=field.resultSlots;s.component=field.resultSlots?p.target.valueIndex:0;
        s.previous = *Cell(s.row,s.column); s.value = field.resultSlots?ComponentNumber(&s.previous,s.component):ReadNumber(&s.previous);
    }
    void Write(Snapshot& s, double value) {
        auto cell=Cell(s.row,s.column);
        if(cell->m_Kind!=s.previous.m_Kind)throw std::runtime_error("Stat storage type changed after preflight");
        if(s.resultSlots&&s.previous.m_Kind==VALUE_STRING) {
            auto updated=ReplaceSlot(cell->ToString(),s.component,NumericString(value));*cell=YYTK::RValue(updated);
        }else {auto updated=ValueLike(s.previous,value);*cell=std::move(updated);}
    }
    void Restore(Snapshot& s, double onlyIfValue) noexcept {
        try {
            auto cell=Cell(s.row,s.column);
            if(cell->m_Kind!=s.previous.m_Kind)return;
            if(s.resultSlots&&s.previous.m_Kind==VALUE_STRING) {
                if(cell->m_Kind==VALUE_STRING&&ComponentNumber(cell,s.component)==onlyIfValue) {
                    auto original=Slot(SplitSlots(s.previous.ToString()),s.component);
                    *cell=YYTK::RValue(ReplaceSlot(cell->ToString(),s.component,original));
                }
            }else if(ReadNumber(cell)==onlyIfValue)*cell=s.previous;
        }catch (...) {}
    }
    void Invalidate(const std::vector<StatPatch>& patches) noexcept {
        const int hero = HeroClass();
        // Do not let one unavailable cache row prevent clearing later affected rows.
        InvalidateAffectedSlots(patches,hero,[&](int ref,int index){auto cell=Cell(*Cell(*Member("fast_check_skills_value"),ref),index);if(Numeric(cell))*cell=YYTK::RValue(-1.0);});
    }
};
}
