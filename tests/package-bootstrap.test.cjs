'use strict';
// Actual unpacked package; all operations run against a disposable inert PE.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const packageRoot=path.resolve(process.argv[2]),out=path.resolve(process.argv[3]);
const bootstrap=require(path.join(packageRoot,'tools/bootstrap'));
fs.mkdirSync(out,{recursive:true});
const pe=Buffer.alloc(2048);pe.write('MZ');pe.writeUInt32LE(128,60);pe.write('PE\0\0',128,'binary');pe.writeUInt16LE(0x8664,132);pe.writeUInt16LE(1,134);pe.writeUInt16LE(240,148);pe.writeUInt16LE(0x20b,152);pe.write('.text',392);
const game=path.join(out,'game');fs.mkdirSync(game);
for(const name of [...bootstrap.REQUIRED,'slorm-loader-checksums.json']){const dest=path.join(game,name);fs.mkdirSync(path.dirname(dest),{recursive:true});fs.copyFileSync(path.join(packageRoot,name),dest);}
fs.writeFileSync(path.join(game,'The Slormancer.exe'),pe);fs.writeFileSync(path.join(game,'data.win'),'inert fixture');
const before=bootstrap.hash(game,'The Slormancer.exe'),engine=bootstrap.createBootstrap({stopped(){}});
engine.install(game);engine.install(game);engine.check(game);assert.equal(bootstrap.hash(game,'The Slormancer.exe'),before);
engine.restore(game);engine.restore(game);assert(!bootstrap.receipt(game));
const core=path.join(game,'plugins/AurieCore.asi');fs.writeFileSync(core,'corrupted');assert.throws(()=>engine.install(game),/missing or changed/);assert.equal(bootstrap.hash(game,'The Slormancer.exe'),before);assert(!bootstrap.receipt(game));
for(const name of ['bootstrap.js','strict-json.js'])assert.deepEqual(fs.readFileSync(path.join(packageRoot,'tools',name)),fs.readFileSync(path.join(packageRoot,'source/src',name)));
const walk=dir=>fs.readdirSync(dir,{withFileTypes:true}).flatMap(e=>e.isDirectory()?walk(path.join(dir,e.name)):[path.join(dir,e.name)]);
assert(!walk(packageRoot).some(f=>/\.exe$/i.test(f)),'No compiled EXE anywhere in the player archive');
fs.writeFileSync(path.join(out,'results.json'),JSON.stringify({result:'passed',packageInstallRepeatCheckRestore:true,tamperedCoreRejected:true,exePreserved:true,noBundledExe:true,gameLaunched:false}));
console.log('PASS actual ZIP install/repeat/check/restore, tamper rejection, shared JS equality and no bundled EXE; inert fixture only');
