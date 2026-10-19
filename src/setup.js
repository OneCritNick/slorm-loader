#!/usr/bin/env node
'use strict';
// Optional manual-install CLI. Vortex uses its own bundled JavaScript module.
const path=require('path'),{createBootstrap}=require('./bootstrap');
const command=process.argv[2]||'install',game=path.resolve(process.argv[3]||path.join(__dirname,'..'));
if(process.argv.length>4||!['install','check','restore'].includes(command)){
  console.error('Usage: node tools/slorm-loader-setup.js install|check|restore [game-folder]');process.exitCode=1;
}else try{console.log(createBootstrap().execute(game,command).message);}
catch(error){console.error('ERROR: '+error.message);process.exitCode=1;}
