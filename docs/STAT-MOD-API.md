# Retired JSON skill/passive patches

Slorm Loader 0.6.0 removes this API, its field registry, patch engine, cache
invalidation and skill diagnostics. The Loader only manages mod startup/status.
Disable old JSON mods before upgrading. Leftovers under mods/Slormancer are
reported as unsupported without parsing or applying patch data.

No replacement Skill/Passive API is included yet. A future separate API can
use native code and shared Lua providers, preserving Loader's narrow role.
Existing native and Lua Legendary/Reaper mods remain supported.
