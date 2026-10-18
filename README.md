# Slorm Loader

Windows x64 mod runtime for **The Slormancer**, bundling Aurie Core, YYToolkit and the Slorm Loader module. It provides native mod loading/status, guarded JSON skill edits, menu counts and diagnostics. The Vortex extension, Legendary API, Lua loader and gameplay mods are separate packages.

## Current release

**0.4.2 is a fresh bundle with corrected documentation and consistent versioning.** The ZIP, runtime manifest and runtime logs/status all identify Slorm Loader as 0.4.2. It retains the proxy startup fix from 0.4.1 and introduces no new gameplay features.

The proxy bootstrap leaves the game executable unchanged. It installs the pinned Ultimate ASI Loader 9.0.0 as `version.dll`, Aurie Core as `plugins/AurieCore.asi`, and official YYToolkit 5.0.0c plus SlormLoader.dll under `mods/Aurie`. No gameplay mod is enabled by the runtime itself.

The previous 0.4.1 runtime was verified by the user in game: YYToolkit initialized, seven installed native mods loaded, and the current session reported zero errors and no pending mods. Modded items worked. This is validation on one setup, not a guarantee for every game/mod combination or Windows application-control policy. See [release notes](docs/RELEASE-NOTES.md) for validation scope. The freshly built 0.4.2 bundle still needs its own user game test.

## Install

With the game closed, install the separate [Vortex extension](https://www.nexusmods.com/site/mods/2455), version **0.2.11** or later, through Extensions and restart Vortex. Install Slorm Loader through **Mods**, then deploy. Use Hardlink Deployment with staging on the same NTFS volume as the game. Extension 0.2.11 preserves the loader's archive/user name and version.

For manual installation, unzip the runtime into the folder containing `The Slormancer.exe` and run `SlormLoaderSetup.exe`. It defaults to install and shows a completion or error dialog. Place native Aurie mods under `mods/Aurie`. Keep the archive's directory structure. See [installation and migration](docs/INSTALL.md) before upgrading an older 0.3.x executable-patching installation.

## Build

Install Visual Studio 2022 C++ Build Tools, Windows SDK 10.0.22621.0, Node.js 22+ and .NET Framework 4.x. From this checkout in PowerShell:

```powershell
.\build.ps1
```

This builds source, runs isolated tests, obtains pinned dependencies, packages a runtime ZIP and verifies it using a benign test executable. It never launches the game. Every package uses a new `build/packages/<build-id>/` directory; `build/latest-package.json` identifies the output. See [build options](docs/BUILD.md), [test scope](docs/TESTING.md) and [verification limits](docs/VERIFYING.md). A fresh source/CI build is not claimed byte-identical to the accepted release binaries.

| Path | Purpose |
| --- | --- |
| `src/` | Loader, stat API, metadata and setup helper |
| `third_party/` | Runtime dependencies, original Core baseline and SDK |
| `scripts/`, `tools/`, `tests/` | Build, verification and isolated regression tests |
| `docs/`, `licenses/` | Installation, release evidence, provenance and notices |

Read [bootstrap behavior](docs/BOOTSTRAP.md), [security behavior](SECURITY.md) and [the JSON stat API](docs/STAT-MOD-API.md) for implementation details. Regression fixtures and bundled source are not deployed gameplay mods.

## Credits and licenses

Developed by nick with AI-assisted code/documentation. Aurie and YYToolkit provide the underlying framework. Native runtime code is AGPL-3.0; the setup helper and project scripts/docs use MIT. Dependencies retain their own terms: [LICENSE](LICENSE), [project MIT notice](licenses/PROJECT-MIT.txt), [third-party credits](THIRD_PARTY_NOTICES.md).
