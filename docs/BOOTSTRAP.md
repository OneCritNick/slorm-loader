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
