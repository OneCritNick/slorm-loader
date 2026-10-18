# Release notes

## 0.4.2 - documentation cleanup and fresh bundle

- Ships a fresh 0.4.2 build. The ZIP, Vortex/runtime manifest and runtime logs/status all report 0.4.2. Setup records its version from the same runtime manifest instead of a stale hard-coded value.
- Clarifies that the proxy bootstrap leaves the game executable unchanged during normal installation and removal. Restoring a verified legacy 0.3.x patch is a separate migration operation.
- Recommends Vortex extension 0.2.11 and distinguishes extension installation from runtime installation.
- Corrects security/test documentation, preserves manual setup instructions and records the successful prior 0.4.1 game test.
- Retains the 0.4.1 startup fix and existing mod compatibility. No new gameplay features or effects are introduced.

The prior 0.4.1 session showed YYToolkit initialized, seven installed native mods loaded, zero errors and no pending mods; the user confirmed modded items worked. The user also confirmed extension 0.2.11 preserved the loader's name and version. This evidence applies to that tested setup. Version 0.4.2 is newly compiled and still needs its own user game test. No live public-release purge/redeploy/vanilla-start cycle is claimed as completed.

## 0.4.1 - proxy startup ordering

Aurie startup passes now use an explicit startup phase. The optional InitializeASI callback waits outside DllMain until early hooks are ready, then lets GameMaker create the runner interface/window before late module initialization. This fixes the YYToolkit and dependent-mod initialization failures in 0.4.0. See [startup correction](PROXY-STARTUP-0.4.1.md).

## 0.4.0 - proxy bootstrap

Replaces the legacy executable-patching startup path with pinned Ultimate ASI Loader 9.0.0 as version.dll and Core as plugins/AurieCore.asi. Version 0.4.1 corrected its startup ordering. Previous release archives are preserved for recovery/reference.
