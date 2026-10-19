#pragma once
#include <filesystem>

namespace Slorm {
// Vortex removes deployed files before cleaning up directories during purge.
// Only empty directories or its marker alone represent inactive deployment residue.
inline bool InactiveModFolder(const std::filesystem::path& folder) {
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().filename() != "__folder_managed_by_vortex" ||
            entry.symlink_status().type() != std::filesystem::file_type::regular) return false;
    }
    return true;
}
}
