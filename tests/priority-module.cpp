#include <Aurie/shared.hpp>
#include <fstream>
using namespace Aurie;
static void Record(const char* phase) {
    wchar_t exe[32768];
    GetModuleFileNameW(nullptr, exe, 32768);
    std::ofstream(fs::path(exe).parent_path() / "priority-phases.txt", std::ios::app) << phase << '\n';
}
#ifdef NESTED_PROBE
EXPORTED AurieStatus ModulePreinitialize(AurieModule*, const fs::path&) { Record("nested-preinitialize"); return AURIE_SUCCESS; }
EXPORTED AurieStatus ModuleInitialize(AurieModule*, const fs::path&) { Record("nested-initialize"); return AURIE_SUCCESS; }
#else
EXPORTED AurieStatus ModuleEntrypoint(AurieModule*, const fs::path&) { Record("entrypoint"); return AURIE_SUCCESS; }
EXPORTED AurieStatus ModulePreinitialize(AurieModule*, const fs::path& path) {
    Record("preinitialize");
    AurieModule* nested = nullptr;
    return MdMapImage(path.parent_path() / "NestedProbe.bin", nested);
}
EXPORTED AurieStatus ModuleInitialize(AurieModule*, const fs::path&) { Record("initialize"); return AURIE_SUCCESS; }
#endif
