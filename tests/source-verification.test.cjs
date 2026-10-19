'use strict';
// Exercise the real verifier with virtual edits; never modify checkout files.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const assert=require('node:assert/strict'),{createRequire}=require('node:module');
const root=path.resolve(__dirname,'..'),tool=path.join(root,'tools/verify-source.js');
const source=fs.readFileSync(tool,'utf8'),localRequire=createRequire(tool);
function verify(edit){
  const fakeFs=Object.create(fs);
  fakeFs.readFileSync=(file,...args)=>{
    const data=fs.readFileSync(file,...args);
    if(edit&&path.resolve(file)===path.resolve(root,edit))return Buffer.isBuffer(data)?Buffer.concat([data,Buffer.from('\nvirtual edit\n')]):data+'\nvirtual edit\n';
    return data;
  };
  vm.runInNewContext(source,{__dirname:path.dirname(tool),console:{log(){}},
    require:name=>name==='node:fs'?fakeFs:localRequire(name)}, {filename:tool});
}
let checks=0;
verify();checks++;
const inventory=localRequire('../docs/source-inventory.json');
assert(!Object.keys(inventory.files).some(f=>f.endsWith('.md')),'Editable documentation must not be a fixed source pin');checks++;
for(const file of ['README.md','SECURITY.md','docs/BUILD.md']){verify(file);checks++;}
for(const file of ['src/SlormLoader.cpp','src/bootstrap.js','scripts/package.ps1']){
  assert.throws(()=>verify(file),error=>error.code==='ERR_ASSERTION',file+' must remain protected');checks++;
}
const sdk=localRequire('../third_party/yytk-sdk/provenance.json');
const file=Object.keys(sdk.files).find(f=>f!=='YYToolkit.dll');
assert.throws(()=>verify('third_party/yytk-sdk/'+file),error=>error.code==='ERR_ASSERTION');checks++;
console.log(`PASS ${checks} source-verification checks: documentation edits accepted; code, recovery, packaging and SDK edits still rejected`);
