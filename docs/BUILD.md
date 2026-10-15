# Build the runtime package

Prerequisites: Windows x64, PowerShell, Visual Studio 2022 C++ Build Tools, Windows SDK 10.0.22621.0, Node.js 22+ and .NET Framework 4.x. Compatible Microsoft C++ runtime libraries are needed to run the built DLLs.

```powershell
.\build.ps1
```

Visual Studio is detected with vswhere. Explicit paths are supported:

```powershell
.\build.ps1 -VCToolsPath 'C:\BuildTools\VC\Tools\MSVC\14.40.33807' -WindowsSdkRoot 'C:\Program Files (x86)\Windows Kits\10'
```

The root command compiles five outputs: loader, setup, modified Core, original Core (test baseline only) and Patcher. It runs 130 native API checks, isolated Core startup/negative tests and setup transaction tests. It then creates the complete installable ZIP and validates the unpacked package using a benign PE fixture. No game files or saves are required or launched.

## YYToolkit dependency

The package uses the exact official x64 5.0.0c DLL pinned by `third_party/yytk-sdk/provenance.json`. Packaging downloads it from its fixed GitHub release URL and verifies SHA-256 before use; subsequent builds verify the cached copy. For an offline build, provide an existing matching DLL:

```powershell
.\build.ps1 -YYToolkitPath 'C:\Dependencies\YYToolkit.dll'
```

A mismatched DLL is rejected. The optional `scripts/build.ps1 -BuildYYToolkit` compiles the pinned experimental source as `YYToolkit-source-candidate.dll` for inspection. The packager does not substitute that unverified candidate for the accepted release binary. Source and binary reproducibility limits are documented in VERIFYING.md.

## Outputs and individual steps

Every package goes into a fresh `build/packages/<build-id>/` directory, including the ZIP, SHA256SUMS.txt and staged runtime. `build/latest-package.json` identifies it. Existing packages and installed games are not overwritten.

For development, the individual commands are `scripts/build.ps1`, `scripts/test.ps1`, `scripts/package.ps1`, and `scripts/test-package.ps1 -ArchivePath <zip>`. Compiler/SDK options are accepted by build and test scripts. Packaging checks each compiled output against build-provenance.json and only copies an explicit runtime/source allowlist.

The ZIP includes a self-contained source tree with this same root build command. It contains dependency source/notices, runtime tests and fixtures, not the extension, creator, gameplay packages, compiler or game data. Complete source-file hashes and build/dependency metadata are stored under its docs folder. Filesystem permissions must allow disposable hardlinks under build for the native tests.

`docs/source-inventory.json` records the retained source snapshot. Intentional edits require reviewing/updating affected entries. `.gitattributes` preserves upstream SDK bytes. These repository-controlled hashes are audit aids, not publisher signatures.

Before publishing a newly compiled runtime, repeat the relevant live-game loading, gameplay and purge/redeploy checks. The generated package is labelled a development source build until that acceptance is complete.
