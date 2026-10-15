#pragma once
#include "SlormYYTKBackend.hpp"
#include "SlormModFolders.hpp"
#include "SlormModStatus.hpp"
#include <functional>
#include <memory>
#include <windows.h>

namespace Slorm {
class StatRuntime {
    YYTKBackend backend;
    std::unique_ptr<StatModSession<YYTKBackend>> session;
    std::filesystem::path game;
    std::function<void(const std::string&)> log;
    std::vector<Mod> mods;
    std::vector<size_t> order;
    Json invalid = Json::array();
    Json nativeItems = Json::array();
    bool nativeScanComplete = false;
    bool nativeStartupFinished = false;
    bool reportDirty = false;
    uint64_t lastReportAttempt = 0;
    static bool Linked(const std::filesystem::path& path) {auto a = GetFileAttributesW(path.c_str()); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT);}
    void Report() noexcept {
        reportDirty=true;lastReportAttempt=GetTickCount64();
        try {
            Json items = invalid;
            for (const auto& m : mods) items.push_back({{"id",m.id},{"name",m.name},{"version",m.version},{"status",m.status},{"reason",m.reason}});
            Json report=ModStatusReport(items,nativeItems,GetCurrentProcessId(),nativeScanComplete,nativeStartupFinished);
            auto folder = game / ".slorm-loader"; std::filesystem::create_directories(folder);
            auto temp = folder / "mod-status.json.tmp", file = folder / "mod-status.json";
            {std::ofstream output(temp,std::ios::binary); output << report.dump(2) << '\n'; if (!output) throw std::runtime_error("Unable to write mod status");}
            if (!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Unable to commit mod status");
            reportDirty=false;
        } catch (const std::exception& e) {log(std::string("MOD_STATUS_WRITE_FAILED ") + e.what());}
    }
public:
    StatRuntime(YYTK::YYTKInterface* api, std::filesystem::path game, std::function<void(const std::string&)> log) : backend(api),game(std::move(game)),log(std::move(log)) {}
    void Discover() {
        auto root = game / "mods" / "Slormancer";
        if (!std::filesystem::exists(root)) {Report(); return;}
        if (Linked(game / "mods") || Linked(root)) throw std::runtime_error("Stat-mod folder must not be a directory junction or symlink");
        std::vector<std::filesystem::path> folders;
        for (const auto& entry : std::filesystem::directory_iterator(root)) if (entry.is_directory()) {
            if (!Linked(entry.path()) && InactiveStatModFolder(entry.path())) {
                log("MOD_FOLDER_IGNORED folder=" + entry.path().filename().string() + " reason=empty_or_vortex_marker_only");
                continue;
            }
            folders.push_back(entry.path());
        }
        std::sort(folders.begin(),folders.end());
        if (folders.size() > 128) throw std::runtime_error("At most 128 stat-mod folders are supported");
        for (const auto& folder : folders) {
            auto file = folder / "mod.json";
            try {
                const auto id = folder.filename().string(); CheckId(id);
                if (Linked(folder) || Linked(file)) throw std::runtime_error("Linked mod folders/manifests are unsupported");
                if (!std::filesystem::is_regular_file(file) || std::filesystem::file_size(file) > 65536) throw std::runtime_error("Missing manifest or manifest exceeds 64 KiB");
                std::ifstream input(file,std::ios::binary); std::string text((std::istreambuf_iterator<char>(input)),{});
                auto mod = ParseManifest(ParseJson(text));
                if (mod.id != id) throw std::runtime_error("Mod folder name must match manifest ID");
                log("MOD_DISCOVERED id=" + mod.id + " name=" + mod.name + " version=" + mod.version);
                mods.push_back(std::move(mod));
            } catch (const std::exception& e) {
                const auto name = folder.filename().string(); invalid.push_back({{"id",name},{"name",name},{"version",""},{"status","invalid"},{"reason",e.what()}});
                log("MOD_INVALID folder=" + name + " reason=" + e.what());
            }
        }
        order = Resolve(mods);
        session = std::make_unique<StatModSession<YYTKBackend>>(backend,mods,order,log);
        for (const auto& m : mods) if (m.status == "blocked") log("MOD_BLOCKED id=" + m.id + " name=" + m.name + " reason=" + m.reason);
        Report();
    }
    void Tick() {
        const auto now=GetTickCount64();
        if (session && session->Tick(now)) Report();
        else if(reportDirty&&now-lastReportAttempt>=1000)Report();
    }
    int ActiveCount() const {return static_cast<int>(std::count_if(mods.begin(),mods.end(),[](const Mod& m){return m.status == "applied";}));}
    int NativeActiveCount() const {return static_cast<int>(std::count_if(nativeItems.begin(),nativeItems.end(),[](const Json& item){return item.value("status","")=="loaded";}));}
    int TotalActiveCount() const {return ActiveCount()+NativeActiveCount();}
    bool NativeScanComplete() const {return nativeScanComplete;}
    bool NativeStartupFinished() const {return nativeStartupFinished;}
    int PendingCount() const {return static_cast<int>(std::count_if(nativeItems.begin(),nativeItems.end(),[](const Json& item){return item.value("status","")=="pending";}))+static_cast<int>(std::count_if(mods.begin(),mods.end(),[](const Mod& m){return m.status=="pending";}));}
    void UpdateNativeStatus(const NativeSnapshot& snapshot,bool startupFinished) {
        const bool complete=startupFinished&&snapshot.complete;
        if(nativeItems==snapshot.items&&nativeScanComplete==complete&&nativeStartupFinished==startupFinished)return;
        for(const auto& item:snapshot.items)if(std::find(nativeItems.begin(),nativeItems.end(),item)==nativeItems.end())log("NATIVE_MOD_STATUS "+item.dump());
        nativeItems=snapshot.items;nativeScanComplete=complete;nativeStartupFinished=startupFinished;Report();
    }
    const std::vector<Mod>& Mods() const {return mods;}
    int ErrorCount() const {return static_cast<int>(invalid.size() + std::count_if(mods.begin(),mods.end(),[](const Mod& m){return m.status == "blocked" || m.status == "failed";}) + std::count_if(nativeItems.begin(),nativeItems.end(),[](const Json& item){return StatusError(item);}));}
    void Unload() {if (session) session->RestoreAll(); log("STAT_API_UNLOAD retained field values conditionally restored");}
};
}
