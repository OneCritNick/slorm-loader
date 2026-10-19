'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const root=path.resolve(__dirname,'..');
const read=f=>fs.readFileSync(path.join(root,f),'utf8');
let checks=0;
for(const file of ['src/SlormLoader.cpp','src/SlormLoaderStatus.hpp','src/SlormModStatus.hpp','src/SlormLoaderMetadata.hpp']) {
  assert(!/StatModSession|StatRuntime|YYTKBackend|CallGameScriptEx|MmCreateHook|SetInstanceMember|hero_skills|fast_check_skills|PASSIVE_VERIFY|TickDiagnostics/.test(read(file)),file);
  checks++;
}
for(const file of ['SlormStatRuntime.hpp','SlormStatApi.hpp','SlormModModel.hpp','SlormYYTKBackend.hpp','SlormFieldSpecs.hpp','SlormDiagnostics.hpp','SlormLiveDiagnostics.hpp','mod.schema.json','stat-fields.json']) {
  assert(!fs.existsSync(path.join(root,'src',file)),file);checks++;
}
const meta=JSON.parse(read('src/slorm-runtime.json'));
assert.equal(meta.version,'0.6.0');assert(!('patches' in meta));checks++;
assert(read('src/SlormLoaderStatus.hpp').includes('Disable this old JSON mod in Vortex'));checks++;
assert(!/SlormStat|stat-api|generate-field-spec/.test(read('scripts/build.ps1')+read('scripts/test.ps1')));checks++;
console.log(`PASS ${checks} Loader-only architecture and retired JSON checks`);
