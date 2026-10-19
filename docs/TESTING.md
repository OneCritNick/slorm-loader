# Test scope — Loader 0.6.1

The native status suite checks current-session Aurie log evidence, rejected and
uninitialized DLLs, architecture/signatures, basename ambiguity, module aliases,
framework exclusion, incomplete scans, shared log access and atomic status files.
Old JSON folders are unsupported, never applied, and cannot stop native status.
The architecture test verifies that skill writes/hooks, field registries and
stat-patch parsers are absent. A separate host loads SlormLoader.dll and checks
exports without invoking game initialization.

Core/proxy suites use isolated surrogate programs. Bootstrap/package tests use
inert PE buffers, actual ZIP hashes and disposable filesystem links. They cover
install/repeat/check/restore, verified legacy recovery, purge, external changes,
corrupt backups and tamper rejection. No game, saves or Vortex deployment is used.

The 0.6.1 delivery keeps framework bytes from the validated 0.5.0 archive and
rebuilds only the packaged Loader DLL. Native C++/Lua gameplay APIs are separate
and unchanged. User installation/gameplay testing is still required.
