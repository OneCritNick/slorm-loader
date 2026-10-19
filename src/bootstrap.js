'use strict';
// Pure Node installer operations. No bundled/native helper is executed.
const fs=require('fs'),path=require('path'),crypto=require('crypto'),cp=require('child_process');
const {parseStrictJson}=require('./strict-json');
const HASH=/^[a-f0-9]{64}$/;
const REQUIRED=['version.dll','version.ini','plugins/AurieCore.asi','mods/Aurie/YYToolkit.dll','mods/Aurie/SlormLoader.dll','slorm-runtime.json'];
const digest=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
function safe(root,relative){
  root=path.resolve(root);
  if(typeof relative!=='string'||!relative||relative.includes(':')||/[\x00-\x1f]/.test(relative)||path.isAbsolute(relative)||relative.split(/[\\/]/).some(p=>!p||p==='.'||p==='..'))throw new Error('Invalid loader path.');
  const file=path.resolve(root,...relative.split(/[\\/]/));
  if(!file.startsWith(root+path.sep))throw new Error('Loader path escapes the selected game.');
  for(let current=file;;current=path.dirname(current)){
    try{if(fs.lstatSync(current).isSymbolicLink())throw new Error('Linked game/mod/state paths are unsupported: '+current);}
    catch(error){if(error.code!=='ENOENT')throw error;}
    if(path.dirname(current)===current)break;
  }
  return file;
}
function exists(root,name){const file=safe(root,name);try{if(!fs.statSync(file).isFile())throw new Error('Expected a regular loader file: '+name);return true;}catch(e){if(e.code==='ENOENT')return false;throw e;}}
function bytes(root,name,maximum=512*1024*1024){const file=safe(root,name);const stat=fs.statSync(file);if(!stat.isFile()||stat.size>maximum)throw new Error('Invalid or oversized loader file: '+name);return fs.readFileSync(file);}
function json(root,name){return parseStrictJson(bytes(root,name,65536).toString('utf8'));}
function hash(root,name){return digest(bytes(root,name));}
function atomic(root,name,value){
  const target=safe(root,name);fs.mkdirSync(path.dirname(target),{recursive:true});safe(root,name);
  const tmp=name+'.'+crypto.randomBytes(12).toString('hex')+'.tmp',file=safe(root,tmp);
  try{fs.writeFileSync(file,value,{flag:'wx'});safe(root,name);fs.renameSync(file,target);}
  finally{if(fs.existsSync(file))fs.unlinkSync(file);}
}
function write(root,name,value){atomic(root,name,JSON.stringify(value,null,2));}
function remove(root,name,expected){if(!exists(root,name))return;if(expected&&hash(root,name)!==expected)throw new Error('Recovery state changed externally: '+name);fs.unlinkSync(safe(root,name));}
function patched(data){
  if(data.length<64||data.toString('ascii',0,2)!=='MZ')throw new Error('Not a Windows executable.');
  const pe=data.readUInt32LE(60);
  if(pe<64||pe+24>data.length||data.toString('binary',pe,pe+4)!=='PE\0\0'||data.readUInt16LE(pe+4)!==0x8664)throw new Error('The loader requires the x64 game.');
  const count=data.readUInt16LE(pe+6),optional=data.readUInt16LE(pe+20),table=pe+24+optional;
  if(optional<112||pe+26>data.length||data.readUInt16LE(pe+24)!==0x20b||!count||count>96||table+count*40>data.length)throw new Error('Invalid x64 executable headers.');
  for(let i=0;i<count;i++)if(data.toString('ascii',table+i*40,table+i*40+8).replace(/\0.*$/,'').toLowerCase()==='.aurie')return true;
  return false;
}
function validateReceipt(record){
  const valid=n=>typeof n==='string'&&HASH.test(n);
  if(!record||Array.isArray(record)||typeof record!=='object'||!valid(record.original)||
     (record.mode===undefined?!valid(record.patched):record.mode!=='version-proxy'||!valid(record.proxy)))throw new Error('Invalid or unrecognized loader receipt; no recovery changes made.');
  return record;
}
function receipt(root){return exists(root,'.slorm-loader/state/bootstrap.json')?validateReceipt(json(root,'.slorm-loader/state/bootstrap.json')):undefined;}
function checkStopped(){
  const file=path.join(process.env.SystemRoot||'C:\\Windows','System32','tasklist.exe');
  const result=cp.spawnSync(file,['/FI','IMAGENAME eq The Slormancer.exe','/FO','CSV','/NH'],{encoding:'utf8',windowsHide:true,timeout:8000,maxBuffer:1024*1024});
  if(result.error||result.status!==0)throw new Error('Could not check whether The Slormancer is running. Close the game and retry.');
  if(/^"The Slormancer\.exe",/im.test(result.stdout||''))throw new Error('Close The Slormancer before configuring or restoring the loader.');
}
function verifyPackage(root){
  const pins=json(root,'slorm-loader-checksums.json');
  if(!pins||Array.isArray(pins)||typeof pins!=='object')throw new Error('Invalid loader checksum manifest.');
  const seen=new Set();
  for(const [relative,expected] of Object.entries(pins)){
    const normalized=relative.replace(/\\/g,'/');const canonical=REQUIRED.find(n=>n.toLowerCase()===normalized.toLowerCase());
    if(!canonical||seen.has(canonical)||typeof expected!=='string'||!HASH.test(expected))throw new Error('Invalid or duplicate loader checksum path: '+relative);
    seen.add(canonical);if(hash(root,canonical)!==expected)throw new Error('Loader package is missing or changed: '+canonical);
  }
  if(seen.size!==REQUIRED.length)throw new Error('Incomplete proxy runtime checksum manifest. Install Slorm Loader 0.4.x or later.');
  const metadata=json(root,'slorm-runtime.json');
  if(metadata.schemaVersion!==1||metadata.id!=='slormancer-runtime'||metadata.type!=='runtime'||!/^\d+\.\d+\.\d+$/.test(metadata.version))throw new Error('Invalid runtime package metadata.');
  return metadata;
}
function createBootstrap(options={}){
  const stopped=options.stopped||checkStopped;
  function log(root,message){try{const name='.slorm-loader/setup.log',file=safe(root,name);fs.mkdirSync(path.dirname(file),{recursive:true});
    // Appending through a hardlink could alter another file; use an atomic replacement.
    let previous=exists(root,name)?bytes(root,name,4*1024*1024).toString('utf8'):'';
    if(previous.length>1024*1024)previous=previous.slice(-512*1024);
    atomic(root,name,previous+new Date().toISOString()+' '+message+'\n');}catch{}}
  function recover(root){
    const name='.slorm-loader/state/pending.json';if(!exists(root,name))return;
    stopped();const pending=json(root,name),pendingHash=hash(root,name),current=hash(root,'The Slormancer.exe');
    if(pending.schemaVersion===2){
      if(pending.operation!=='restore-legacy'||!HASH.test(pending.before)||!HASH.test(pending.after)||!HASH.test(pending.receiptHash))throw new Error('Invalid interrupted restoration record.');
      const target=bytes(root,'.slorm-loader/state/original.exe');if(digest(target)!==pending.after)throw new Error('Recovery backup checksum mismatch.');
      if(current!==pending.before&&current!==pending.after)throw new Error('Game executable changed externally; interrupted restoration stopped.');
      const saved=receipt(root);
      if(saved){if(saved.mode!==undefined||saved.original!==pending.after||![saved.original,saved.patched].includes(pending.before)||hash(root,'.slorm-loader/state/bootstrap.json')!==pending.receiptHash)throw new Error('Interrupted restoration no longer matches its owned receipt.');}
      else if(current!==pending.after)throw new Error('Interrupted restoration has no owned receipt.');
      if(current===pending.before)atomic(root,'The Slormancer.exe',target);
      if(hash(root,'The Slormancer.exe')!==pending.after)throw new Error('Restored executable checksum mismatch.');
      remove(root,'.slorm-loader/state/bootstrap.json',pending.receiptHash);remove(root,name,pendingHash);
      return;
    }
    // Legacy helpers did not record the transaction's final hash. Never use
    // a pending file alone as authorization to replace an unknown game update.
    if(!HASH.test(pending.hash))throw new Error('Invalid legacy recovery record.');
    const backup=bytes(root,'.slorm-loader/state/transaction.exe');if(digest(backup)!==pending.hash)throw new Error('Recovery backup checksum mismatch.');
    let prior;if(pending.receipt!==undefined){if(typeof pending.receipt!=='string')throw new Error('Invalid legacy transaction receipt.');prior=validateReceipt(parseStrictJson(pending.receipt));}
    const known=[pending.hash];const live=receipt(root);
    for(const r of [prior,live])if(r)for(const key of ['original','patched'])if(HASH.test(r[key]))known.push(r[key]);
    if(!known.includes(current))throw new Error('Ambiguous legacy transaction: current EXE has no verified owned hash. Retain the backup and recovery logs.');
    if(current!==pending.hash)atomic(root,'The Slormancer.exe',backup);
    if(prior)write(root,'.slorm-loader/state/bootstrap.json',prior);else remove(root,'.slorm-loader/state/bootstrap.json');
    remove(root,name,pendingHash);remove(root,'.slorm-loader/state/transaction.exe',pending.hash);
  }
  function restoreLegacy(root,record){
    const backup=bytes(root,'.slorm-loader/state/original.exe');if(digest(backup)!==record.original)throw new Error('Original executable backup checksum mismatch.');
    if(patched(backup))throw new Error('Original executable backup contains a legacy patch.');
    const current=hash(root,'The Slormancer.exe');if(current!==record.patched&&current!==record.original)throw new Error('Game executable changed externally; restoration stopped.');
    const receiptHash=hash(root,'.slorm-loader/state/bootstrap.json');
    write(root,'.slorm-loader/state/pending.json',{schemaVersion:2,operation:'restore-legacy',before:current,after:record.original,receiptHash});
    if(options.beforeRestore)options.beforeRestore(root); // Fault injection in isolated tests only.
    if(hash(root,'The Slormancer.exe')!==current)throw new Error('Game executable changed during restoration; no replacement made.');
    if(hash(root,'.slorm-loader/state/original.exe')!==record.original)throw new Error('Original executable backup changed during restoration.');
    if(current!==record.original)atomic(root,'The Slormancer.exe',backup);
    if(hash(root,'The Slormancer.exe')!==record.original)throw new Error('Restored executable checksum mismatch.');
    remove(root,'.slorm-loader/state/bootstrap.json',receiptHash);remove(root,'.slorm-loader/state/pending.json');
  }
  function status(root){
    const file='.slorm-loader/mod-status.json';if(!exists(root,file))return {message:'No mod status is available yet. Launch after deploying mods.'};
    try{const report=JSON.parse(bytes(root,file,4*1024*1024).toString('utf8').replace(/^\uFEFF/,''));
      const counts=['active','errors','pending'].map(k=>Number.isSafeInteger(report[k])&&report[k]>=0?report[k]:'unknown');
      return {sessionPid:report.sessionPid,message:`Last recorded session: ${counts[0]} loaded / ${counts[1]} errors / ${counts[2]} pending. Relaunch after deployment changes.`};
    }catch{return {message:'The previous mod-status report could not be read. See runtime logs.'};}
  }
  function install(root){
    stopped();recover(root);const metadata=verifyPackage(root);
    if(!exists(root,'data.win'))throw new Error('Not the folder containing The Slormancer.exe and data.win.');
    const core=safe(root,'plugins/AurieCore.asi');if(core.length>=260)throw new Error('Aurie requires a game path shorter than 260 characters.');
    const old=receipt(root);if(old&&old.mode===undefined)restoreLegacy(root,old);
    if(patched(bytes(root,'The Slormancer.exe')))throw new Error('An unowned Aurie executable patch remains. Restore the original game before installing the proxy runtime.');
    if(exists(root,'mods/Native/AurieCore.dll'))throw new Error('Legacy mods/Native/AurieCore.dll remains deployed; disable the old runtime to prevent Core initializing twice.');
    const record={version:metadata.version,mode:'version-proxy',original:hash(root,'The Slormancer.exe'),proxy:hash(root,'version.dll'),corePath:core};
    write(root,'.slorm-loader/state/bootstrap.json',record);
    return {message:'Slorm Loader '+metadata.version+' configured. The game executable is unchanged.',version:metadata.version};
  }
  function restore(root){
    stopped();recover(root);const old=receipt(root);
    if(!old)return {message:'No loader setup record remains.'};
    if(old.mode===undefined){restoreLegacy(root,old);return {message:'Owned legacy executable patch restored. Vortex handles removal of deployed loader files.'};}
    // Clearing a proxy receipt never replaces the EXE, including after Steam
    // has updated it. An external entrypoint patch still requires its owner.
    if(patched(bytes(root,'The Slormancer.exe')))throw new Error('An unowned executable patch is present; proxy restoration stopped.');
    remove(root,'.slorm-loader/state/bootstrap.json');
    return {message:'Loader setup record removed. Disable/purge the runtime in Vortex to remove proxy files. The game executable is unchanged.'};
  }
  function check(root){
    stopped();const metadata=verifyPackage(root),record=receipt(root);
    if(!record||record.mode!=='version-proxy')throw new Error('Proxy loader is not configured. Configure Slorm Loader after deploying it.');
    if(patched(bytes(root,'The Slormancer.exe'))||hash(root,'The Slormancer.exe')!==record.original||hash(root,'version.dll')!==record.proxy)throw new Error('Proxy installation or game executable changed. Redeploy and configure the runtime.');
    return {message:'Slorm Loader '+metadata.version+' installation verified. '+status(root).message,version:metadata.version};
  }
  function execute(root,command){root=path.resolve(root);if(!['install','check','restore'].includes(command))throw new Error('Unknown loader operation.');
    try{const out=({install,check,restore})[command](root);log(root,'SUCCESS '+command+' '+out.message);return out;}
    catch(error){log(root,'ERROR '+command+' '+error.message);throw error;}
  }
  return {execute,install:root=>execute(root,'install'),check:root=>execute(root,'check'),restore:root=>execute(root,'restore')};
}
module.exports={createBootstrap,safe,exists,atomic,write,hash,digest,patched,verifyPackage,receipt,checkStopped,REQUIRED};
