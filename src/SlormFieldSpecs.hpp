// Generated from stat-fields.json by tools/generate-field-spec.js.
#pragma once
#include <array>
#include <stdexcept>
#include <string>
namespace Slorm {
enum class Field { Base, PerRank, ExtraNumber, ManaCost, ManaCostPerRank, Cooldown, PrecastTime, CastTime, Knockback };
struct FieldSpec {Field field; const char* id; int column; bool resultSlots; double minimum;};
inline constexpr std::array<FieldSpec,9> FieldSpecs{{
    {Field::Base,"base",4,true,-1000000000.0},
    {Field::PerRank,"perRank",5,true,-1000000000.0},
    {Field::ExtraNumber,"extraNumber",29,false,-1000000000.0},
    {Field::ManaCost,"manaCost",11,false,0.0},
    {Field::ManaCostPerRank,"manaCostPerRank",12,false,-1000000000.0},
    {Field::Cooldown,"cooldown",20,false,0.0},
    {Field::PrecastTime,"precastTime",22,false,0.0},
    {Field::CastTime,"castTime",24,false,0.0},
    {Field::Knockback,"knockback",26,false,0.0}
}};
inline const FieldSpec& GetFieldSpec(Field field) {for(const auto& f:FieldSpecs)if(f.field==field)return f;throw std::runtime_error("Unsupported stat field");}
inline const char* FieldName(Field field) {return GetFieldSpec(field).id;}
inline Field ParseField(const std::string& name) {for(const auto& f:FieldSpecs)if(name==f.id)return f.field;throw std::runtime_error("Unsupported stat field: "+name);}
}
