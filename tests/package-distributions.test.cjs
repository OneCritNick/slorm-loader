'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const full=path.resolve(process.argv[2]),nexus=path.resolve(process.argv[3]);
const walk=(root,prefix='')=>fs.readdirSync(path.join(root,prefix),{withFileTypes:true}).flatMap(e=>{
  const rel=prefix+e.name;
  assert(!e.isSymbolicLink(),'Packages must not contain links');
  return e.isDirectory()?walk(root,rel+'/'):[rel];
}).sort();
const expected=[
  'version.dll','version.ini','plugins/AurieCore.asi','mods/Aurie/YYToolkit.dll','mods/Aurie/SlormLoader.dll',
  'slorm-runtime.json','slorm-loader-checksums.json',
  'tools/slorm-loader-setup.js','tools/bootstrap.js','tools/strict-json.js',
  'docs/README.md','docs/RELEASE-NOTES.md','docs/SECURITY.md','docs/BOOTSTRAP.md','docs/SOURCE.md',
  'docs/CREDITS.md','docs/LICENSE-AGPL.txt','docs/LICENSE-setup-MIT.txt','docs/LICENSE-json-MIT.txt',
  'docs/LICENSE-Ultimate-ASI-Loader.txt','docs/THIRD-PARTY-NOTICES.txt'
].sort();
assert.deepEqual(walk(nexus),expected,'Nexus contains only player files, setup/recovery, instructions and notices');
for(const file of expected)assert.deepEqual(fs.readFileSync(path.join(nexus,file)),fs.readFileSync(path.join(full,file)),`Distribution mismatch: ${file}`);
for(const dir of ['src','scripts','tests','third_party','tools','docs','licenses','.github'])assert(fs.statSync(path.join(full,'source',dir)).isDirectory(),`Full source retains ${dir}`);
assert(fs.existsSync(path.join(full,'docs/source-checksums.json')));
const meta=JSON.parse(fs.readFileSync(path.join(nexus,'slorm-runtime.json'),'utf8').replace(/^\uFEFF/,''));
assert(fs.readFileSync(path.join(nexus,'docs/SOURCE.md'),'utf8').includes(`Slorm-Loader-${meta.version}-Full.zip`));
assert(fs.readFileSync(path.join(nexus,'docs/SOURCE.md'),'utf8').includes('https://github.com/OneCritNick/slorm-loader'));
console.log(`PASS lean ${expected.length}-file Nexus allowlist, identical runtime/setup/notices and complete Full source`);
