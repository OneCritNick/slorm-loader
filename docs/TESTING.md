# Runtime tests and validation

The repository tests the runtime and its package. Extension/creator tests and installable gameplay mods are separate. `tests/fixtures/stat-manifests.json` contains regression inputs and is not deployed as gameplay content.

## Isolated checks

The root build command runs 130 native API checks for metadata parsing, guards, dependencies, rollback, character switching, status and current/stale log handling. It checks source-built original Core, five candidate startups and a copied missing-manifest negative DLL that must fail with Windows error 182.

Five proxy fixtures cover a synthetic Core, real Core, early/nested callback ordering through InitializeASI, a directly loaded Core with a running main thread, and an absent Core. They compare forwarded Windows version APIs with System32, verify callback order and check that surrogate executables remain byte-identical.

Setup tests exercise proxy install, check, repeated setup, receipt removal and status summaries using a benign x64 executable. Package tests unpack the actual ZIP, verify source/component hashes and official YYToolkit, repeat setup operations and reject a corrupted Core before changing the fixture EXE. The source build still compiles AuriePatcher as a retained legacy reference; the proxy package does not deploy or invoke it for installation.

These checks never launch The Slormancer or use saves. The original Core smoke hosts have empty mod directories; proxy ordering tests load only synthetic test modules. Test outputs remain under ignored `build/`. Disposable hardlink checks need normal NTFS access to their fixture directory, without changing Windows security settings.

## User validation of the released binaries

On 18 October 2026 the user installed 0.4.1 through Vortex and launched the game. The matching session showed YYToolkit initialized, seven installed native mods loaded, zero errors, zero pending mods and a complete scan. The user confirmed modded items worked. The user subsequently confirmed extension 0.2.11 retained the loader's name and version.

One YYToolkit instruction-scanning warning remained; runner discovery and initialization then succeeded. This is evidence of successful loading on that setup, not exhaustive testing of every API effect, mod combination or Windows trust policy.

Version 0.4.2 is a fresh build and needs its own user game test. The successful 0.4.1 session is prior compatibility evidence, not a live-game test of the new binaries. A final live purge/redeploy/vanilla-start cycle for the public upload has not been claimed as completed; the corresponding offline Vortex lifecycle tests passed.

## CI and independently built packages

GitHub Actions runs the source build/tests on Windows 2022 and uploads a development ZIP with toolchain evidence. Public main-branch builds can generate attestations; private builds skip that job. CI artifact availability is not a live-game acceptance result or proof that an independently built binary equals a historical release.
