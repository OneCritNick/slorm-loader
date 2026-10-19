# Slorm Loader 0.6.0 installation

## Vortex

1. Close The Slormancer.
2. Install Vortex Extension 0.5.0 through Extensions and restart Vortex.
3. Disable the previous Slorm Loader runtime and deploy. Retain .slorm-loader recovery state until migration/removal succeeds.
4. Install Slorm-Loader-0.6.0.zip through Mods. Enable exactly one runtime and deploy using Hardlink Deployment. The staging folder must be on the same NTFS volume as the game.
5. The extension configures the loader directly in JavaScript. Use Check Slorm Loader in the Mods toolbar to inspect setup. Launch the game yourself after deploying your APIs/content.

No separate setup executable, Node installation or PowerShell script is needed with Vortex. The extension supplies Configure Slorm Loader, Check Slorm Loader and Restore loader setup actions in the Mods toolbar. They operate only on the selected Slormancer folder while the game is closed. A check describes the installation and last recorded game session; deployment is not proof that a later game session loaded every mod.

Normal proxy setup never changes The Slormancer.exe. version.dll loads plugins/AurieCore.asi; Aurie loads YYToolkit.dll and SlormLoader.dll from mods/Aurie. Runtime installation alone enables no gameplay mod.

## Upgrade from older versions

Version 0.4.x already uses this proxy architecture. The extension recognizes old proxy packages but does not deploy their compiled setup helper. Replace the old package rather than enabling two runtimes.

Version 0.3.x modified the executable. With its original .slorm-loader/state intact, the extension can restore only an owned executable hash from a checksum-verified original.exe backup. It rejects unknown patches, damaged backups, external game updates and duplicate mods/Native/AurieCore.dll. Old ambiguous interrupted transactions are retained for manual investigation, never guessed. Preserve the logs/state and seek help if migration refuses. Steam file verification can restore an unmodified game, but it does not remove independently deployed mod files.

## Removal

Close the game, disable the runtime in Vortex and deploy or purge. The extension removes the setup receipt and Vortex removes its deployed files. Owned legacy executable patches are restored first. Saves, manual gameplay mods and original backups are retained. Restore loader setup alone clears setup ownership; it does not undeploy version.dll. Remove or disable the runtime through Vortex as well.

If a lifecycle operation fails or the game was open, close it and use the notification's Retry action. Data-only pending records can recover after the deployed package has been purged. No cached native helper is required. Only an exact checksum-owned old cached helper is unlinked during migration; unknown files are preserved.

## Manual installation

For an unmodified, closed game, extract the ZIP beside The Slormancer.exe and data.win, preserving folders. The proxy itself starts without a setup EXE. To verify hashes, create a setup receipt or migrate a legacy installation, use an existing Node.js 22+ installation:

    node tools/slorm-loader-setup.js install "C:\Games\The Slormancer"
    node tools/slorm-loader-setup.js check "C:\Games\The Slormancer"
    node tools/slorm-loader-setup.js restore "C:\Games\The Slormancer"

The folder argument defaults to the parent of tools. Keep all three JavaScript files together. Do not combine manual and Vortex ownership of the same files. Manual removal requires removing the runtime's proxy/DLL/sidecar files after restoration; preserve other mods, saves and .slorm-loader backups. Do not use these commands while the game is running.

Native DLL/ASI files remain compiled code and may still be blocked by Windows. No application-control or antivirus setting is changed. See SECURITY.md and BOOTSTRAP.md.

## Retired JSON skill/passive mods

Disable Irremovable, Full Plate Armor and other old JSON patch mods before
upgrading. Loader 0.6.0 no longer applies them. Remaining folders report an
unsupported error; no files or saves are deleted. Keep Lua Runtime, Legendary
and Reaper APIs, Item Tool and existing native/Lua content. A separate future
Skill/Passive API is not included in this release.
