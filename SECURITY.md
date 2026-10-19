# Runtime behavior and security reports

Slorm Loader loads native game modifications. This describes implementation behavior, not certification of every dependency or mod.

## Setup and ownership

The 0.6.0 player archive contains no compiled EXE. Vortex Extension 0.5.0 runs setup/check/restore as its own JavaScript. Optional manual JavaScript requires an existing Node.js 22+ installation. The only setup process query uses Windows' own System32/tasklist.exe. No PowerShell setup, bundled Node binary, native helper or legacy Patcher is launched.

Startup retains the SHA-256-pinned version.dll proxy, Core ASI and official YYToolkit. Normal proxy setup/removal preserves the game EXE. Migration from 0.3.x can replace only an owned legacy executable with its verified saved original; unknown changes, damaged backups and ambiguous old transactions are refused. Hardlinks are supported; checked symbolic links/junctions are rejected. Atomic metadata replacement preserves external hardlink aliases.

Vortex owns deployed files. Data-only pending records remain under .slorm-loader for interrupted operations. Exact old cached-helper ownership permits unlinking that file, never executing it. Recovery does not require package files after purge. Event failures cannot veto every Vortex operation; damaged external state can require manual help. See docs/INSTALL.md and docs/BOOTSTRAP.md.

## Native code and Windows

DLL/ASI files remain compiled native code. Aurie installs hooks; YYToolkit inspects the runner; the Loader manages guarded patches, callbacks and local diagnostics. Renaming a DLL to .asi does not sign or exempt it from Smart App Control. Removing EXEs does not guarantee Windows or Nexus acceptance. No application-control or antivirus setting is changed.

Native gameplay DLLs run with the game's permissions and are not sandboxed. The Loader/setup does not intentionally edit saves or data.win, install services or send telemetry. Other mods have their own behavior. Logs can contain personal paths; redact before sharing.

Developer scripts can download fixed hash-checked dependencies and compile/run isolated fixture EXEs. They remain outside the player ZIP. CI upload/provenance services are development behavior.

Report potential vulnerabilities privately through the repository's reporting option or the maintainer's Nexus profile. For ordinary loading issues, supply versions, reproduction steps, fresh aurie.log and relevant .slorm-loader logs with personal paths redacted. Never share credentials or private save data publicly.
