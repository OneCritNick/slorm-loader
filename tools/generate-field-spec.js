'use strict';
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'..'),source=path.join(root,'src/stat-fields.json');
const registry=JSON.parse(fs.readFileSync(source,'utf8'));const names=new Set(),columns=new Set();
for(const f of registry.fields) {
  if(!/^[a-zA-Z][a-zA-Z0-9]*$/.test(f.id)||names.has(f.id)||!Number.isInteger(f.column)||f.column<0||f.column>=registry.skillRowLength||columns.has(f.column))throw new Error('Invalid field registry');
  names.add(f.id);columns.add(f.column);
}
const body=`// Generated from stat-fields.json by tools/generate-field-spec.js.\n#pragma once\n#include <array>\n#include <stdexcept>\n#include <string>\nnamespace Slorm {\nenum class Field { ${registry.fields.map(f=>f.enum).join(', ')} };\nstruct FieldSpec {Field field; const char* id; int column; bool resultSlots; double minimum;};\ninline constexpr std::array<FieldSpec,${registry.fields.length}> FieldSpecs{{\n${registry.fields.map(f=>`    {Field::${f.enum},${JSON.stringify(f.id)},${f.column},${f.resultSlots},${f.minimum}.0}`).join(',\n')}\n}};\ninline const FieldSpec& GetFieldSpec(Field field) {for(const auto& f:FieldSpecs)if(f.field==field)return f;throw std::runtime_error("Unsupported stat field");}\ninline const char* FieldName(Field field) {return GetFieldSpec(field).id;}\ninline Field ParseField(const std::string& name) {for(const auto& f:FieldSpecs)if(name==f.id)return f.field;throw std::runtime_error("Unsupported stat field: "+name);}\n}\n`;
function writeChanged(file,text){if(!fs.existsSync(file)||fs.readFileSync(file,'utf8')!==text){fs.mkdirSync(path.dirname(file),{recursive:true});fs.writeFileSync(file,text);}}
writeChanged(path.join(root,'src/SlormFieldSpecs.hpp'),body);
const schemaPath=path.join(root,'src/mod.schema.json'),schema=JSON.parse(fs.readFileSync(schemaPath,'utf8'));
schema.$defs.target.properties.valueIndex={type:'integer',minimum:0,maximum:15,default:0};
schema.$defs.patch.properties.field.enum=registry.fields.map(f=>f.id);
schema.$defs.patch.allOf=registry.fields.filter(f=>f.minimum>-1e9).map(f=>({if:{properties:{field:{const:f.id}}},then:{properties:{value:{minimum:f.minimum}}}}));
schema.$defs.patch.allOf.push({if:{properties:{field:{enum:registry.fields.filter(f=>!f.resultSlots).map(f=>f.id)}}},then:{properties:{target:{properties:{valueIndex:{const:0}}}}}});
writeChanged(schemaPath,JSON.stringify(schema,null,2)+'\n');
