#pragma once
#include "SlormModStatus.hpp"
#include "SlormModFolders.hpp"
#include <functional>
#include <windows.h>
namespace Slorm {
namespace fs=std::filesystem;
class LoaderStatus {
    fs::path game;
    std::function<void(const std::string&)> log;
    Json retired=Json::array(),native=Json::array();
    bool complete=false,started=false,dirty=false;
    uint64_t lastWrite=0;
    static bool Linked(const fs::path& path){auto a=GetFileAttributesW(path.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT);}
    void Report() noexcept {
        dirty=true;lastWrite=GetTickCount64();
        try {
            const auto folder=game/".slorm-loader";fs::create_directories(folder);
            const auto temp=folder/"mod-status.json.tmp",target=folder/"mod-status.json";
            {std::ofstream out(temp,std::ios::binary);out<<ModStatusReport(retired,native,GetCurrentProcessId(),complete,started).dump(2)<<'\n';if(!out)throw std::runtime_error("Unable to write status");}
            if(!MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Unable to commit status");
            dirty=false;
        }catch(const std::exception& e){log(std::string("MOD_STATUS_WRITE_FAILED ")+e.what());}
    }
    void Reject(const std::string& id,const std::string& reason){retired.push_back({{"id",id},{"name",id},{"status","unsupported"},{"reason",reason}});log("MOD_UNSUPPORTED id="+id+" reason="+reason);}
public:
    LoaderStatus(fs::path path,std::function<void(const std::string&)> logger):game(std::move(path)),log(std::move(logger)){}
    void Discover(){
        const auto root=game/"mods/Slormancer";
        try {
            if(fs::exists(root)){
                if(Linked(game/"mods")||Linked(root))throw std::runtime_error("Legacy mod directory is linked; no contents inspected");
                unsigned count=0;
                for(const auto& entry:fs::directory_iterator(root)){
                    if(!entry.is_directory())continue;
                    if(++count>128)throw std::runtime_error("Legacy mod directory exceeds 128 folders");
                    if(!Linked(entry.path())&&InactiveModFolder(entry.path()))continue;
                    Reject(PathText(entry.path().filename()),"JSON skill/passive patches were removed from Slorm Loader 0.6.0. Disable this old JSON mod in Vortex. A separate Skill/Passive API is planned; it is not included in this release.");
                }
            }
        }catch(const std::exception& e){Reject("legacy-json-discovery",e.what());}
        Report();
    }
    void Tick(){if(dirty&&GetTickCount64()-lastWrite>=1000)Report();}
    void UpdateNativeStatus(const NativeSnapshot& snapshot,bool startupFinished){
        const bool done=startupFinished&&snapshot.complete;
        if(native==snapshot.items&&complete==done&&started==startupFinished)return;
        for(const auto& item:snapshot.items)if(std::find(native.begin(),native.end(),item)==native.end())log("NATIVE_MOD_STATUS "+item.dump());
        native=snapshot.items;complete=done;started=startupFinished;Report();
    }
    int TotalActiveCount()const{return std::count_if(native.begin(),native.end(),[](const Json& j){return j.value("status","")=="loaded";});}
    int NativeActiveCount()const{return TotalActiveCount();}
    int ErrorCount()const{return retired.size()+std::count_if(native.begin(),native.end(),[](const Json& j){return StatusError(j);});}
    int PendingCount()const{return std::count_if(native.begin(),native.end(),[](const Json& j){return j.value("status","")=="pending";});}
    bool NativeScanComplete()const{return complete;}
    bool NativeStartupFinished()const{return started;}
};
}
