# Current 0.4.x proxy startup

The game executable remains unchanged. SHA-256-pinned Ultimate ASI Loader 9.0.0 is distributed unchanged as version.dll and forwards the Windows version APIs. It loads plugins/AurieCore.asi through LoadLibrary, then calls Core's optional InitializeASI export outside DllMain.

Core starts its existing worker from DllMain. InitializeASI waits on an event while that worker loads startup modules, dispatches ModuleEntrypoint and ModulePreinitialize, then signals early readiness. The ASI callback returns so GameMaker can create its runner interface and game window. Core then dispatches ModuleInitialize. The wait is bounded at 30 seconds and emits an OutputDebugString diagnostic on timeout.

Startup passes use an explicit phase rather than the main thread's suspension state. This also defers initialization of dependencies loaded from preinitialize callbacks. Previously, a running thread caused priority loading to be skipped or ModuleInitialize to run before ModuleEntrypoint. YYToolkit therefore never captured the runner interface, and dependent mods failed with dependency-not-resolved errors.

The original Aurie exports are preserved; InitializeASI is the only added export. Legacy suspension/resume remains supported when suspension is positively detected. The proxy INI keeps DontLoadFromDllMain=1, which is required for waiting safely outside the Windows loader lock.

Setup validates pins, restores a verified owned legacy EXE patch when migrating, rejects unowned patches or duplicate legacy Core DLLs, then records mode=version-proxy. Subsequent proxy setup/check/removal does not alter the EXE. Extension 0.2.11 is recommended for deployment and removal; it also preserves the runtime name/version.

Tests exercise both an ASI callback that waits for early readiness and a directly loaded Core with a running main thread, including a nested module load. They verify exact callback order, API forwarding and unchanged fixture EXEs. The user verified loading and modded items on 0.4.1; fresh builds such as 0.4.2 require their own game test. Windows application-control checks still apply to both Core and the upstream loader.

# Historical executable-patching bootstrap

The sections below describe 0.3.x behavior retained for migration and recovery analysis.
# Setup helper and the game EXE

## SlormLoaderSetup.exe

This is a small .NET installation/recovery program built from src/SlormLoaderSetup.cs. It supports install, check and restore. It is separate from the code that runs inside the game.

During install it:

1. Refuses changes while The Slormancer is running, checks package hashes and validates paths/x64 executable layout. It rejects unsupported links and a competing version.dll bootstrap.
2. Copies the original game EXE into .slorm-loader/state/original.exe, retaining hashes and a transaction journal for interrupted operations.
3. Invokes the packaged tools/AuriePatcher.exe with the selected EXE and absolute mods/Native/AurieCore.dll path.
4. Verifies the new .aurie section and records original/patched hashes plus the Core path in bootstrap.json. Repeated setup verifies the same owned installation.

Check validates the package and owned bootstrap, then presents the last recorded runtime/mod status. Restore verifies the saved original and current EXE before copying back the exact original; it does not blindly restore over an externally changed game update. Operations append setup.log. Dialogs explain success/failure; Vortex uses the quiet command mode.

The helper does not change skill values itself. It does not edit saves or data.win, fetch/download code, install services or change antivirus settings. It delegates the actual EXE modification to AuriePatcher.

## Exact EXE changes

The upstream AuriePatcher implementation is in third_party/aurie-original/AuriePatcher/source/main.cpp. Its install path adds a readable/writable/executable PE section named .aurie. It copies its mapped patcher image into that section, stores the original entrypoint offset and the selected Core DLL's absolute path, and updates the PE section/image headers and AddressOfEntryPoint to point to ArProcessInitialize inside the added section.

On game startup, that bootstrap resolves ntdll functions and loads the stored DLL through LdrLoadDll. If loading fails, it calls the original entrypoint. If loading succeeds, it suspends the initial thread while Aurie initializes; the game then resumes through its original entrypoint. Aurie supplies native module loading/hooks, YYToolkit supplies GameMaker access, and SlormLoader.dll applies/reports supported mods.

This is a real on-disk executable modification, not just a configuration setting or an import of skill data. The embedded payload is the patcher's mapped image, not merely a short filename string. The setup helper's owned backup/receipt is why exact restoration and update guards are available. Normal Vortex lifecycle integration automates setup/restore but is outside this repository.

## Why this startup method?

The unmodified game does not invoke our Core DLL. The chosen AuriePatcher bootstrap provides early initialization while allowing the game to be launched through its normal EXE, Steam or Vortex. EXE patching is a choice of bootstrap, not an inherent requirement for JSON skill edits. Replacing it would require implementing/testing a different startup mechanism and updating install/removal integration.

A scanner result cannot be attributed to SlormLoaderSetup.exe from its filename alone. The helper invokes a patcher; AuriePatcher changes PE headers, adds executable content and redirects startup. Aurie/YYToolkit also load native code and hook the game. Identify the exact flagged file/hash and detection report before deciding which component is responsible. Removing only the wrapper would not remove the patcher or this bootstrap behavior.
