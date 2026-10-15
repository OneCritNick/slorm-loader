# Slorm Loader

Windows x64 mod runtime for **The Slormancer**. It bundles Aurie Core, YYToolkit and the Slorm Loader module, with guarded JSON skill edits, native-mod status, menu counts, logs and installation/recovery tools.

This repository stays **private until the maintainer approves an actual release**. It contains only the runtime, its dependencies, build/package scripts and runtime tests. The Vortex extension, creator and gameplay mods are separate projects.

## Build the complete package

Install Visual Studio 2022 C++ Build Tools, Windows SDK 10.0.22621.0, Node.js 22 or later and .NET Framework 4.x. In PowerShell from this checkout:

```powershell
.\build.ps1
```

That command compiles the runtime/helper/patcher, runs the native API and Core startup tests, obtains the SHA-256-pinned official YYToolkit 5.0.0c DLL, creates the complete ZIP, then tests setup/restoration using that ZIP and a benign test executable. It never launches the game. See [build options](docs/BUILD.md) for explicit toolchain paths or offline dependency input.

The ZIP and its SHA256SUMS.txt are placed together under a fresh `build/packages/<build-id>/` directory. `build/latest-package.json` identifies the current output. GitHub Actions builds and uploads the same complete development package.

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/` | Loader, stat API, JSON schema and setup helper |
| `third_party/` | Runtime dependencies, original Core test baseline and pinned SDK |
| `scripts/`, `tools/` | Compile, verify, test and package |
| `tests/` | Runtime regression tests and manifest fixtures; no installable gameplay packages |
| `docs/`, `licenses/` | Installation, bootstrap explanation, provenance and notices |

The installable ZIP includes SlormLoader.dll, AurieCore.dll, official YYToolkit.dll, SlormLoaderSetup.exe, AuriePatcher.exe, runtime metadata/checksums, notices and the source needed to rebuild it. It enables no gameplay mod by itself. Source under `source/tests/fixtures` is test data, not deployed mods.

## Install and understand

Unzip the .zip inside the game folder and execute SlormLoaderSetup.exe inside the game diretory. When the setup finished there will be a little dialog window telling you.
Once finshed, you can put your Aurie mods into the "The Slormancer/mods/Aurie/" folder.

## Credits and licenses

Developed by nick with AI-assisted code/documentation. Aurie and YYToolkit provide the underlying framework. Native runtime code is AGPL-3.0; the setup helper and project scripts/docs use MIT. Dependency notices retain their own terms: see [LICENSE](LICENSE), [project MIT notice](licenses/PROJECT-MIT.txt) and [third-party credits](THIRD_PARTY_NOTICES.md).
