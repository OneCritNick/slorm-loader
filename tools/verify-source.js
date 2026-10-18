'use strict';
// Source provenance check only: never starts native binaries or reads game files.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const inventory = require('../docs/source-inventory.json');
for (const [relative, expected] of Object.entries(inventory.files)) {
  const file = path.resolve(root, relative);
  assert(file.startsWith(root + path.sep), 'Inventory path escapes repository');
  assert.equal(hash(file), expected, `Source differs from recorded snapshot: ${relative}`);
}
const sdk = require('../third_party/yytk-sdk/provenance.json');
assert.equal(sdk.sourceCommit, 'd5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54');
assert.equal(sdk.release, 'v5.0.0c');
for (const [relative, expected] of Object.entries(sdk.files)) {
  if (relative === 'YYToolkit.dll') continue; // Not committed in this source repository.
  assert.equal(hash(path.join(root, 'third_party/yytk-sdk', relative)), expected, relative);
}
function files(folder, prefix = '') {
  const result = new Map();
  for (const item of fs.readdirSync(folder, {withFileTypes: true})) {
    const rel = prefix + item.name;
    if (item.isDirectory()) {
      for (const [name, value] of files(path.join(folder, item.name), rel + '/')) result.set(name, value);
    } else {
      assert(item.isFile(), `Nonregular upstream source: ${rel}`);
      result.set(rel, hash(path.join(folder, item.name)));
    }
  }
  return result;
}
const before = files(path.join(root, 'third_party/aurie-original/Aurie'), 'Aurie/');
const after = files(path.join(root, 'third_party/aurie/Aurie'), 'Aurie/');
const changed = [...new Set([...before.keys(), ...after.keys()])]
  .filter(file => before.get(file) !== after.get(file)).sort();
assert.deepEqual(changed, [
  'Aurie/AurieCore.rc',
  'Aurie/source/AurieMain.cpp',
  'Aurie/source/framework/Debugging/debug.cpp',
  'Aurie/source/framework/Debugging/owned_console_sink.hpp',
  'Aurie/source/framework/Module Manager/module.cpp',
  'Aurie/source/framework/Module Manager/module.hpp',
  'Aurie/source/include/spdlog/sinks/wincolor_sink-inl.h'
]);
console.log(`PASS ${Object.keys(inventory.files).length} source hashes, pinned SDK and exact seven-file Aurie console/bootstrap diff.`);
