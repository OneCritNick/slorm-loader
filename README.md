# Slorm Loader 0.6.0

Windows x64 mod runtime for The Slormancer. Bundles Aurie Core, YYToolkit and the Slorm Loader module. Legendary API, Reaper API, Lua Runtime and gameplay mods are separate packages.

The player ZIP contains **no compiled EXE files**. Game startup uses the same pinned version.dll proxy, plugins/AurieCore.asi and official YYToolkit 5.0.0c as 0.4.2. The Loader DLL identifies itself as 0.6.0. Native DLL/ASI code remains necessary; removing setup EXEs does not bypass Windows application control.

## Install with Vortex

Close the game. Install Vortex Extension **0.5.0** first, then restart Vortex. Disable the previous runtime, deploy, install Slorm Loader 0.6.0 through Mods, enable it and deploy. Enable only one runtime. Use Hardlink Deployment with staging on the same NTFS volume as the game.

The extension performs setup and recovery directly in JavaScript. Mods toolbar actions provide Configure Slorm Loader, Check Slorm Loader and Restore loader setup. No setup EXE, PowerShell script or separate Node installation is needed with Vortex. Archive/user names and versions are preserved.

Manual users should read [installation](docs/INSTALL.md), especially for an older executable-patching installation. An optional tools/slorm-loader-setup.js command uses an existing Node.js 22+ installation. No Node binary is bundled.

## Build and verify

Windows x64, Visual Studio 2022 C++ Build Tools, Windows SDK 10.0.22621.0, Node.js 22+ and PowerShell are developer prerequisites. Run .\build.ps1 from this checkout. It compiles the Loader and two Core DLLs, runs isolated tests, packages source and runtime, and verifies the actual ZIP. Test executables stay under build and are never included in the player ZIP. .NET is not a setup dependency.

See [build options](docs/BUILD.md), [test scope](docs/TESTING.md), [verification limits](docs/VERIFYING.md), [bootstrap behavior](docs/BOOTSTRAP.md), [security](SECURITY.md). JSON skill/passive patching has been removed. Gameplay APIs remain separate. This new bundle requires the user's installation and gameplay test; earlier working releases are prior compatibility evidence.

## Credits and licenses

Developed by nick with AI-assisted code/documentation. Native runtime code is AGPL-3.0; project setup/scripts/docs use MIT. Aurie, YYToolkit and other dependencies retain their own licenses: LICENSE, licenses/PROJECT-MIT.txt and THIRD_PARTY_NOTICES.md.

## Gameplay boundary

Loader 0.6.0 manages native startup, discovery/status and diagnostics about mod
loading. It performs no skill/passive table writes or gameplay script hooks.
Disable old JSON patches such as Irremovable and Full Plate Armor before
upgrading. Leftovers are reported as unsupported and never applied; native mods
continue loading. A separate Skill/Passive API may provide replacements later.
The shared Lua Runtime, Legendary/Reaper APIs and their existing mods are unchanged.

With -FrameworkArchive, tests verify the exact dependency archive pin and do not repeat Core/proxy startup tests for those unchanged shipped bytes. The Loader is newly built and tested. -PythonPath selects the developer Python interpreter; no Python binary is bundled.
