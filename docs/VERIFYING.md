# Verifying the runtime build

Each complete ZIP includes docs/build-info.json with actual compiler/SDK information and the origin/hash of its official YYToolkit DLL. slorm-loader-checksums.json contains the component pins used by setup. docs/source-checksums.json records every bundled source file. SHA256SUMS.txt beside the archive records its exact ZIP hash.

```powershell
Get-FileHash .\Slorm-Loader-0.4.2.zip -Algorithm SHA256
```

The package builder checks compiled outputs against build-provenance.json before staging and rejects YYToolkit binaries that differ from the pinned official x64 5.0.0c release. The ZIP is unpacked and tested after creation. Source includes the compile/test/package scripts needed to repeat that process from its own source folder.

Aurie uses the retained 2.0.2 Core/Patcher sources, with a four-file console fix and three-file proxy startup fix. Core retains every upstream export and adds InitializeASI. SDK hashes in third_party/yytk-sdk/provenance.json identify the selected v5 headers. tools/verify-source.js checks the retained source inventory, SDK and exact seven-file Core diff. Dependency source distributions retain their licenses and original notices; unrelated installers/examples are omitted.

The official YYToolkit DLL has not been reproduced byte for byte from the experimental source. The upstream release tag has stale v4 headers, so the SDK/source is pinned to commit d5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54. The optional source candidate is not substituted in runtime packages. Timestamps, toolchains and build metadata also mean independently built Core/loader/setup/Patcher hashes need not equal historical package hashes.

The repository contains only the runtime and its source/dependencies. CI builds, tests and uploads its own development ZIP. Public trusted main pushes can attest that CI output; private builds skip that job. Attestations identify the repository/commit/workflow for their subjects, not an equivalence to a previous download.

The historical release-provenance-0.3.10.json describes a previously prepared package, not newly generated build bytes. Current outputs use their own build-info and checksum files. Live-game loading/gameplay and Vortex profile/purge/redeploy checks remain acceptance steps before promoting a rebuilt runtime.

The 0.4.2 bundle is a fresh build with matching package and runtime versions. The successful user game test of 0.4.1 is prior compatibility evidence; it does not make independently built 0.4.2 binaries live-game-tested. See RELEASE-NOTES.md and TESTING.md for the exact acceptance scope.
