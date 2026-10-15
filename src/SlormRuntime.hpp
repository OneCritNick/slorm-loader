#pragma once
#include <filesystem>
#include <cstdio>
#include <windows.h>
#include <string>

inline bool SlormReadCurrentAurieLog(const std::filesystem::path& game,std::string& text,std::string& reason) {
    text.clear();reason.clear();const auto path=game/L"aurie.log";
    const HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){reason="Waiting for readable aurie.log (Windows error "+std::to_string(GetLastError())+").";return false;}
    struct Close {HANDLE file;~Close(){CloseHandle(file);}} close{file};
    FILETIME started{},exited{},kernel{},user{},written{};LARGE_INTEGER size{};
    if(!GetProcessTimes(GetCurrentProcess(),&started,&exited,&kernel,&user)||!GetFileTime(file,nullptr,nullptr,&written)||CompareFileTime(&written,&started)<0){reason="Waiting for aurie.log from this game process; an older log is not startup confirmation.";return false;}
    if(!GetFileSizeEx(file,&size)||size.QuadPart<0||size.QuadPart>8*1024*1024){reason="Cannot confirm startup: aurie.log is unavailable or exceeds 8 MiB.";return false;}
    text.resize(static_cast<size_t>(size.QuadPart));DWORD read=0;
    if(!ReadFile(file,text.data(),static_cast<DWORD>(text.size()),&read,nullptr)){text.clear();reason="Waiting for readable aurie.log contents.";return false;}
    text.resize(read);return true;
}

inline std::filesystem::path SlormRuntimeLogPath() {
    wchar_t overridden[32768]{};
    DWORD length = GetEnvironmentVariableW(L"SLORM_MOD_LOG", overridden, 32768);
    if (length && length < 32768) return std::filesystem::path(overridden);
    wchar_t image[32768]{};
    length = GetModuleFileNameW(nullptr, image, 32768);
    if (!length || length >= 32768) return {};
    auto folder = std::filesystem::path(image).parent_path() / L".slorm-loader" / L"logs";
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    if (error) return {};
    FILETIME creation{}, exit{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user);
    SYSTEMTIME started{};
    FileTimeToSystemTime(&creation, &started);
    wchar_t filename[128]{};
    swprintf_s(filename, L"runtime-%04u%02u%02u-%02u%02u%02u-UTC-%lu.log",
        started.wYear, started.wMonth, started.wDay, started.wHour, started.wMinute,
        started.wSecond, GetCurrentProcessId());
    return folder / filename;
}
