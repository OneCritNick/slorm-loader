# Slorm Loader 0.6.1

- Added a lean Nexus player ZIP without bundled developer sources, tests or build
  diagnostics. Runtime DLLs, manual setup/recovery scripts, instructions, credits
  and license notices are retained.
- Complete source, dependency trees and build tools remain in the matching
  GitHub Full bundle and repository. Both ZIPs use the same version and identical
  runtime files; install only one.
- Both distributions are verified from their actual ZIPs, including fixture
  installation, repeated setup, removal and tamper rejection.
- No mod-loading behavior, recovery logic, gameplay APIs or save handling changed.
  Vortex Extension 0.5.0 remains compatible.

## Slorm Loader 0.6.0

- Removed built-in JSON skill/passive patching, skill-table/cache writes,
  field registries and gameplay diagnostic hooks.
- Loader now handles native mod startup/discovery/status and menu reporting.
- Retired JSON mods are reported as unsupported without applying their data;
  native loading continues. Disable Irremovable, Full Plate Armor and other old
  JSON patch mods before upgrading.
- Use Vortex Extension 0.5.0, which rejects retired JSON packages and retains
  generic native DLL/Lua installation.
- Native C++ and Lua Legendary/Reaper mods and shared Lua Runtime are unchanged.
- Bootstrap/legacy recovery code is unchanged. Player ZIP remains EXE-free.

Skill/Passive functionality may return through a separate API later; that API
is not included in this release. Replace only Loader and extension packages.
