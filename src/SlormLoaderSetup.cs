using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;
using System.Web.Script.Serialization;

// Bootstrap installer only. Vortex owns mod installation, profiles and deployment.
public static class SlormLoaderSetup {
    static readonly JavaScriptSerializer Json=new JavaScriptSerializer{MaxJsonLength=4000000};
    static string packageRoot=AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
    public static string Hash(string path){using(var stream=File.OpenRead(path))using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(stream)).Replace("-","").ToLowerInvariant();}
    static T Read<T>(string path){return Json.Deserialize<T>(File.ReadAllText(path));}
    static string PackageVersion(){return Convert.ToString(Read<Dictionary<string,object>>(Path.Combine(packageRoot,"slorm-runtime.json"))["version"]);}
    static void Write(string path,object value){Directory.CreateDirectory(Path.GetDirectoryName(path));string tmp=path+".tmp";File.WriteAllText(tmp,Json.Serialize(value),new UTF8Encoding(false));if(File.Exists(path))File.Replace(tmp,path,null);else File.Move(tmp,path);}
    static void Copy(string source,string target){Directory.CreateDirectory(Path.GetDirectoryName(target));string tmp=target+".tmp";File.Copy(source,tmp,true);if(File.Exists(target))File.Replace(tmp,target,null);else File.Move(tmp,target);}
    static string Safe(string root,string relative){
        string basePath=Path.GetFullPath(root).TrimEnd('\\')+"\\";
        if(Path.IsPathRooted(relative)||relative.Contains(":"))throw new Exception("Invalid package path.");
        string result=Path.GetFullPath(Path.Combine(basePath,relative));
        if(!result.StartsWith(basePath,StringComparison.OrdinalIgnoreCase))throw new Exception("Package path escapes root.");
        for(string check=result;check!=null && check.Length>=basePath.Length-1;check=Path.GetDirectoryName(check)){
            if(!File.Exists(check)&&!Directory.Exists(check))continue;
            FileAttributes attributes=File.GetAttributes(check);
            if((attributes&FileAttributes.ReparsePoint)==0)continue;
            string kind=(attributes&FileAttributes.Directory)!=0 ? "Linked directory or junction" : "Symbolic-link file";
            throw new Exception(kind+" is not supported: "+check+"\r\n\r\n"+
                "For Vortex, open Settings > Mods. Put the Mod Staging Folder on the same NTFS volume as the selected game, choose Hardlink Deployment, then deploy again and run Configure Slormancer loader.\r\n"+
                "Vortex itself can stay installed on another drive. Linked game/mod directories must be replaced with normal directories.");
        }
        return result;
    }
    static bool Patched(string exe){byte[] b=File.ReadAllBytes(exe);if(b.Length<64||b[0]!=77||b[1]!=90)throw new Exception("Not a Windows executable.");int pe=BitConverter.ToInt32(b,60);if(pe<0||pe>b.Length-256||BitConverter.ToUInt16(b,pe+4)!=0x8664)throw new Exception("The loader requires the x64 game.");int table=pe+24+BitConverter.ToUInt16(b,pe+20),count=BitConverter.ToUInt16(b,pe+6);for(int i=0;i<count;i++)if(Encoding.ASCII.GetString(b,table+i*40,8).TrimEnd('\0')==".aurie")return true;return false;}
    static void Stopped(){if(Process.GetProcessesByName("The Slormancer").Length!=0)throw new Exception("Close The Slormancer before configuring or restoring the loader.");}
    static void VerifyPackage(){var pins=Read<Dictionary<string,string>>(Path.Combine(packageRoot,"slorm-loader-checksums.json"));foreach(var item in pins)if(Hash(Safe(packageRoot,item.Key))!=item.Value)throw new Exception("Loader package is missing or changed: "+item.Key);}
    static void Recover(string game){string state=Safe(game,".slorm-loader\\state"),pending=Path.Combine(state,"pending.json"),receipt=Path.Combine(state,"bootstrap.json");if(!File.Exists(pending))return;Stopped();var old=Read<Dictionary<string,string>>(pending);string backup=Path.Combine(state,"transaction.exe");if(Hash(backup)!=old["hash"])throw new Exception("Recovery backup checksum mismatch.");Copy(backup,Safe(game,"The Slormancer.exe"));if(old.ContainsKey("receipt"))Write(receipt,Json.Deserialize<Dictionary<string,string>>(old["receipt"]));else if(File.Exists(receipt))File.Delete(receipt);File.Delete(pending);File.Delete(backup);Console.WriteLine("Recovered interrupted bootstrap transaction.");}
    static void Install(string game){
        Stopped();Recover(game);VerifyPackage();string exe=Safe(game,"The Slormancer.exe"),core=Safe(game,"plugins\\AurieCore.asi"),state=Safe(game,".slorm-loader\\state"),receipt=Path.Combine(state,"bootstrap.json");
        if(!File.Exists(Safe(game,"data.win")))throw new Exception("This is not the folder containing The Slormancer.exe and data.win.");
        if(core.Length>=260)throw new Exception("Aurie's current path handling requires a game path shorter than 260 characters.");
        // A legacy owned entrypoint patch must be restored before the proxy is
        // enabled. No new executable patch is ever installed by this version.
        if(File.Exists(receipt)){
            var previous=Read<Dictionary<string,string>>(receipt);
            if(!previous.ContainsKey("mode"))RestoreLegacy(game);
            else if(previous["mode"]!="version-proxy")throw new Exception("Unrecognized loader receipt. Restore it with its owning loader first.");
        }
        if(Patched(exe))throw new Exception("An unowned Aurie executable patch remains. Restore the original game using the owning loader before using this proxy runtime.");
        if(File.Exists(Safe(game,"mods\\Native\\AurieCore.dll")))throw new Exception("Legacy mods/Native/AurieCore.dll remains deployed. Disable the previous runtime so Core cannot initialize twice.");
        string proxy=Safe(game,"version.dll");if(!File.Exists(proxy)||!File.Exists(core))throw new Exception("Proxy runtime is incomplete. Redeploy version.dll and plugins/AurieCore.asi.");
        Write(receipt,new Dictionary<string,string>{{"version",PackageVersion()},{"mode","version-proxy"},{"original",Hash(exe)},{"proxy",Hash(proxy)},{"corePath",core}});
        Console.WriteLine("Version proxy verified. The game executable is unchanged; Vortex owns deployment and removal of version.dll and plugins/AurieCore.asi.");
    }
    static void RestoreLegacy(string game){
        Stopped();Recover(game);string state=Safe(game,".slorm-loader\\state"),receipt=Path.Combine(state,"bootstrap.json"),original=Path.Combine(state,"original.exe"),exe=Safe(game,"The Slormancer.exe");var pins=Read<Dictionary<string,string>>(receipt);if(Hash(original)!=pins["original"])throw new Exception("Original executable backup checksum mismatch.");if(Hash(exe)!=pins["patched"]&&Hash(exe)!=pins["original"])throw new Exception("Game executable changed externally; restoration stopped.");Copy(original,exe);File.Delete(receipt);Console.WriteLine("Original executable restored. Purge or disable loader files in Vortex separately.");
    }
    static void Restore(string game){
        Stopped();Recover(game);string receipt=Safe(game,".slorm-loader\\state\\bootstrap.json");
        if(!File.Exists(receipt)){Console.WriteLine("No loader setup record remains. The game executable was not changed.");return;}
        var pins=Read<Dictionary<string,string>>(receipt);if(!pins.ContainsKey("mode")){RestoreLegacy(game);return;}
        if(pins["mode"]!="version-proxy")throw new Exception("Unrecognized loader receipt; no changes made.");
        if(Patched(Safe(game,"The Slormancer.exe")))throw new Exception("An external executable patch is present. The proxy setup cannot restore an unowned patch.");
        File.Delete(receipt);Console.WriteLine("Proxy setup record removed. Vortex removes version.dll and Core on undeployment; the game executable is unchanged.");
    }
    static void Check(string game){VerifyPackage();string receipt=Safe(game,".slorm-loader\\state\\bootstrap.json"),exe=Safe(game,"The Slormancer.exe");if(!File.Exists(receipt))throw new Exception("Loader has not been configured. Run Configure Slormancer loader after deploying the loader package.");var pins=Read<Dictionary<string,string>>(receipt);if(!pins.ContainsKey("mode")||pins["mode"]!="version-proxy")throw new Exception("Legacy executable bootstrap detected. Configure the proxy runtime to migrate it.");if(Patched(exe)||Hash(exe)!=pins["original"]||Hash(Safe(game,"version.dll"))!=pins["proxy"])throw new Exception("Proxy installation or game executable changed. Redeploy and configure the runtime.");Console.WriteLine("Proxy bootstrap verified; game executable unchanged: "+pins["corePath"]);string logs=Safe(game,".slorm-loader\\logs");if(Directory.Exists(logs)){string latest=Directory.GetFiles(logs,"runtime-*.log").OrderByDescending(File.GetLastWriteTimeUtc).FirstOrDefault();if(latest!=null){Console.WriteLine("Latest runtime log: "+latest);using(var stream=new FileStream(latest,FileMode.Open,FileAccess.Read,FileShare.ReadWrite))using(var reader=new StreamReader(stream))Console.WriteLine(reader.ReadToEnd());}}Console.WriteLine(LastModStatus(game));}
    static void Test(string originalGame,string testRoot){
        Directory.CreateDirectory(testRoot);string game=Path.Combine(testRoot,"game");Directory.CreateDirectory(game);File.Copy(Path.Combine(originalGame,"The Slormancer.exe"),Path.Combine(game,"The Slormancer.exe"));File.WriteAllText(Path.Combine(game,"data.win"),"test");string initial=Hash(Path.Combine(game,"The Slormancer.exe"));
        // Use a complete staged package as the installer source; do not launch the test game.
        foreach(var item in Read<Dictionary<string,string>>(Path.Combine(packageRoot,"slorm-loader-checksums.json")))Copy(Safe(packageRoot,item.Key),Safe(game,item.Key));
        Install(game);if(Patched(Path.Combine(game,"The Slormancer.exe"))||Hash(Path.Combine(game,"The Slormancer.exe"))!=initial)throw new Exception("Test: proxy changed the executable.");var receipt=Read<Dictionary<string,string>>(Path.Combine(game,".slorm-loader","state","bootstrap.json"));if(receipt["version"]!=PackageVersion()||receipt["mode"]!="version-proxy"||!Path.IsPathRooted(receipt["corePath"]))throw new Exception("Test: wrong bootstrap receipt.");Install(game);if(Hash(Path.Combine(game,"The Slormancer.exe"))!=initial)throw new Exception("Test: reinstall changed executable.");Check(game);Restore(game);Restore(game);if(Hash(Path.Combine(game,"The Slormancer.exe"))!=initial)throw new Exception("Test: restoration changed executable.");Console.WriteLine("PASS: proxy install/check/repeat/restore leave the executable byte-identical. No game was launched.");
        string reportPath=Path.Combine(game,".slorm-loader","mod-status.json");
        Write(reportPath,new{mods=new[]{new{id="full-plate-armor",name="Full Plate Armor",version="1.0.0",status="applied",reason=""},new{id="dependency-test",name="Dependency Test",version="1.0.0",status="blocked",reason="Missing dependency: example"}}});
        string summary=SuccessMessage("check",game);if(!summary.Contains("Full Plate Armor 1.0.0 [JSON]: applied")||!summary.Contains("Missing dependency: example")||!summary.Contains("Last game session"))throw new Exception("Test: status dialog lacks mod names, version, reason or freshness label.");Console.WriteLine("PASS: status summary names, versions, dependency failure and previous-session label; no dialog shown.");
        Write(reportPath,new{schemaVersion=2,active=1,errors=1,pending=0,nativeScanState="complete",mods=new[]{new{id="native:loaded",name="Loaded.dll",version="",source="native",path="mods/Aurie/Loaded.dll",status="loaded",reason=""},new{id="native:rejected",name="Rejected.dll",version="",source="native",path="mods/Aurie/nested/Rejected.dll",status="rejected",reason="Not an Aurie 2 module"}}});
        summary=SuccessMessage("check",game);if(!summary.Contains("1 loaded / 1 errors")||!summary.Contains("[DLL]")||!summary.Contains("mods/Aurie/nested/Rejected.dll")||!summary.Contains("Not an Aurie 2 module"))throw new Exception("Test: unified native status summary is incomplete.");Console.WriteLine("PASS: native DLL summary retains total counts, type, relative path and rejection reason.");
        Write(reportPath,new{active=0,errors=0,pending=1,nativeScanState="pending",mods=new[]{new{id="native:waiting",name="Waiting.dll",version="",source="native",status="pending",reason="Waiting for Aurie startup"}}});
        summary=SuccessMessage("check",game);if(!summary.Contains("Native scan pending")||!summary.Contains("1 pending"))throw new Exception("Test: pending native scan mistaken for a completed report.");Console.WriteLine("PASS: incomplete startup evidence is clearly labeled pending.");
        File.WriteAllText(reportPath,"{broken");if(!SuccessMessage("check",game).Contains("could not be read"))throw new Exception("Test: malformed status report fallback.");Console.WriteLine("PASS: malformed previous status report has a readable fallback.");
    }
    static void Record(string game,string message){try{string file=Safe(game,".slorm-loader\\setup.log");Directory.CreateDirectory(Path.GetDirectoryName(file));File.AppendAllText(file,DateTimeOffset.Now.ToString("o")+" "+message+Environment.NewLine);}catch{}}
    static string LastModStatus(string game){
        string file=Safe(game,".slorm-loader\\mod-status.json");
        if(!File.Exists(file))return "\r\n\r\nNo mod status is available yet. Launch the game after deploying mods.";
        try{
            Dictionary<string,object> report;using(var stream=new FileStream(file,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))using(var reader=new StreamReader(stream))report=Json.Deserialize<Dictionary<string,object>>(reader.ReadToEnd());var list=report["mods"] as System.Collections.IEnumerable;var message=new StringBuilder("\r\n\r\nLast game session's mods (launch again after deployment changes):");int count=0;
            if(report.ContainsKey("active")&&report.ContainsKey("errors")){message.Append("\r\nTotal: ").Append(report["active"]).Append(" loaded / ").Append(report["errors"]).Append(" errors");if(report.ContainsKey("pending"))message.Append(" / ").Append(report["pending"]).Append(" pending");}
            if(report.ContainsKey("nativeScanState")&&Convert.ToString(report["nativeScanState"])!="complete")message.Append("\r\nNative scan ").Append(report["nativeScanState"]).Append("; native DLL status is not fully verified.");
            foreach(var item in list){var mod=item as Dictionary<string,object>;if(mod==null)continue;if(++count>10){message.Append("\r\nMore mods are listed in the status report.");break;}string source=mod.ContainsKey("source")?Convert.ToString(mod["source"]):"json";bool native=source=="native";message.Append("\r\n").Append(mod["name"]);if(!String.IsNullOrEmpty(Convert.ToString(mod["version"])))message.Append(" ").Append(mod["version"]);message.Append(native?" [DLL]":source=="loader"?" [loader]":" [JSON]").Append(": ").Append(mod["status"]);if(native&&mod.ContainsKey("path"))message.Append("\r\n  ").Append(mod["path"]);if(mod.ContainsKey("reason")&&!String.IsNullOrEmpty(Convert.ToString(mod["reason"])))message.Append(" — ").Append(mod["reason"]);}
            if(count==0)message.Append("\r\nNo gameplay mods were recorded in that session.");message.Append("\r\n\r\nStatus report:\r\n").Append(file);return message.ToString();
        }catch{return "\r\n\r\nThe previous mod-status report could not be read. See runtime logs.";}
    }
    static string SuccessMessage(string command,string game){
        string message=command=="install"?"The Slormancer version-proxy loader is configured. The game executable is unchanged.\r\n\r\nLaunch through Steam or Vortex. Enabled mods take effect on the next launch.":command=="restore"?"Loader setup has been removed.\r\n\r\nVortex removes the proxy and Core during disable/purge. Any owned legacy executable patch was restored.":"The loader package and version-proxy configuration passed verification. The game executable is unchanged.\r\n\r\nThis checks the installation; launch the game to check runtime mod loading.";
        return message+"\r\n\r\nGame folder:\r\n"+game+"\r\n\r\nSetup log:\r\n"+Path.Combine(game,".slorm-loader","setup.log")+(command=="check"?LastModStatus(game)+"\r\n\r\nRuntime logs:\r\n"+Path.Combine(game,".slorm-loader","logs"):"");
    }
    static void Notify(string message,bool error){MessageBox.Show(message,"Slormancer loader",MessageBoxButtons.OK,error?MessageBoxIcon.Error:MessageBoxIcon.Information);}
    [STAThread]
    public static int Main(string[] args){
        bool quiet=args.Contains("--quiet");args=args.Where(a=>a!="--quiet").ToArray();string game=packageRoot,command=args.Length>0?args[0]:"install";bool notify=!quiet&&args.Length<=1&&command!="self-test";
        try{game=args.Length>1?Path.GetFullPath(args[1]):packageRoot;if(command=="install"&&args.Length<=2)Install(game);else if(command=="restore"&&args.Length<=2)Restore(game);else if(command=="check"&&args.Length<=2)Check(game);else if(command=="self-test"&&args.Length==3)Test(game,Path.GetFullPath(args[2]));else throw new Exception("Usage: SlormLoaderSetup.exe install|check|restore [game-folder] [--quiet]");Record(game,"SUCCESS "+command);if(notify)Notify(SuccessMessage(command,game),false);return 0;}
        catch(Exception error){Console.Error.WriteLine("ERROR: "+error.Message);Record(game,"ERROR "+error.Message);if(notify)Notify(error.Message+"\r\n\r\nSetup log:\r\n"+Path.Combine(game,".slorm-loader","setup.log"),true);return 1;}
    }
}
