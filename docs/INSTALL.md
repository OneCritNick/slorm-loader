# Install Slorm Loader

The package supports The Slormancer on Windows x64 and includes Aurie 2.0.2 with console fix revision 2, official YYToolkit 5.0.0c, SlormLoader.dll and setup/recovery tools.

## Vortex

Install the separate The Slormancer extension from https://www.nexusmods.com/site/mods/2455 and restart Vortex. Manage the game, choose Hardlink Deployment with staging on the same NTFS volume as the game, install this runtime ZIP through Mods and deploy with the game closed. Normal deployment configures the bootstrap. Normal purge/removal restores the original EXE. The Configure, Check and Restore original game EXE tools remain available in Vortex. The extension is maintained separately from this repository.

## Manual

Close the game. Extract the runtime ZIP into the folder containing The Slormancer.exe and data.win, preserving the directory layout. Run SlormLoaderSetup.exe to configure it. Keep .slorm-loader/state and its original EXE backup. Before removing a manual installation, restore using Vortex's Restore original game EXE tool or run SlormLoaderSetup.exe restore from the game folder.

Native mods belong under mods/Aurie. JSON stat mods belong in mods/Slormancer/<id>/mod.json. The runtime contains no enabled gameplay mod, so zero loaded gameplay mods is normal until you add one. Close the game before deployment/profile changes. Symbolic links and linked game/mod directories are unsupported; ordinary files and hardlink deployment are supported.

The loader modifies the game EXE to start Aurie, then changes selected skill metadata in memory. It does not back up saves. See BOOTSTRAP.md in the source docs for the exact startup/backup/restoration process.

The ZIP includes source, license notices, source checksums and build-info.json. Development builds must pass live-game acceptance before a release is promoted. Runtime logs/status and setup.log are under .slorm-loader; review personal paths before sharing them.
