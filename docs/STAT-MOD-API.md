# JSON skill mods

Runtime 0.4.x retains JSON stat API compatibility version 0.3.10 for existing `slormancer-runtime` dependency ranges. Bootstrap package versions and stat API compatibility versions are tracked separately: status reports the package in `runtimeVersion` and the JSON API in `statApiVersion`.

Each stat mod is a `mod.json` using `schemaVersion: 1`. The editor schema is `src/mod.schema.json`; regression inputs are under `tests/fixtures/stat-manifests.json`. Gameplay mod packages and authoring tools are maintained separately from this runtime. Vortex deploys a manifest to `mods/Slormancer/<id>/mod.json`; manually installed manifests may use the same layout. Close the game before changing deployments.

Irremovable changes Knight Whirlwind's tenacity scaling from base 20 + 2 per rank to base 20 + 8 per rank: 28% at rank 1 and 100% at rank 10. Full Plate Armor changes the in-combat movement penalty from -35% to -10%, retaining its armor scaling.

| Field | Selected skill parameter | Game column |
| --- | --- | --- |
| `base` | Raw displayed-result base | 4 |
| `perRank` | Displayed-result increment per rank | 5 |
| `extraNumber` | Separate numeric skill parameter | 29 |
| `manaCost` | Raw resource cost; interpretation follows resource type | 11 |
| `manaCostPerRank` | Resource scaling | 12 |
| `cooldown` | Raw cooldown ticks | 20 |
| `precastTime` | Raw precast ticks | 22 |
| `castTime` | Raw cast ticks | 24 |
| `knockback` | Raw knockback parameter; physical unit unverified | 26 |

Targets identify `classId` (Knight 0, Huntress 1, Mage 2), internal `skillRef`, existing `stat`, `expectedBase` and `expectedPerRank`. Each patch has `expected` and replacement `value`. The runtime checks these guards against the expected 54-column layout before committing changes. A stat name identifies an existing component; it does not add a new stat.

Optional `target.valueIndex` selects a component from 0 to 15 for `base`/`perRank` only. Other fields require index 0. Numeric strings retain their storage type; packed components retain their unaffected text. Symbolic/compound values are unsupported. Costs, timing and knockback replacements must be nonnegative. All numbers must be finite and within the supported bounds. Timing previews use nominal 60 simulation ticks per second before game modifiers; raw values are not guaranteed to equal tooltip results.

`requires` uses mod IDs and version ranges. The runtime enforces deployed dependencies, duplicates, conflicts and cycles. Dependencies apply first. Two mods targeting the same skill row are blocked to avoid ambiguous guards; combine related edits in one mod. Each mod validates all patches before applying, rolls back its own committed fields on failure, propagates changes to later hero tables and restores conditionally if a character's tables become incompatible.

Manifest limits include 64 KiB, 32 patches, 32 requirements, 32 conflicts and 32 nesting levels; the runtime accepts up to 128 immediate mod folders. Empty directories are ignored. Linked directories/manifests are rejected. The schema is an editor aid; native/extension semantic validation also checks duplicates and guard consistency.

The menu count includes applied JSON mods and confirmed initialized gameplay DLLs, excluding frameworks. `.slorm-loader/mod-status.json` report schema version 2 includes separate JSON/native counts, pending startup state and failure reasons. The report describes the last recorded process, not the current Vortex selection before relaunch. Optional diagnostics provide table readbacks and observed game-script returns; neither alone proves every combat effect.

The C++ helper API is compiled into the runtime, not a stable exported ABI for third-party DLLs. Complex native mods use Aurie/YYToolkit directly.
