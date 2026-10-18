# Runtime behavior and security reports

Slorm Loader loads native game modifications. This document describes the reviewed implementation; it is not a certification of every dependency or mod.

## Installation and removal

Current 0.4.x packages use an unchanged, SHA-256-pinned Ultimate ASI Loader 9.0.0 as `version.dll`, with Aurie Core loaded from `plugins/AurieCore.asi`. Normal proxy install/check/removal leaves `The Slormancer.exe` unchanged. Setup verifies package hashes, checks ownership/path constraints, and records `mode=version-proxy` under `.slorm-loader/state`. It refuses setup changes while the game is running. The current runtime ZIP does not deploy AuriePatcher or `mods/Native/AurieCore.dll`.

Older 0.3.x packages patched the executable. Migration can restore a verified, owned legacy patch from its saved original before configuring the proxy. That restoration replaces the executable with its verified original; it is distinct from normal proxy operation. Setup rejects unowned patches, damaged backups and a duplicate legacy Core. Keep the old `.slorm-loader/state` until migration has completed.

Vortex owns deployment/removal of proxy files. Its extension retains a checksum-verified recovery helper and pending action under `.slorm-loader` for interrupted lifecycle operations. Event errors cannot veto every Vortex operation; external deletion or damaged recovery files can require manual recovery. See [installation](docs/INSTALL.md) and [bootstrap details](docs/BOOTSTRAP.md).

Hardlinks are supported. Symlinks and junctions in checked game/mod paths are rejected. Supplied hashes detect changes relative to the package manifest; they are not cryptographic publisher signatures.

## Game process and application control

Aurie loads native modules and installs hooks. YYToolkit inspects GameMaker internals and exposes game data/functions. Slorm Loader registers callbacks, changes selected skill fields in memory, invalidates affected caches, draws the mod count and writes local logs/status under `.slorm-loader`.

JSON patches use expected-value guards, dependency/conflict checks and conditional rollback. Native DLLs are not sandboxed: they run with the game's permissions. Renaming Core to `.asi` does not sign it or exempt it from Smart App Control. Application-control acceptance was observed on the development machine; other systems may make different trust decisions. This runtime does not disable SAC or change antivirus settings.

The loader/setup does not intentionally edit saves or `data.win`, install services, or send telemetry. Separate gameplay mods can have their own behavior. Logs may contain absolute paths and diagnostic game values; redact personal paths before sharing.

Build scripts can download fixed, hash-checked dependency releases and run isolated test executables. CI uses GitHub artifact upload/provenance services. Those development operations are separate from installed runtime behavior.

## Reporting

Report potential vulnerabilities privately through the repository's security reporting option, if available, or the maintainer's Nexus Mods profile. Do not publish credentials, private paths or save data. For ordinary loading failures, provide game/runtime/extension versions, reproduction steps, fresh `aurie.log` and relevant `.slorm-loader/logs` files with personal information redacted.
