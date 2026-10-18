#include "../src/SlormStatApi.hpp"
#include "../src/SlormValueSlots.hpp"
#include "../src/SlormDiagnostics.hpp"
#include "../src/SlormModFolders.hpp"
#include "../src/SlormModStatus.hpp"
#define NOMINMAX
#include "../src/SlormRuntime.hpp"
#include <filesystem>
#include <iostream>
#include <memory>
#include <chrono>
#include <climits>

using namespace Slorm;
static unsigned checks = 0;
static void Check(bool result,const char* label) {if(!result)throw std::runtime_error(label); ++checks;std::cout << "PASS " << label << '\n';}
template<class Fn> static void Reject(Fn action,const char* label) {bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Check(rejected,label);}
static Json Read(const std::filesystem::path& file) {std::ifstream input(file);std::string text((std::istreambuf_iterator<char>(input)),{});return ParseJson(text);}
struct FakeBackend {
    struct Row {double ref{},base{},perRank{},extra{};std::string stat;};
    struct Snapshot {std::shared_ptr<Row> row;Field field{};double ref{},base{},perRank{},value{};std::string stat;};
    std::map<std::string,std::shared_ptr<Row>> rows;
    unsigned writes=0,invalidations=0,failAt=0;
    int currentHero=0;
    bool Ready(){return true;}
    int HeroClass(){return currentHero;}
    bool HeroReady(){return true;}
    static std::string Key(int ref,bool hero=false,int classId=0){return (hero?"hero:":"class:")+std::to_string(classId)+":"+std::to_string(ref);}
    bool HasHeroRows(const std::vector<StatPatch>& patches){return std::all_of(patches.begin(),patches.end(),[&](const StatPatch& p){return rows.contains(Key(p.target.skillRef,true,currentHero));});}
    static double& Cell(Row& row,Field field) {return field==Field::Base?row.base:field==Field::PerRank?row.perRank:row.extra;}
    void Add(int ref,const char* stat,double base,double per,double extra=0,bool hero=false,int classId=0) {rows[Key(ref,hero,classId)]=std::make_shared<Row>(Row{double(ref),base,per,extra,stat});}
    void Read(const StatPatch& p,bool hero,Snapshot& s) {auto row=rows.at(Key(p.target.skillRef,hero,hero?currentHero:p.target.classId));s={row,p.field,row->ref,row->base,row->perRank,Cell(*row,p.field),row->stat};}
    void Write(Snapshot& s,double v) {Cell(*s.row,s.field)=v;if(++writes==failAt)throw std::runtime_error("Injected write failure");}
    void Restore(Snapshot& s,double onlyIfValue) noexcept {auto& v=Cell(*s.row,s.field);if(v==onlyIfValue)v=s.value;}
    void Invalidate(const std::vector<StatPatch>&) {++invalidations;}
    double Value(int ref,Field field,bool hero=false,int classId=0) {return Cell(*rows.at(Key(ref,hero,classId)),field);}
};
struct SlotBackend {
    struct Row {double ref=0;std::string stats="physical_damage|max_health";std::map<int,std::string> cells{{4,"10|20"},{5,"1|2"},{11,"54"},{12,"0"},{20,"840"},{22,"12"},{24,"12"},{26,"0.5"},{29,"-35"}};};
    struct Snapshot {std::shared_ptr<Row> row;int column=0,index=0;bool slots=false;std::string previous;double ref{},base{},perRank{},value{};std::string stat;};
    std::shared_ptr<Row> row=std::make_shared<Row>();unsigned writes=0,invalidations=0,failAt=0;
    void Read(const StatPatch& p,bool,Snapshot& s){auto f=GetFieldSpec(p.field);s.row=row;s.column=f.column;s.index=f.resultSlots?p.target.valueIndex:0;s.slots=f.resultSlots;s.previous=row->cells.at(s.column);s.ref=row->ref;s.stat=Slot(SplitSlots(row->stats),p.target.valueIndex);s.base=NumericText(Slot(SplitSlots(row->cells.at(4)),p.target.valueIndex));s.perRank=NumericText(Slot(SplitSlots(row->cells.at(5)),p.target.valueIndex));s.value=NumericText(s.slots?Slot(SplitSlots(s.previous),s.index):s.previous);}
    void Write(Snapshot& s,double value){auto& cell=s.row->cells.at(s.column);cell=s.slots?ReplaceSlot(cell,s.index,NumericString(value)):NumericString(value);if(++writes==failAt)throw std::runtime_error("Injected packed write failure");}
    void Restore(Snapshot& s,double expected) noexcept {try{auto& cell=s.row->cells.at(s.column);if(NumericText(s.slots?Slot(SplitSlots(cell),s.index):cell)==expected)cell=s.slots?ReplaceSlot(cell,s.index,Slot(SplitSlots(s.previous),s.index)):s.previous;}catch(...){}}
    void Invalidate(const std::vector<StatPatch>&){invalidations++;}
};
int main(int argc,char** argv) {
    try {
        if(argc==3&&std::string(argv[1])=="--validate") {ParseManifest(Read(std::filesystem::u8path(argv[2])),true);return 0;}
        const auto root=std::filesystem::current_path();
        const auto fixtures=Read(root/"tests/fixtures/stat-manifests.json");
        {
            const std::string complete="[12:00:00] [trace] Aurie Core v2.0.2 loaded at 123\n[12:00:01] [trace] [ArProcessAttach] Init done.\n";
            Check(AurieStartupFinished(complete,true),"current-session Aurie completion marker confirms native startup");
            Check(!AurieStartupFinished(complete,false),"old Aurie log cannot finalize native status");
            Check(!AurieStartupFinished("[ArProcessAttach] Init done.\n",true),"completion text without current Aurie startup header is not proof");
            Check(!AurieStartupFinished(complete.substr(0,complete.size()-1),true),"partial final completion line remains pending");
            const std::string failed="[trace] [MdMapImageEx] MdpMapImage(Example.dll) returns AURIE_SUCCESS (GLE 0)\n[trace] MdpMapImage(Example.dll) returns AURIE_OBJECT_ALREADY_EXISTS\n[warning] [ArProcessAttach] Module \"Example.dll\" failed ModuleInitialize with status AURIE_MODULE_DEPENDENCY_NOT_RESOLVED and will be purged.\n";
            Check(AurieFailureStatus(failed,"Example.dll",true)=="AURIE_MODULE_DEPENDENCY_NOT_RESOLVED","native initialization failure retains exact current-log status");
            Check(AurieFailureStatus(failed,"Example.dll",false).empty(),"duplicate DLL basenames never inherit ambiguous log errors");
            Check(AurieFailureStatus(failed,"Other.dll",true).empty(),"one DLL cannot inherit another DLL's failure");
            Check(AurieFailureStatus("MdpMapImage(Example.dll) returns AURIE_SUCCESS\nMdpMapImage(Example.dll) returns AURIE_OBJECT_ALREADY_EXISTS\n","Example.dll",true).empty(),"successful and already-mapped log events are not failures");
            NativeEvidence valid;valid.architectureRead=true;valid.architecture=0x8664;valid.frameworkInit=true;valid.moduleInit=true;valid.lookupStatus="AURIE_OBJECT_NOT_FOUND";
            auto initialized=valid;initialized.registered=true;initialized.initialized=true;
            Check(NativeStatus("mods/Aurie/Example.dll",initialized)["status"]=="loaded","native loaded status requires confirmed Aurie initialization");
            initialized.initialized=false;Check(NativeStatus("mods/Aurie/Example.dll",initialized)["status"]=="not_initialized","registered but uninitialized DLL is not counted as loaded");
            auto proxy=valid;proxy.frameworkInit=false;proxy.moduleInit=false;
            auto rejected=NativeStatus("mods/Aurie/SlormReforger.dll",proxy);
            Check(rejected["status"]=="rejected"&&rejected["aurieStatus"]=="AURIE_INVALID_SIGNATURE"&&rejected["reason"].get<std::string>().find("__AurieFrameworkInit")!=std::string::npos,"proxy DLL missing Aurie exports has a readable signature rejection");
            auto x86=valid;x86.architecture=0x14c;Check(NativeStatus("mods/Aurie/Example.dll",x86)["aurieStatus"]=="AURIE_INVALID_ARCH","wrong architecture is reported distinctly");
            auto unreadable=valid;unreadable.architectureRead=false;unreadable.inspectionStatus="AURIE_ACCESS_DENIED";Check(NativeStatus("mods/Aurie/Example.dll",unreadable)["status"]=="inspection_failed","unreadable DLL is reported as an inspection failure");
            auto preinit=valid;preinit.moduleInit=false;preinit.modulePreinit=true;Check(NativeStatus("mods/Aurie/Example.dll",preinit)["status"]=="not_loaded","preinitialize-only Aurie modules pass export compatibility inspection");
            auto unknown=NativeStatus("mods/Aurie/Example.dll",valid);Check(unknown["status"]=="not_loaded"&&unknown["evidence"]=="aurie_lookup","absent compatible DLL does not invent an initialization failure reason");
            auto failure=valid;failure.originalFailure="AURIE_MODULE_DEPENDENCY_NOT_RESOLVED";Check(NativeStatus("mods/Aurie/Example.dll",failure)["evidence"]=="aurie_log","known original failure is attributed to its log evidence");
            Check(NativeStatus("mods/Aurie/Example.DLL",valid)["reason"].get<std::string>().find("lowercase .dll")!=std::string::npos,"uppercase manual DLL extension explains Aurie's discovery limitation");
            Check(!NativeCandidate("YYToolkit.dll")&&!NativeCandidate("SLORMLOADER.DLL")&&!NativeCandidate("AurieCore.dll")&&NativeCandidate("Example.DLL"),"framework DLLs are excluded case-insensitively from gameplay status");
            const auto fixture=root/"tests"/("native-status-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(fixture/"mods/Aurie/nested");
            struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}} cleanup{fixture};
            for(const auto& name:{"Example.dll","nested/Example.dll","Upper.DLL","YYToolkit.dll","SlormLoader.dll","AurieCore.dll","README.txt"}){std::ofstream out(fixture/"mods/Aurie"/name);out<<"fixture";}
            unsigned observations=0;auto pending=ScanNativeMods(fixture,false,[&](const auto&,bool){++observations;return valid;},"Waiting for startup");
            Check(observations==0&&pending.items.size()==3&&pending.items[0]["status"]=="pending","pending native scan enumerates candidates without touching Aurie's module list");
            unsigned ambiguous=0;auto scan=ScanNativeMods(fixture,true,[&](const auto& path,bool unique){++observations;if(!unique)++ambiguous;return path.filename()=="Upper.DLL"?valid:initialized;},"");
            Check(observations==3&&ambiguous==2&&scan.complete,"post-startup scan observes each candidate and detects duplicate basenames");
            Check(scan.items[0]["id"]!=scan.items[2]["id"]&&scan.items[0]["path"]!=scan.items[2]["path"],"nested DLLs retain distinct path-based identities");
            auto isolated=ScanNativeMods(fixture,true,[&](const auto& path,bool){if(path.parent_path().filename()=="nested")throw std::runtime_error("Injected inspection failure");return valid;},"");
            Check(isolated.items.size()==3&&isolated.items[2]["status"]=="inspection_failed","one DLL inspection failure preserves other native candidates");
            std::filesystem::create_hard_link(fixture/"mods/Aurie/Example.dll",fixture/"mods/Aurie/Alias.dll");
            auto aliases=ScanNativeMods(fixture,true,[&](const auto& path,bool){auto observed=valid;observed.registered=true;observed.initialized=true;observed.moduleIdentity=std::filesystem::equivalent(path,fixture/"mods/Aurie/Example.dll")?1:2;return observed;},"");
            Check(std::count_if(aliases.items.begin(),aliases.items.end(),[](const Json& item){return item["status"]=="loaded";})==2&&std::count_if(aliases.items.begin(),aliases.items.end(),[](const Json& item){return item["status"]=="alias";})==2,"multiple DLL paths sharing an Aurie module are counted once per module identity");
            std::filesystem::create_hard_link(fixture/"mods/Aurie/YYToolkit.dll",fixture/"mods/Aurie/ToolkitAlias.dll");
            auto noFrameworkAlias=ScanNativeMods(fixture,false,[&](const auto&,bool){throw std::runtime_error("No lookup allowed");return valid;},"Waiting");
            Check(noFrameworkAlias.items.size()==4,"hardlink aliases of pinned framework DLLs are excluded before lookup");
            auto registeredFramework=ScanNativeMods(fixture,true,[&](const auto&,bool){auto observed=valid;observed.registered=true;observed.initialized=true;observed.registeredFilename="YYToolkit.dll";return observed;},"");
            Check(registeredFramework.items.empty(),"registered framework identity cannot be counted under a gameplay alias filename");
            Json jsonItems=Json::array({Json{{"id","json-ok"},{"status","applied"}},Json{{"id","json-blocked"},{"status","blocked"}},Json{{"id","json-invalid"},{"status","invalid"}}});
            Json nativeItems=Json::array({NativeStatus("mods/Aurie/Loaded.dll",NativeEvidence{true,true}),rejected,pending.items[0]});
            auto report=ModStatusReport(jsonItems,nativeItems,123,false);
            Check(report["active"]==2&&report["errors"]==3&&report["pending"]==1,"unified totals count JSON and native success failures and pending separately");
            Check(report["jsonActive"]==1&&report["nativeActive"]==1&&report["jsonErrors"]==2&&report["nativeErrors"]==1&&report["mods"].size()==6,"unified report includes both sources and exact count breakdowns");
            Check(report["nativeScanState"]=="pending"&&ModStatusReport(jsonItems,Json::array(),123,false,true)["nativeScanState"]=="incomplete","pending startup and incomplete filesystem scan remain distinct");
            Check(ModStatusReport(Json::array(),Json::array(),123,true)["active"]==0&&ModStatusReport(Json::array(),Json::array(),123,true)["nativeScanState"]=="complete","empty successful native scan reports zero mods without errors");
            const auto broken=fixture/"broken";std::filesystem::create_directories(broken/"mods");{std::ofstream out(broken/"mods/Aurie");out<<"not a directory";}
            auto badScan=ScanNativeMods(broken,true,[&](const auto&,bool){return valid;},"");
            Check(!badScan.complete&&badScan.items.size()==1&&StatusError(badScan.items[0]),"native root scan failures are visible instead of silently becoming zero errors");
            FILETIME creation{},exited{},kernel{},user{};
            if(!GetProcessTimes(GetCurrentProcess(),&creation,&exited,&kernel,&user))throw std::runtime_error("Cannot read fixture process creation time (Windows error "+std::to_string(GetLastError())+").");
            ULARGE_INTEGER time{};time.LowPart=creation.dwLowDateTime;time.HighPart=creation.dwHighDateTime;
            if(time.QuadPart<10000000||time.QuadPart>ULLONG_MAX-10000000)throw std::runtime_error("Fixture process creation time cannot be offset safely.");
            time.QuadPart+=10000000;const FILETIME fresh{time.LowPart,time.HighPart};
            time.QuadPart-=20000000;const FILETIME old{time.LowPart,time.HighPart};
            // Set explicit times around process creation instead of depending on filesystem wall-clock precision.
            const auto log=fixture/"aurie.log";{std::ofstream out(log);out<<complete;}
            const auto stampLog=[&](const FILETIME& stamp){
                const auto handle=CreateFileW(log.c_str(),FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open fixture timestamp (Windows error "+std::to_string(GetLastError())+").");
                const bool changed=SetFileTime(handle,nullptr,nullptr,&stamp);const auto error=GetLastError();CloseHandle(handle);
                if(!changed)throw std::runtime_error("Cannot set fixture timestamp (Windows error "+std::to_string(error)+").");
            };
            stampLog(fresh);
            std::string text,reason;const bool currentRead=SlormReadCurrentAurieLog(fixture,text,reason);
            Check(currentRead&&AurieStartupFinished(text,true),("actual current-process log can be read with shared access"+(currentRead?std::string():": "+reason)).c_str());
            stampLog(old);
            Check(!SlormReadCurrentAurieLog(fixture,text,reason)&&reason.find("older log")!=std::string::npos,"actual stale log timestamp prevents current native status confirmation");
            const auto writer=CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(writer==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open held-writer fixture");
            struct CloseWriter {HANDLE file;~CloseWriter(){CloseHandle(file);}} closeWriter{writer};DWORD written=0;if(!WriteFile(writer,complete.data(),static_cast<DWORD>(complete.size()),&written,nullptr)||written!=complete.size()||!FlushFileBuffers(writer))throw std::runtime_error("Cannot write held-open Aurie fixture");
            stampLog(fresh);const bool heldRead=SlormReadCurrentAurieLog(fixture,text,reason);
            Check(heldRead&&AurieStartupFinished(text,true),("truncated current Aurie log is confirmed while its writer stays open"+(heldRead?std::string():": "+reason)).c_str());
        }
        {
            const auto fixture=root/"tests"/("discovery-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(fixture);
            struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}} cleanup{fixture};
            Check(InactiveStatModFolder(fixture),"empty removed-mod directory is inactive");
            {std::ofstream out(fixture/"__folder_managed_by_vortex");out<<"Vortex directory marker";}
            Check(InactiveStatModFolder(fixture),"Vortex marker alone is inactive");
            const auto manifest=fixture/"mod.json";
            {std::ofstream out(manifest);out<<fixtures.at("tenacity").dump();}
            Check(!InactiveStatModFolder(fixture),"deployed manifest beside Vortex marker remains discoverable");
            std::filesystem::remove(manifest);
            Check(InactiveStatModFolder(fixture),"Vortex single-mod undeployment leaves no mod candidate");
            {std::ofstream out(manifest);out<<"broken JSON";}
            Check(!InactiveStatModFolder(fixture),"malformed manifest still reaches normal validation");
            std::filesystem::remove(manifest);
            {std::ofstream out(fixture/"README.txt");out<<"Incomplete mod package";}
            Check(!InactiveStatModFolder(fixture),"payload without manifest remains an error candidate");
            std::filesystem::remove(fixture/"README.txt");
            std::filesystem::remove(fixture/"__folder_managed_by_vortex");
            std::filesystem::create_directory(fixture/"__folder_managed_by_vortex");
            Check(!InactiveStatModFolder(fixture),"directory impersonating Vortex marker is not ignored");
        }
        auto irremovable=ParseManifest(fixtures.at("tenacity"));
        auto plate=ParseManifest(fixtures.at("armor"));
        Check(irremovable.patches[0].value==8&&plate.patches[0].value==-10,"both real mod manifests accepted");
        Check(ParseManifest(Read(root/"src/slorm-runtime.json"),true).version==RuntimePackageVersion,"runtime metadata matches distribution version while JSON API remains compatible");
        Check(Satisfies("0.3.0",">=0.3.0 <0.4.0")&&!Satisfies("0.4.0",">=0.3.0 <0.4.0"),"version range boundaries");
        Reject([]{ParseVersion("01.2.3");},"ambiguous leading-zero versions rejected");
        Reject([]{Satisfies("1.2.3","^1.0.0");},"unsupported version range syntax rejected");
        Reject([]{ParseJson("{\"id\":\"a\",\"id\":\"b\"}");},"duplicate JSON keys rejected");
        Reject([]{ParseJson(std::string(65537,' '));},"oversized manifest rejected");
        auto original=fixtures.at("tenacity"),bad=original;
        bad["schemaVersion"]=2;Reject([&]{ParseManifest(bad);},"unknown schema version rejected");
        bad=original;bad["id"]="../escape";Reject([&]{ParseManifest(bad);},"path-like mod ID rejected");
        bad=original;bad["id"]="con";Reject([&]{ParseManifest(bad);},"Windows device name rejected");
        bad=original;bad["requires"]=Json::array();Reject([&]{ParseManifest(bad);},"runtime requirement is mandatory");
        bad=original;bad["unknown"]=true;Reject([&]{ParseManifest(bad);},"unknown metadata fields rejected");
        bad=original;bad["patches"][0]["value"]="8";Reject([&]{ParseManifest(bad);},"patch values must be JSON numbers");
        bad=original;bad["patches"].push_back(bad["patches"][0]);Reject([&]{ParseManifest(bad);},"duplicate patch IDs rejected");
        {
            std::vector<Mod> mods{irremovable,plate};auto order=Resolve(mods);
            Check(order.size()==2&&mods[order[0]].id=="full-plate-armor","independent mods resolve deterministically");
        }
        {
            auto a=irremovable;a.dependencies.push_back({"yytoolkit",">=5.0.0 <6.0.0"});
            std::vector<Mod> mods{a};Check(Resolve(mods).size()==1,"v5 framework dependency resolves against the bundled toolkit");
            a.dependencies.back().version=">=4.0.0 <5.0.0";mods={a};
            Check(Resolve(mods).empty()&&mods[0].status=="blocked","v4-only framework dependency is blocked on the v5 runtime");
            auto tuning=ParseManifest(fixtures.at("tuning"));
            mods={irremovable,plate,tuning};Check(Resolve(mods).size()==3,"all three existing JSON packages retain compatible runtime ranges");
        }
        {
            auto a=irremovable;a.dependencies.push_back({plate.id,">=1.0.0"});std::vector<Mod> mods{a,plate};auto order=Resolve(mods);
            Check(order.size()==2&&mods[order[0]].id==plate.id,"dependency applies before dependent");
        }
        {
            auto a=irremovable;a.dependencies.push_back({"missing-mod","1.0.0"});std::vector<Mod> mods{a,plate};auto order=Resolve(mods);
            Check(order.size()==1&&mods[0].status=="blocked"&&mods[1].status=="pending","missing dependency blocks only affected mod");
        }
        {
            auto a=irremovable;a.dependencies[0].version=">=0.4.0";std::vector<Mod> mods{a};Check(Resolve(mods).empty()&&mods[0].status=="blocked","incompatible runtime version blocked");
        }
        {
            auto a=irremovable;a.dependencies.push_back({plate.id,"1.0.0"});auto b=plate;b.dependencies.push_back({a.id,"2.0.0"});std::vector<Mod> mods{a,b};
            Check(Resolve(mods).empty()&&mods[0].status=="blocked"&&mods[1].status=="blocked","dependency cycle never applies");
        }
        {
            auto a=irremovable;a.conflicts.push_back(plate.id);std::vector<Mod> mods{a,plate};Check(Resolve(mods).empty(),"explicit conflicts block both sides");
        }
        {
            auto a=irremovable;a.id="alternate-tenacity";std::vector<Mod> mods{a,irremovable,plate};Check(Resolve(mods).size()==1&&mods[2].status=="pending","overlapping skill patches block both but preserve unrelated mod");
        }
        {
            std::vector<Mod> mods{irremovable,irremovable};Check(Resolve(mods).empty(),"duplicate deployed IDs rejected");
        }
        {
            FakeBackend b;b.Add(112,"tenacity",20,2);b.Add(205,"res_phy_percent",30,7,-35);StatPatchEngine<FakeBackend> api(b);
            api.Apply(irremovable);api.Apply(plate);
            Check(b.Value(112,Field::PerRank)==8&&b.Value(205,Field::ExtraNumber)==-10,"shared helper applies both real mods");
            Check(b.Value(205,Field::Base)==30&&b.Value(205,Field::PerRank)==7,"Full Plate Armor resistance bonus unchanged");
            b.Add(112,"tenacity",20,2,0,true);api.Apply(irremovable,true);
            Check(b.Value(112,Field::PerRank,true)==8,"later character-specific row receives same patch");
            unsigned before=b.writes;api.Apply(irremovable,true);Check(b.writes==before,"repeated hero checks do not rewrite values");
            api.RestoreMod(irremovable);api.RestoreMod(plate);
            Check(b.Value(112,Field::PerRank)==2&&b.Value(112,Field::PerRank,true)==2&&b.Value(205,Field::ExtraNumber)==-35,"original fields restored across class and hero copies");
        }
        {
            FakeBackend b;b.Add(112,"tenacity",20,3);StatPatchEngine<FakeBackend> api(b);
            Reject([&]{api.Apply(irremovable);},"changed game schema rejected");Check(b.writes==0&&b.Value(112,Field::PerRank)==3,"schema rejection leaves original runtime untouched");
        }
        {
            FakeBackend b;b.Add(112,"tenacity",20,2);b.Add(205,"res_phy_percent",30,7,-40);auto group=irremovable;group.patches.push_back(plate.patches[0]);StatPatchEngine<FakeBackend> api(b);
            Reject([&]{api.Apply(group);},"second invalid patch rejects entire mod");Check(b.writes==0&&b.Value(112,Field::PerRank)==2,"preflight prevents partial application");
        }
        {
            FakeBackend b;b.Add(112,"tenacity",20,2);b.Add(205,"res_phy_percent",30,7,-35);b.failAt=2;auto group=irremovable;group.patches.push_back(plate.patches[0]);StatPatchEngine<FakeBackend> api(b);
            Reject([&]{api.Apply(group);},"injected failure during second write reported");Check(b.Value(112,Field::PerRank)==2&&b.Value(205,Field::ExtraNumber)==-35,"write failure rolls back all committed fields");
        }
        {
            FakeBackend b;b.Add(112,"tenacity",20,2);StatPatchEngine<FakeBackend> api(b);api.Apply(irremovable);b.rows[FakeBackend::Key(112)]->perRank=9;api.RestoreMod(irremovable);
            Check(b.Value(112,Field::PerRank)==9,"restoration preserves a later external change");
        }
        {
            auto mixed=irremovable;auto other=irremovable.patches[0];other.id="other-class";other.target.classId=1;other.target.stat="different-stat";mixed.patches.push_back(other);
            FakeBackend b;b.Add(112,"tenacity",20,2);b.Add(112,"different-stat",20,2,0,false,1);b.Add(112,"different-stat",20,2,0,true,1);b.currentHero=1;
            std::vector<Mod> mods{mixed};auto order=Resolve(mods);StatModSession<FakeBackend> session(b,mods,order,[](const auto&){});session.Tick(0);
            Check(b.Value(112,Field::PerRank)==8&&b.Value(112,Field::PerRank,false,1)==8&&b.Value(112,Field::PerRank,true,1)==8,"mixed-class mod patches matching class and selected hero only");
        }
        {
            FakeBackend b;b.Add(112,"tenacity",20,2);b.Add(205,"res_phy_percent",30,7,-35);auto dependent=plate;dependent.dependencies.push_back({irremovable.id,"2.0.0"});
            std::vector<Mod> mods{irremovable,dependent};auto order=Resolve(mods);
            StatModSession<FakeBackend> session(b,mods,order,[](const auto& line){if(line.rfind("STAT_PATCH_APPLIED",0)==0)throw std::runtime_error("Injected log failure");});session.Tick(0);
            Check(mods[0].status=="failed"&&b.Value(112,Field::PerRank)==2,"post-commit logging failure restores committed fields");
            Check(mods[1].status=="blocked"&&b.Value(205,Field::ExtraNumber)==-35,"dependent cannot apply after dependency reporting failure");
        }
        {
            auto huntress=irremovable;huntress.id="huntress-test";huntress.patches[0].target.classId=1;huntress.patches[0].target.stat="different-stat";
            FakeBackend b;b.Add(112,"different-stat",20,2,0,false,1);b.Add(112,"tenacity",20,2,0,true,1);b.currentHero=1;
            std::vector<Mod> mods{huntress};auto order=Resolve(mods);StatModSession<FakeBackend> session(b,mods,order,[](const auto&){});session.Tick(0);
            Check(mods[0].status=="applied"&&!mods[0].reason.empty()&&b.Value(112,Field::PerRank,true,1)==2,"transient stale hero table defers without wrong-class write");
            b.Add(112,"different-stat",20,2,0,true,1);session.Tick(100);
            Check(mods[0].status=="applied"&&mods[0].reason.empty()&&b.Value(112,Field::PerRank,true,1)==8,"correct table arriving during class switch receives patch");
        }
        {
            auto dependent=plate;dependent.dependencies.push_back({irremovable.id,"2.0.0"});auto third=plate;third.id="third-test";third.patches[0].target.skillRef=206;third.dependencies.push_back({plate.id,"1.0.0"});
            auto unrelated=plate;unrelated.id="unrelated-test";unrelated.patches[0].target.skillRef=207;
            FakeBackend b;b.Add(112,"tenacity",20,2);b.Add(112,"tenacity",20,2,0,true);
            for(int ref:{205,206,207}){b.Add(ref,"res_phy_percent",30,7,-35);b.Add(ref,"res_phy_percent",30,7,-35,true);}
            std::vector<Mod> mods{irremovable,dependent,third,unrelated};auto order=Resolve(mods);StatModSession<FakeBackend> session(b,mods,order,[](const auto&){});session.Tick(0);
            b.rows[FakeBackend::Key(112,true)]->stat="incompatible";session.Tick(10);session.Tick(2010);
            Check(mods[0].status=="failed"&&b.Value(112,Field::PerRank)==2&&b.Value(112,Field::PerRank,true)==2,"persistent hero mismatch restores class and hero patches");
            Check(mods[1].status=="blocked"&&mods[2].status=="blocked"&&b.Value(205,Field::ExtraNumber)==-35&&b.Value(206,Field::ExtraNumber,true)==-35,"hero failure rolls back transitive dependents");
            Check(mods[3].status=="applied"&&b.Value(207,Field::ExtraNumber,true)==-10,"unrelated mod survives dependency rollback");
        }
        {
            std::vector<StatPatch> patches{irremovable.patches[0],plate.patches[0]};std::vector<int> cleared;
            InvalidateAffectedSkills(patches,0,[&](int ref){if(ref==112)throw std::runtime_error("Missing cache row");cleared.push_back(ref);});
            Check(cleared==std::vector<int>{205},"missing cache entry does not suppress later invalidation");
        }
        {
            Check(FieldSpecs.size()==9&&GetFieldSpec(Field::Cooldown).column==20&&GetFieldSpec(Field::ManaCost).column==11,"nine registered fields include verified cooldown and cost offsets");
            Check(ReplaceSlot("10|20|0.00",1,"40")=="10|40|0.00","editing one packed value preserves other token text");
            Reject([]{NumericText("2|3");},"packed string cannot masquerade as scalar numeric field");
            Reject([]{NumericText("1junk");},"numeric suffixes rejected");
            Reject([]{NumericText("+-1");},"mixed numeric signs rejected");
            Reject([]{NumericText("++1");},"repeated numeric signs rejected");
            Reject([]{ReplaceSlot("1|2",2,"3");},"absent result slot rejected");
            Reject([]{NumericText("NaN");},"nonfinite packed values rejected");
            auto json=original;json["id"]="slot-test";json["patches"]=Json::array();
            for(int index:{0,1})json["patches"].push_back({{"id","slot-"+std::to_string(index)},{"target",{{"classId",0},{"skillRef",0},{"stat",index==0?"physical_damage":"max_health"},{"expectedBase",index==0?10:20},{"expectedPerRank",index==0?1:2},{"valueIndex",index}}},{"field","base"},{"expected",index==0?10:20},{"value",index==0?15:40}});
            auto mod=ParseManifest(json);SlotBackend b;StatPatchEngine<SlotBackend> api(b);api.Apply(mod);
            Check(b.row->cells.at(4)=="15|40","multiple patches in same packed column do not clobber each other");
            api.RestoreMod(mod);Check(b.row->cells.at(4)=="10|20","packed components restore in reverse order");
            api.Apply(mod);b.row->cells[4]="15|99";api.RestoreMod(mod);Check(b.row->cells.at(4)=="10|99","restoring one packed component preserves external edits to another");
            auto invalid=json;invalid["patches"][1]["target"]["valueIndex"]=16;Reject([&]{ParseManifest(invalid);},"result index bound enforced by manifest");
            mod.patches[1].target.valueIndex=2;b.row->cells[4]="10|20";unsigned before=b.writes;Reject([&]{api.Apply(mod);},"missing second packed slot rejects entire mod");Check(b.writes==before,"packed preflight failure writes no components");
            std::vector<std::pair<int,int>> invalidated;auto patches=ParseManifest(json).patches;
            InvalidateAffectedSlots(patches,0,[&](int ref,int index){invalidated.emplace_back(ref,index);});
            Check(invalidated==std::vector<std::pair<int,int>>{{0,0},{0,1}},"cache invalidation targets both edited result indices");
            SlotBackend failing;failing.row->cells[4]="010|020.00";failing.failAt=2;StatPatchEngine<SlotBackend> failingApi(failing);
            Reject([&]{failingApi.Apply(ParseManifest(json));},"second packed write failure reported");
            Check(failing.row->cells[4]=="010|020.00","packed rollback restores exact original token formatting");
        }
        {
            SlotBackend b;b.row->ref=9;b.row->stats="physical_damage";b.row->cells[4]="30";b.row->cells[5]="0";
            auto mod=irremovable;mod.id="raw-fields-test";mod.patches.clear();
            for(auto field:{Field::ManaCost,Field::ManaCostPerRank,Field::Cooldown,Field::Knockback,Field::PrecastTime,Field::CastTime}) {auto info=GetFieldSpec(field);double expected=NumericText(b.row->cells.at(info.column));mod.patches.push_back({info.id,{0,9,"physical_damage",30,0},field,expected,expected/2});}
            StatPatchEngine<SlotBackend> api(b);api.Apply(mod);
            Check(b.row->cells[11]=="27"&&b.row->cells[20]=="420"&&b.row->cells[26]=="0.25","direct cost cooldown and knockback parameters apply");
            Check(b.row->cells[22]=="6"&&b.row->cells[24]=="6"&&b.row->cells[4]=="30","cast timing changes leave displayed damage intact");
            api.RestoreMod(mod);Check(b.row->cells[11]=="54"&&b.row->cells[20]=="840"&&b.row->cells[22]=="12","raw parameters restore original values");
            auto invalid=original;invalid["patches"][0]["field"]="cooldown";invalid["patches"][0]["expected"]=840;invalid["patches"][0]["value"]=-1;Reject([&]{ParseManifest(invalid);},"negative cooldown replacement rejected");
            invalid["patches"][0]["value"]=420;invalid["patches"][0]["target"]["valueIndex"]=1;Reject([&]{ParseManifest(invalid);},"direct fields cannot select a displayed result slot");
        }
        {
            Json config{{"schemaVersion",1},{"enabled",true},{"calculations",true},{"ranks",Json::array({1,10})}};
            auto options=ParseDiagnostics(config);Check(options.calculations&&options.ranks==std::vector<int>{1,10},"opt-in diagnostic configuration accepted");
            auto invalid=config;invalid["enabled"]=false;Reject([&]{ParseDiagnostics(invalid);},"disabled diagnostic file cannot enable probes");
            invalid=config;invalid["unknown"]=true;Reject([&]{ParseDiagnostics(invalid);},"unknown diagnostic option rejected");
            invalid=config;invalid["ranks"]=Json::array({1,1});Reject([&]{ParseDiagnostics(invalid);},"duplicate probe ranks rejected");
            invalid=config;invalid["ranks"]=Json::array({1.5});Reject([&]{ParseDiagnostics(invalid);},"fractional probe ranks rejected");
            invalid=config;invalid["ranks"]=Json::array({16});Reject([&]{ParseDiagnostics(invalid);},"probe ranks bounded");
            invalid=config;invalid["calculations"]="yes";Reject([&]{ParseDiagnostics(invalid);},"probe switch must be Boolean");
            Check(DiagnosticMatches(27,27)&&!DiagnosticMatches(54,27)&&!DiagnosticMatches(NAN,27),"factual readback matching rejects wrong and nonfinite values");
            auto formula=PatchedFormula(irremovable,irremovable.patches[0]);Check(formula.first==20&&formula.second==8,"probe prediction uses modified per-rank field");
            auto multi=irremovable;auto other=multi.patches[0];other.target.valueIndex=1;other.field=Field::Base;other.value=100;multi.patches.push_back(other);formula=PatchedFormula(multi,multi.patches[0]);Check(formula.first==20&&formula.second==8,"probe prediction excludes sibling result component");
            auto observed=irremovable;observed.status="applied";std::vector<Mod> observedMods{observed};
            Check(DiagnosticRelevant(observedMods,"scr_skill_get_result_value",0,112,0),"natural result observer accepts patched hero and component");
            Check(!DiagnosticRelevant(observedMods,"scr_skill_get_result_value",1,112,0),"natural result observer excludes another class with same reference");
            Check(!DiagnosticRelevant(observedMods,"scr_skill_get_result_value",0,112,1),"natural result observer excludes another result component");
            observedMods[0].status="failed";Check(!DiagnosticRelevant(observedMods,"scr_skill_get_result_value",0,112,0),"failed mods cannot label unrelated calls as patch observations");
            observedMods[0].status="applied";Check(!DiagnosticRelevant(observedMods,"scr_modified_skill_cost",0,112,-1),"stat scaling patch does not claim a cost observation");
            observedMods[0].reason="Waiting for character skill data";Check(!DiagnosticRelevant(observedMods,"scr_skill_get_result_value",0,112,0),"transient character-table wait suppresses natural patch observations");observedMods[0].reason.clear();
            observedMods[0].patches[0].field=Field::ManaCost;Check(DiagnosticRelevant(observedMods,"scr_modified_skill_cost",0,112,-1),"cost patches select natural cost observations");
            Check(DiagnosticProbeEligible(irremovable,irremovable.patches[0]),"known Irremovable schema permits explicit probe");
            auto unknown=irremovable;unknown.patches[0].target.skillRef=0;Check(!DiagnosticProbeEligible(unknown,unknown.patches[0]),"arbitrary skills cannot trigger unverified explicit probes");
            FakeBackend b;b.Add(112,"tenacity",20,8);FakeBackend::Snapshot readback;b.Read(irremovable.patches[0],false,readback);
            Check(DiagnosticFieldMatches(irremovable,irremovable.patches[0],readback),"readback verifies modified field and complete target guards");
            readback.ref=113;Check(!DiagnosticFieldMatches(irremovable,irremovable.patches[0],readback),"same value in wrong skill row cannot pass factual readback");
            readback.ref=112;readback.value=2;Check(!DiagnosticFieldMatches(irremovable,irremovable.patches[0],readback),"unmodified original field cannot pass factual readback");
        }
        std::cout << "RESULT: " << checks << " checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
