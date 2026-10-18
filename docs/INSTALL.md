# Install Slorm Loader 0.4.2

This is a fresh 0.4.2 bundle. The ZIP, runtime metadata and logs/status all report 0.4.2. It retains the tested 0.4.1 proxy startup fix and changes no gameplay behavior. Existing API, Lua and gameplay packages do not need replacement for this update.

## Vortex

1. Close the game. Install The Slormancer extension **0.2.11 or later** through Vortex Extensions, then restart Vortex. It preserves the runtime's archive/user name and version.
2. Use Hardlink Deployment with the Mod Staging Folder on the same NTFS volume as the game. Linked game/mod directories and symlinks are unsupported.
3. Install this runtime ZIP through **Mods**, not Extensions. Replace the previous runtime, enable only one runtime version, and deploy. The extension configures setup automatically.
4. Start the game normally through Steam or Vortex. Native Aurie mods belong under `mods/Aurie`; JSON stat mods belong under `mods/Slormancer/<id>/mod.json`. Install those separately.

For a 0.3.x executable-patching installation, first disable the old runtime and deploy so its original executable is restored, then enable this package and deploy. Keep `.slorm-loader/state` and its original backup until migration is complete. Do not retain `mods/Native/AurieCore.dll` or a competing `version.dll` bootstrap. Setup rejects unowned legacy patches and damaged backups rather than guessing ownership.

## Manual installation

With the game closed, extract the ZIP into the folder containing `The Slormancer.exe` and `data.win`, preserving the directory structure. Run `SlormLoaderSetup.exe`; without arguments it installs and shows a completion/error dialog. The explicit commands are `SlormLoaderSetup.exe install`, `check` and `restore`.

The proxy files are `version.dll`, `version.ini` and `plugins/AurieCore.asi`. YYToolkit and SlormLoader.dll belong under `mods/Aurie`. Normal proxy setup does not patch the game executable. See [bootstrap details](BOOTSTRAP.md) for legacy migration behavior.

## Removal and troubleshooting

Close the game before changing deployments. Disable/purge through Vortex to remove its deployed files. For manual removal, run `SlormLoaderSetup.exe restore` first, then remove files belonging to this runtime. Do not delete other mods or legacy recovery state indiscriminately.

A successful mod scan is recorded in `.slorm-loader/mod-status.json`. Check `sessionPid` and fresh logs so an older session is not mistaken for the current launch. Native framework modules are excluded from the gameplay-mod count. Zero gameplay mods is normal when only the runtime is installed.

Retain fresh `aurie.log`, relevant `.slorm-loader/logs` and setup.log for troubleshooting; redact personal paths before sharing. Keep SAC enabled. Windows still evaluates both proxy and plugin binaries, and success on one PC does not establish trust on every PC.

The ZIP includes source, dependency notices, component checksums and release/validation notes. The runtime does not back up saves.
