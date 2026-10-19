'use strict';
function parseStrictJson(input) {
  if(Buffer.byteLength(input,'utf8')>65536)throw new Error('Manifest exceeds 64 KiB');
  const source=input.replace(/^\uFEFF/,'');const result=JSON.parse(source);let pos=0;
  const whitespace=()=>{while(/[\x20\x09\x0a\x0d]/.test(source[pos]||'x'))pos++;};
  const string=()=>{const start=pos++;while(pos<source.length){if(source[pos]==='\\'){pos+=2;continue;}if(source[pos++]==='"'){const value=JSON.parse(source.slice(start,pos));for(let i=0;i<value.length;i++){const c=value.charCodeAt(i);if(c>=0xd800&&c<=0xdbff){const next=value.charCodeAt(++i);if(!(next>=0xdc00&&next<=0xdfff))throw new Error('Unpaired Unicode surrogate');}else if(c>=0xdc00&&c<=0xdfff)throw new Error('Unpaired Unicode surrogate');}return value;}}throw new Error('Invalid JSON string');};
  const visit=depth=>{
    if(depth>32)throw new Error('Manifest nesting exceeds 32');whitespace();
    if(source[pos]==='{') {
      pos++;whitespace();const seen=new Set();if(source[pos]==='}'){pos++;return;}
      while(true){const key=string();if(seen.has(key))throw new Error(`Duplicate JSON key: ${key}`);seen.add(key);whitespace();pos++;visit(depth+1);whitespace();if(source[pos++]==='}')break;whitespace();}
    }else if(source[pos]==='[') {
      pos++;whitespace();if(source[pos]===']'){pos++;return;}while(true){visit(depth+1);whitespace();if(source[pos++]===']')break;whitespace();}
    }else if(source[pos]==='"')string();
    else {while(pos<source.length&&!/[\x20\x09\x0a\x0d,}\]]/.test(source[pos]))pos++;}
  };
  visit(0);return result;
}
module.exports={parseStrictJson};
