# Proxy startup correction 0.4.1

The user's 18 October 2026 session (PID 19288) launched successfully through runtime 0.4.0, but aurie.log contained no early module mappings. After the game window appeared, YYToolkit reported "Failed to await runner interface creation!" and failed initialization. SlormLoader, Legendary Effects, Item Tool and other YYToolkit consumers failed with dependency-not-resolved errors. Lua/content modules remained waiting for the Legendary API. The existing mod-status.json belonged to an older session and was not evidence of success.

Aurie's startup loader inferred startup/runtime phase from whether the entrypoint thread was suspended. That assumption held for the old patched bootstrap but not for the proxy loader. Query failure silently skipped early mappings; a non-suspended result initialized modules before their ModuleEntrypoint callbacks. An isolated priority-module probe reproduced preinitialize, initialize, entrypoint instead of the required order.

The fix maps both early passes explicitly as startup loads and defers nested initialization during startup. InitializeASI holds the upstream proxy's plugin callback until entrypoint and preinitialize hooks have finished, outside DllMain. It releases before waiting for the game window so GameMaker can create the runner interface that YYToolkit needs. Duplicate modules found in both early passes are skipped.

Tests use disposable surrogate executables, never The Slormancer.exe. They validate early callback ordering, nested dependencies, forwarding to Windows version APIs, complete Core startup, missing Core handling and unchanged surrogate EXE bytes. Core retains all upstream exports and adds only InitializeASI. The subsequent user Vortex test succeeded: YYToolkit initialized and seven native mods loaded with zero errors or pending mods; modded items worked.

Delivery replaces only the runtime ZIP. Vortex extension 0.2.10 and existing API/content packages remain compatible. The upstream version.dll is unchanged. No installed game files, Vortex deployments or security settings are modified by preparing this fix.
