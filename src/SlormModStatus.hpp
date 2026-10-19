#pragma once
#include "SlormLoaderMetadata.hpp"
#include <filesystem>
#include <sstream>

namespace Slorm {
inline std::string PathText(const std::filesystem::path& path) {
    const auto text=path.generic_u8string();return {reinterpret_cast<const char*>(text.data()),text.size()};
}
inline std::string AsciiLower(std::string text) {
    for(auto& c:text)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return text;
}
inline bool FrameworkDll(const std::filesystem::path& path) {
    const auto name=AsciiLower(PathText(path.filename()));
    return name=="slormloader.dll"||name=="yytoolkit.dll"||name=="auriecore.dll";
}
inline bool NativeCandidate(const std::filesystem::path& path) {
    return AsciiLower(PathText(path.extension()))==".dll"&&!FrameworkDll(path);
}
inline bool AurieStartupFinished(const std::string& text,bool currentSession) {
    if(!currentSession)return false;
    bool started=false;std::istringstream lines(text);std::string line;
    while(std::getline(lines,line)) {
        // A partial final line is not startup confirmation.
        if(lines.eof()&&(!text.empty()&&text.back()!='\n'))break;
        if(line.find("Aurie Core v2.0.2 loaded at ")!=std::string::npos)started=true;
        if(started&&line.find("[ArProcessAttach] Init done.")!=std::string::npos)return true;
    }
    return false;
}
inline std::string AurieFailureStatus(const std::string& text,const std::string& filename,bool uniqueName) {
    if(!uniqueName)return {}; // Aurie logs basenames; duplicate names cannot be attributed safely.
    std::istringstream lines(text);std::string line,status;
    while(std::getline(lines,line)) {
        if(lines.eof()&&(!text.empty()&&text.back()!='\n'))break;
        size_t offset=std::string::npos;
        const auto mapped="MdpMapImage("+filename+") returns ";
        const auto initialized="Module \""+filename+"\" failed ModuleInitialize with status ";
        auto p=line.find(mapped);if(p!=std::string::npos)offset=p+mapped.size();
        p=line.find(initialized);if(p!=std::string::npos)offset=p+initialized.size();
        if(offset==std::string::npos||line.compare(offset,6,"AURIE_")!=0)continue;
        const auto end=line.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_",offset);
        const auto candidate=line.substr(offset,end==std::string::npos?end:end-offset);
        if(candidate!="AURIE_SUCCESS"&&candidate!="AURIE_OBJECT_ALREADY_EXISTS")status=candidate;
    }
    return status;
}
struct NativeEvidence {
    bool registered=false,initialized=false,architectureRead=false;
    unsigned short architecture=0;
    bool frameworkInit=false,moduleInit=false,modulePreinit=false;
    std::string lookupStatus,inspectionStatus,originalFailure;
    uintptr_t moduleIdentity=0;
    std::filesystem::path registeredFilename;
};
inline Json NativeItem(const std::filesystem::path& relative,const std::string& status,const std::string& reason,const std::string& evidence,const std::string& aurieStatus={}) {
    const auto path=PathText(relative);
    return Json{{"id","native:"+AsciiLower(path)},{"name",PathText(relative.filename())},{"version",""},{"source","native"},{"path",path},{"status",status},{"reason",reason},{"evidence",evidence},{"aurieStatus",aurieStatus}};
}
inline Json NativeStatus(const std::filesystem::path& relative,const NativeEvidence& e) {
    if(e.registered&&e.initialized)return NativeItem(relative,"loaded","","aurie_module_state","AURIE_SUCCESS");
    if(e.registered)return NativeItem(relative,"not_initialized","Aurie registered this DLL, but did not complete its initialization. See aurie.log.","aurie_module_state",e.lookupStatus);
    if(relative.extension()!=std::filesystem::path(".dll"))return NativeItem(relative,"not_loaded","Aurie 2.0.2 automatically discovers only the lowercase .dll extension. Rename this file and restart the game.","file_extension",e.lookupStatus);
    if(!e.architectureRead)return NativeItem(relative,"inspection_failed","Could not read the DLL's executable architecture. See aurie.log.","aurie_file_inspection",e.inspectionStatus);
    if(e.architecture!=0x8664)return NativeItem(relative,"rejected","This DLL does not target the x64 architecture required by this game.","aurie_file_inspection","AURIE_INVALID_ARCH");
    if(!e.frameworkInit||(!e.moduleInit&&!e.modulePreinit))return NativeItem(relative,"rejected","Not an Aurie 2 module: requires __AurieFrameworkInit and ModuleInitialize or ModulePreinitialize exports.","aurie_file_inspection","AURIE_INVALID_SIGNATURE");
    if(!e.originalFailure.empty())return NativeItem(relative,"rejected","Aurie reported "+e.originalFailure+" while loading or initializing this DLL. See aurie.log.","aurie_log",e.originalFailure);
    return NativeItem(relative,"not_loaded","Aurie did not register this DLL. Its original loading error is unavailable; see aurie.log.","aurie_lookup",e.lookupStatus);
}
struct NativeSnapshot {Json items=Json::array();bool complete=true;};
inline void NativeScanIssue(NativeSnapshot& result,const std::string& reason) {
    result.complete=false;
    result.items.push_back(Json{{"id","native:scan"},{"name","Native mod scan"},{"version",""},{"source","loader"},{"status","inspection_failed"},{"reason",reason},{"evidence","filesystem_scan"}});
}
template<class Observer> NativeSnapshot ScanNativeMods(const std::filesystem::path& game,bool startupFinished,Observer observe,const std::string& pendingReason) {
    NativeSnapshot result;const auto root=game/"mods"/"Aurie";std::error_code error;
    const bool exists=std::filesystem::exists(root,error);
    if(error){NativeScanIssue(result,"Cannot inspect mods/Aurie: "+error.message());return result;}
    if(!exists)return result;
    std::vector<std::filesystem::path> paths;
    std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::none,error),end;
    if(error)NativeScanIssue(result,"Cannot scan mods/Aurie: "+error.message());
    while(!error&&it!=end) {
        const bool file=it->is_regular_file(error);
        if(error){NativeScanIssue(result,"Cannot inspect native entry: "+error.message());break;}
        if(file&&NativeCandidate(it->path())) {
            bool frameworkAlias=false;
            for(const auto& known:{game/"mods/Aurie/YYToolkit.dll",game/"mods/Aurie/SlormLoader.dll",game/"mods/Native/AurieCore.dll"}) {
                std::error_code aliasError;if(std::filesystem::equivalent(it->path(),known,aliasError)&&!aliasError){frameworkAlias=true;break;}
            }
            if(!frameworkAlias)paths.push_back(it->path());
        }
        it.increment(error);if(error)NativeScanIssue(result,"Native scan is incomplete: "+error.message());
    }
    std::sort(paths.begin(),paths.end());
    std::map<std::string,size_t> names;for(const auto& path:paths)++names[AsciiLower(PathText(path.filename()))];
    std::map<uintptr_t,std::string> observedModules;
    for(const auto& path:paths) {
        const auto relative=path.lexically_relative(game);
        if(!startupFinished){result.items.push_back(NativeItem(relative,"pending",pendingReason,"awaiting_aurie_startup"));continue;}
        try {
            const auto evidence=observe(path,names[AsciiLower(PathText(path.filename()))]==1);
            if(evidence.registered&&FrameworkDll(evidence.registeredFilename))continue;
            if(evidence.registered&&evidence.moduleIdentity) {
                const auto [previous,inserted]=observedModules.emplace(evidence.moduleIdentity,PathText(relative));
                if(!inserted){auto alias=NativeItem(relative,"alias","This path refers to the same Aurie module already counted at "+previous->second+".","aurie_module_identity",evidence.lookupStatus);alias["aliasOf"]=previous->second;result.items.push_back(alias);continue;}
            }
            result.items.push_back(NativeStatus(relative,evidence));
        }
        catch(const std::exception& e){result.items.push_back(NativeItem(relative,"inspection_failed",std::string("Could not inspect this DLL: ")+e.what(),"native_inspection"));}
    }
    return result;
}
inline bool StatusError(const Json& item) {
    const auto status=item.value("status","");
    return status=="unsupported"||status=="invalid"||status=="blocked"||status=="failed"||status=="rejected"||status=="not_loaded"||status=="not_initialized"||status=="inspection_failed";
}
inline Json ModStatusReport(Json retiredItems,const Json& nativeItems,unsigned long pid,bool complete,bool started=false) {
    int loaded=0,errors=0,pending=0;
    for(auto& item:retiredItems){item["source"]="retired_json";errors+=StatusError(item);}
    const auto unsupported=retiredItems.size();
    for(const auto& item:nativeItems){loaded+=item.value("status","")=="loaded";errors+=StatusError(item);pending+=item.value("status","")=="pending";retiredItems.push_back(item);}
    return Json{{"schemaVersion",3},{"runtimeVersion",RuntimePackageVersion},{"sessionPid",pid},{"nativeScanComplete",complete},{"nativeScanState",complete?"complete":started?"incomplete":"pending"},{"mods",retiredItems},{"active",loaded},{"nativeActive",loaded},{"errors",errors},{"pending",pending},{"unsupported",unsupported},{"gameplayPatching",false}};
}
}
