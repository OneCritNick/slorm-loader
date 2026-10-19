# Build Loader 0.6.1

Use Windows x64, PowerShell, MSVC with C++20, Windows SDK 10.0.22621.0 and
Node.js 22+ and Python 3 for the isolated DLL-load test. Root build.ps1 builds Loader and both Core test variants, runs the
native status/architecture/DLL-load/Core/proxy/bootstrap suites, packages and
checks the actual ZIP. No game is launched and no deployment is changed.

-VCToolsPath, -WindowsSdkRoot, -SdkVersion and -YYToolkitPath select local tools
and the pinned official YYToolkit binary. Dependencies use fixed SHA-256 pins;
the optional source YYToolkit candidate is never packaged as the official DLL.

For a Loader-only release, -FrameworkArchive can supply the exact validated
Slorm-Loader-0.5.0.zip pinned in third_party/runtime-framework/provenance.json.
Only its Core ASI is reused; Loader is always newly compiled. Proxy and YYToolkit
already use their own unchanged official pins. Build-info identifies the Core
actually packaged separately from source-built Core test products.

ZIPs use unique build/packages/<id> directories and never overwrite a delivered
archive. No compiled EXE enters player downloads, including source/tools. Source
inventory and SDK checks are provenance checks, not Windows publisher signatures.

The static source inventory pins code, build/test scripts and dependencies.
Documentation, including README.md, can be edited without refreshing that
inventory. Every actual Full ZIP still records checksums of all bundled source
files, including documentation, and verifies them after packaging.

Each build produces Slorm-Loader-<version>.zip for Nexus players and
Slorm-Loader-<version>-Full.zip for GitHub. The Full bundle keeps all corresponding
source and build tools; the lean package copies an explicit player-file allowlist
from that same payload. Both preserve the same setup/recovery scripts and runtime
checksums. Tests unpack both ZIPs, compare every lean file byte for byte and run
installation/check/removal against disposable fixtures for each distribution.
build/latest-package.json records both archive paths and SHA-256 hashes.

With -FrameworkArchive, tests verify the exact dependency archive pin and do not repeat Core/proxy startup tests for those unchanged shipped bytes. The Loader is newly built and tested. -PythonPath selects the developer Python interpreter; no Python binary is bundled.
