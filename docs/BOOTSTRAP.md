# EXE-free bootstrap and recovery

Slorm Loader 0.5.0 retains the 0.4.2 pinned proxy startup architecture. version.dll is Ultimate ASI Loader 9.0.0; it loads plugins/AurieCore.asi. Aurie loads the official YYToolkit 5.0.0c DLL and SlormLoader.dll. No separate custom EXE runs during startup or setup.

Vortex Extension 0.3.0 includes bootstrap.js and strict-json.js. The optional manual tools/slorm-loader-setup.js uses the same files under an existing Node.js 22+ installation. Vortex uses its own extension code, not a script deployed by a mod. Neither path downloads or launches a native setup program. Windows' existing System32/tasklist.exe is queried solely to refuse operations while the game is running; it is not bundled.

Install verifies exactly six component pins from slorm-loader-checksums.json, validates runtime metadata and the x64 game PE, rejects linked paths/duplicate Core, and atomically records a version-proxy receipt. Repeated install is idempotent. Check verifies installation and labels mod counts as the last recorded session. Normal install/check/restore never rewrites the game EXE.

## Legacy migration

Legacy executable-patching receipts carry original and patched SHA-256 hashes. Restoration verifies the saved original.exe, rejects an original containing .aurie, and accepts only the recorded original/patched current executable. A version-2 pending transaction records before/after hashes and the exact receipt hash before replacement. Recovery is allowed only when the backup, receipt and current file match those owned values. Unexpected external changes stop restoration and preserve evidence. Original backups remain retained.

Old pending transactions lack a final hash. They can recover only when the current executable matches the transaction backup or a verified prior/live ownership receipt. An ambiguous half-patched executable is not overwritten automatically.

Proxy removal clears its receipt without rewriting an unpatched EXE, including an EXE subsequently updated by Steam. It still refuses an unowned .aurie patch. Vortex handles file deployment and removal. Restoration works after package files/checksums are gone.

## Lifecycle

The extension serializes will/did deploy, purge and removal hooks; game activation uses Vortex's ordinary event handler. Hardlink identity is a fallback when deployment metadata is absent. Multiple enabled runtimes are refused. A data-only automation record persists pending operations; no recovery EXE is cached. The previous schema's exact checksum-owned cached helper may be unlinked, never executed. Unrecognized cached files are preserved.

Vortex event failures cannot veto every file operation. Close the game and Retry after an error; externally damaged state may require investigation. Symlinks/junctions are refused, metadata writes replace files atomically rather than modifying hardlink aliases, and unknown executable hashes are not restored. Repository/package hashes are integrity checks, not publisher signatures.
