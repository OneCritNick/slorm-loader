# Runtime tests

This repository tests the runtime and its package. Extension/creator tests and installable gameplay examples are outside this repository. tests/fixtures/stat-manifests.json holds regression inputs for parser/dependency/patch behavior; the packager never deploys those as gameplay mods.

The root build command runs:

- 130 native API checks with simulated rows, including manifest parsing, guards, dependencies, rollback, character switching, native status and stale/current log handling.
- Source-built original Core and five modified Core startups in isolated hidden hosts, including the owned console marker.
- A copied missing-manifest negative DLL that must reproduce Windows error 182.
- Real setup/Patcher install, repeated setup, exact restore and status-report checks using a benign x64 executable.
- Actual ZIP validation, all packaged source hashes and setup transaction checks from the freshly unpacked package.

These tests do not launch The Slormancer or read saves. The bootstrap test uses an executable compiled for the fixture. The Core hosts load only the Core under test and have empty mod directories. Source candidates and compile checks do not establish live gameplay correctness.

Fresh/stale Aurie log fixtures use explicit timestamps around process creation to avoid relying on filesystem timestamp precision. Fixture directories and build outputs remain under ignored build. Creating disposable hardlinks requires normal filesystem access to that directory, without administrator settings changes.

GitHub Actions executes the same one-command runtime package build on Windows 2022, records the actual MSVC/SDK versions and uploads the complete ZIP. In a private repository the public-attestation job is skipped. Consult the individual run results before using its development package.
