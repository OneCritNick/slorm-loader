# Runtime behavior and security reports

Slorm Loader is a native game modification. This document describes the intended behavior for review; it is not a certification that every dependency or binary is safe.

## Installation and removal

`src/SlormLoaderSetup.cs` invokes AuriePatcher to add an Aurie bootstrap section to the selected game's Windows x64 EXE. The bootstrap names an absolute path to `mods/Native/AurieCore.dll`. Setup keeps a hash-checked original EXE and transaction records under the game's `.slorm-loader/state`. It refuses to overwrite an externally changed executable during restoration and refuses setup while a process named The Slormancer is running.

Normal Vortex deployment configures the bootstrap. Normal purge/removal restores the original EXE before deployed files disappear. Event errors cannot veto every Vortex operation. The extension retains a checksum-verified recovery helper and pending action under `.slorm-loader`; bypassed events, external deletion and corrupted recovery files can still require manual recovery. That integration lives in the separately maintained Vortex extension; see docs/INSTALL.md and docs/BOOTSTRAP.md.

Hardlinks are supported; symbolic links and junctions in checked game/mod paths are rejected. Hash checks detect changes relative to the supplied manifest. A manifest shipped alongside files is not a cryptographic publisher signature.

## Game process

Aurie loads native modules into the game and supplies hooks and framework services. YYToolkit inspects GameMaker internals and exposes game data/functions. Slorm Loader registers callbacks, changes selected skill fields in memory, invalidates affected caches, draws the mod count, and writes logs/status reports inside `.slorm-loader`.

JSON patches use expected-value guards, dependency/conflict checks and conditional rollback. They do not give an untrusted native DLL a sandbox. Native mods can run arbitrary code with the game process's permissions; install them only from authors you trust.

The loader/setup code does not intentionally access save contents, modify `data.win`, install services, change antivirus settings or send telemetry. Runtime log messages may contain absolute paths and diagnostic game values. Review and redact logs before sharing.

The build scripts compile source and run isolated test executables. The CI workflow also uses GitHub's artifact upload and provenance services. These development activities are separate from runtime networking behavior.

## Reporting

For a potential vulnerability, use this repository's private security reporting option if available. Otherwise contact the maintainer through their Nexus Mods profile first; do not publish exploit details, credentials, personal paths or save data in an issue. Ordinary loading failures can be reported in Issues with redacted logs, game/runtime versions and reproduction steps.
