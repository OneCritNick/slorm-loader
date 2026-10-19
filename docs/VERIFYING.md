# Verification limits

Loader 0.6.1 removes gameplay patching. Its Loader DLL is newly compiled; the
published bundle uses the exact framework Core, YYToolkit and proxy bytes from
the validated 0.5.0 distribution. build-info records this distinction. Normal
source builds can rebuild Core instead; that output is identified separately.

Offline tests do not establish gameplay acceptance or guarantee Windows App
Control trust. Installation and gameplay testing are performed by the user.
No security setting is changed. Player ZIPs contain no compiled EXE; native DLLs
and ASIs remain compiled code. Historical evidence describes its own version.
