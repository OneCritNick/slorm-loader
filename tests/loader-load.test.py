"""Isolated DLL loading/export check. Never invokes game initialization."""
import ctypes
import sys
from pathlib import Path

library = ctypes.CDLL(str(Path(sys.argv[1]).resolve()))
for name in ['ModuleInitialize', 'ModuleUnload', '__AurieFrameworkInit', '__AurieIsDebugBuild']:
    getattr(library, name)
print('PASS Loader DLL loads and exports entry points; game initialization not called')
