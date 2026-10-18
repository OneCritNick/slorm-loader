# Credits and license scope

| Component | Source and license |
| --- | --- |
| Project native loader, stat API and C++ tests | AGPL-3.0, root `LICENSE` |
| Project C# setup, field generator, PowerShell scripts, documentation and test fixtures | MIT, `licenses/PROJECT-MIT.txt` |
| Aurie Core and Patcher | [Aurie 2.0.2](https://github.com/AurieFramework/Aurie/tree/v2.0.2), AGPL-3.0; original and modified trees retain `LICENSE` |
| YYToolkit | [5.0.0c release](https://github.com/AurieFramework/YYToolkit/releases/tag/v5.0.0c), AGPL-3.0; pinned source commit `d5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54` |
| nlohmann/json 3.12.0 | MIT, `src/vendor/nlohmann/LICENSE.MIT` and original header notices |
| spdlog 1.15.1 and fmt | MIT, notices in Aurie's vendored headers and `licenses/` |
| SafetyHook, embedded upstream revision not independently identified | Boost Software License 1.0, `licenses/SafetyHook-LICENSE.txt` |
| Zydis/Zycore | MIT, original amalgamated source notices and `licenses/Zydis-Zycore-notices.txt` |
| Ultimate ASI Loader 9.0.0 | [Tagged upstream release](https://github.com/ThirteenAG/Ultimate-ASI-Loader/tree/v9.0.0), MIT; `third_party/ual/LICENSE.txt` and pinned hashes in `third_party/ual/provenance.json`. Distributed unchanged as version.dll. |

The standalone SafetyHook notice was retrieved from upstream tag v0.6.5; that identifies the notice source, not the embedded code version. `licenses/notice-provenance.json` records notice URLs and hashes. Complete vendored source retains additional original notices.

The Aurie console fix by nick, dated 14 October 2026, changes four files relative to 2.0.2:

- `Aurie/source/framework/Debugging/debug.cpp`
- added `Aurie/source/framework/Debugging/owned_console_sink.hpp`
- `Aurie/source/include/spdlog/sinks/wincolor_sink-inl.h`
- `Aurie/AurieCore.rc`

The logger owns a separate `CONOUT$` handle, synchronizes its release with writes and closes it before detachment. The version resource identifies revision 2. The build embeds a Common Controls 6 activation manifest and regression tests actual DLL startup. Dated modification notices are present in the changed files.

The proxy startup fix by nick, dated 18 October 2026, changes three additional files: `Aurie/source/AurieMain.cpp`, `Aurie/source/framework/Module Manager/module.cpp` and `module.hpp`. It adds the optional `InitializeASI` entrypoint, waits for early startup outside DllMain, and uses explicit startup phases instead of process suspension to order module callbacks. All upstream Core exports are retained. The original dependency tree is kept separately for comparison.

The Slormancer game belongs to its respective owners. This runtime repository/package includes no game executable, game data, saves, promotional artwork or community mod binaries.
