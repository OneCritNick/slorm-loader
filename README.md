# Slorm Loader 0.6.1

Windows x64 mod runtime for The Slormancer. Bundles Aurie Core, YYToolkit and the Slorm Loader module. Legendary API, Reaper API, Lua Runtime and gameplay mods are separate packages.

## Downloads

- **Slorm-Loader-0.6.1.zip**: lean player download for Nexus/Vortex. Includes runtime files, manual setup/recovery scripts, installation instructions and licenses.
- **Slorm-Loader-0.6.1-Full.zip**: complete GitHub bundle with the same runtime files plus corresponding source, dependency sources, tests, build tools and developer documentation.

Both use the same version and identical runtime files. Install only one. GitHub's repository source ZIP remains complete; it contains source and build tools rather than prebuilt runtime DLLs. The player ZIP includes a source reference in docs/SOURCE.md.

The player ZIP contains **no compiled EXE files**. Game startup uses the same pinned version.dll proxy, plugins/AurieCore.asi and official YYToolkit 5.0.0c as 0.4.2. The Loader DLL identifies itself as 0.6.1. Native DLL/ASI code remains necessary; removing setup EXEs does not bypass Windows application control.

## Installation
### Vortex
Close the game. Install Vortex Extension **0.5.0** first, then restart Vortex. Disable the previous runtime, deploy, install Slorm Loader 0.6.1 through Mods, enable it and deploy. Enable only one runtime. Use Hardlink Deployment with staging on the same NTFS volume as the game.

The extension performs setup and recovery directly in JavaScript. Mods toolbar actions provide Configure Slorm Loader, Check Slorm Loader and Restore loader setup. No setup EXE, PowerShell script or separate Node installation is needed with Vortex. Archive/user names and versions are preserved.

### For manual installation:
Simply download and unzip the mod loader in the main game directory. You can then manuall move mod .dll files into the Aurie/Lua folder, see below.

### Mod file structure:
For a native DLL Aurie/YYToolkit mod, use:
```
MyMod.zip
└── mods/
    └── Aurie/
        └── MyMod.dll
```
or
```
MyMod.zip
└── MyMod.dll
```
Vortex installs these in: ./mods/Aurie
./mods/Aurie

For Lua mods:
```
MyLuaMod.zip
├─ lua-pack.json
├─ main.lua
└─ scripts/          ← optional helper scripts
  └─ helpers.lua
```
Vortex installs these in : ./mods/Lua/<mod-id>/

## Build and verify

Windows x64, Visual Studio 2022 C++ Build Tools, Windows SDK 10.0.22621.0, Node.js 22+ and PowerShell are developer prerequisites. Run .\build.ps1 from this checkout. It compiles the Loader and two Core DLLs, runs isolated tests, packages source and runtime, and verifies the actual ZIP. Test executables stay under build and are never included in the player ZIP. .NET is not a setup dependency.

See [build options](docs/BUILD.md), [test scope](docs/TESTING.md), [verification limits](docs/VERIFYING.md), [bootstrap behavior](docs/BOOTSTRAP.md), [security](SECURITY.md). JSON skill/passive patching has been removed. Gameplay APIs remain separate. This new bundle requires the user's installation and gameplay test; earlier working releases are prior compatibility evidence.

## Credits and licenses

Developed by nick with AI-assisted code/documentation. Native runtime code is AGPL-3.0; project setup/scripts/docs use MIT. Aurie, YYToolkit and other dependencies retain their own licenses: LICENSE, licenses/PROJECT-MIT.txt and THIRD_PARTY_NOTICES.md.

## Gameplay boundary

Loader 0.6.1 manages native startup, discovery/status and diagnostics about mod
loading. It performs no skill/passive table writes or gameplay script hooks.
Disable old JSON patches such as Irremovable and Full Plate Armor before
upgrading. Leftovers are reported as unsupported and never applied; native mods
continue loading. A separate Skill/Passive API may provide replacements later.
The shared Lua Runtime, Legendary/Reaper APIs and their existing mods are unchanged.

With -FrameworkArchive, tests verify the exact dependency archive pin and do not repeat Core/proxy startup tests for those unchanged shipped bytes. The Loader is newly built and tested. -PythonPath selects the developer Python interpreter; no Python binary is bundled.
